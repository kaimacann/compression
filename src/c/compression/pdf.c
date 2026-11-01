/** @file pdf.c Classic-PDF object processing, with immutable-source fallback. @author kai */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// SHARED CONTRACT
#include "media.h"

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

// PROCESSING STATE
typedef struct {
    const unsigned char *p;
    size_t n;
} Slice;
typedef struct {
    size_t offset, stream, length;
    Slice value; // The first value bounds dictionary lookups, excluding any binary stream.
    const unsigned char *end; // Original object bytes continue up to the endobj token.
    unsigned generation;
    int live, mask;
    RcBuffer replacement;
} PdfObject;
typedef struct {
    const unsigned char *p;
    size_t n;
    PdfObject *objects;
    unsigned count;
    Slice trailer;
    int unsupported;
} Pdf;

/*//////////////////////////////////////////////////
PARSING
//////////////////////////////////////////////////*/

// TOKENS
/** Recognise PDF whitespace, including NUL. @author kai */
static int white(unsigned char c) {
    return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32;
}
/** Identify PDF token boundaries without matching inside names. @author kai */
static int delimiter(unsigned char c) { return white(c) || strchr("()<>[]{}/%", c) != NULL; }
/** Skip PDF whitespace and comments. @author kai */
static size_t space(const unsigned char *p, size_t n, size_t at) {
    for (;;) {
        while (at < n && white(p[at]))
            at++;
        if (at < n && p[at] == '%') {
            while (at < n && p[at] != '\r' && p[at] != '\n')
                at++;
        } else
            return at;
    }
}
/** Match a bounded PDF keyword at a token boundary. @author kai */
static int token(const unsigned char *p, size_t n, size_t at, const char *word) {
    size_t len = strlen(word);
    return at <= n && len <= n - at && !memcmp(p + at, word, len) &&
           (at + len == n || delimiter(p[at + len]));
}
/** Read a nonnegative PDF integer with overflow checking. @author kai */
static int uintAt(const unsigned char *p, size_t n, size_t *at, size_t *v) {
    size_t k = space(p, n, *at), value = 0, start = k;
    while (k < n && p[k] >= '0' && p[k] <= '9') {
        unsigned d = p[k++] - '0';
        if (value > ((size_t)-1 - d) / 10)
            return 0;
        value = value * 10 + d;
    }
    if (k == start || (k < n && !delimiter(p[k])))
        return 0;
    *at = k;
    *v = value;
    return 1;
}
/** Skip one bounded PDF value, including nested containers and escaped strings. @author kai */
static size_t valueEnd(const unsigned char *p, size_t n, size_t at, unsigned depth) {
    size_t k = space(p, n, at), start = k;
    if (depth > 32 || k == n)
        return n + 1;

    // SKIP LITERAL STRINGS
    // Escapes and nested parentheses belong to the string rather than the surrounding PDF syntax.
    if (p[k] == '(') {
        unsigned nesting = 1;
        k++;
        while (k < n && nesting) {
            if (p[k] == '\\') {
                k++;
                if (k < n) {
                    if (p[k] == '\r' && k + 1 < n && p[k + 1] == '\n')
                        k++;
                    k++;
                }
            } else {
                if (p[k] == '(')
                    nesting++;
                if (p[k] == ')')
                    nesting--;
                k++;
            }
        }
        return nesting ? n + 1 : k;
    }

    // SKIP HEXADECIMAL STRINGS
    if (p[k] == '<' && k + 1 < n && p[k + 1] != '<') {
        k++;
        while (k < n && p[k] != '>')
            k++;
        return k < n ? k + 1 : n + 1;
    }

    // SKIP NESTED CONTAINERS
    // Recurse through arrays and dictionaries while enforcing the shared nesting limit.
    if (p[k] == '[' || (p[k] == '<' && k + 1 < n && p[k + 1] == '<')) {
        int dict = p[k] == '<';
        k += dict ? 2 : 1;
        for (;;) {
            k = space(p, n, k);
            if (k >= n)
                return n + 1;
            if (!dict && p[k] == ']')
                return k + 1;
            if (dict && k + 1 < n && p[k] == '>' && p[k + 1] == '>')
                return k + 2;
            k = valueEnd(p, n, k, depth + 1);
            if (k > n)
                return k;
        }
    }

    // READ A SCALAR OR INDIRECT REFERENCE
    // An integer pair followed by R is one value, so callers must advance past all three tokens.
    if (p[k] == '/')
        k++;
    else if (strchr("]>)", p[k]))
        return n + 1;
    while (k < n && !delimiter(p[k]))
        k++;
    if (k == start)
        return n + 1;
    if (p[start] >= '0' && p[start] <= '9') {
        size_t next = k, generation;
        if (uintAt(p, n, &next, &generation)) {
            next = space(p, n, next);
            if (token(p, n, next, "R")) {
                return next + 1;
            }
        }
    }
    return k;
}
/** Find a top-level dictionary key without searching nested values or streams. @author kai */
static int dictValue(Slice dictionary, const char *key, Slice *result) {
    const unsigned char *p = dictionary.p;
    size_t end = dictionary.n, at = space(p, end, 0), keyLength = strlen(key);
    if (at + 2 > end || memcmp(p + at, "<<", 2))
        return 0;
    at += 2;
    while ((at = space(p, end, at)) < end) {
        size_t start = at, keyEnd, value, finish;
        if (at + 1 < end && p[at] == '>' && p[at + 1] == '>')
            return 0;
        if (p[at] != '/')
            return -1;
        keyEnd = valueEnd(p, end, at, 0);
        if (keyEnd > end)
            return -1;
        value = space(p, end, keyEnd);
        finish = valueEnd(p, end, value, 0);
        if (finish > end)
            return -1;
        if (keyEnd - start == keyLength && !memcmp(p + start, key, keyLength)) {
            result->p = p + value;
            result->n = finish - value;
            return 1;
        }
        at = finish;
    }
    return -1;
}
/** Compare an entire PDF name or literal token. @author kai */
static int nameIs(Slice s, const char *v) { return s.n == strlen(v) && !memcmp(s.p, v, s.n); }

// REFERENCES
/** Resolve an xref entry to its first validated object value. @author kai */
static int objectValue(Pdf *pdf, unsigned id, Slice *value) {
    size_t at, num, gen;
    if (id >= pdf->count || !pdf->objects[id].live)
        return 0;
    at = pdf->objects[id].offset;
    if (!uintAt(pdf->p, pdf->n, &at, &num) || !uintAt(pdf->p, pdf->n, &at, &gen) || num != id ||
        gen != pdf->objects[id].generation)
        return 0;
    at = space(pdf->p, pdf->n, at);
    if (!token(pdf->p, pdf->n, at, "obj"))
        return 0;
    at = space(pdf->p, pdf->n, at + 3);
    {
        size_t end = valueEnd(pdf->p, pdf->n, at, 0);
        if (end > pdf->n)
            return 0;
        value->p = pdf->p + at;
        value->n = end - at;
    }
    return 1;
}
/** Decode an indirect reference and bound its object and generation numbers. @author kai */
static int reference(Slice s, unsigned *id, unsigned *gen) {
    size_t at = 0, a, b;
    if (!uintAt(s.p, s.n, &at, &a) || !uintAt(s.p, s.n, &at, &b) || a > 100000 || b > 65535)
        return 0;
    at = space(s.p, s.n, at);
    if (!token(s.p, s.n, at, "R"))
        return 0;
    *id = (unsigned)a;
    *gen = (unsigned)b;
    return 1;
}
/** Follow a bounded indirect-reference chain. @author kai */
static int resolve(Pdf *pdf, Slice *s) {
    unsigned id, gen;
    unsigned depth;
    for (depth = 0; depth < 8; depth++) {
        if (!reference(*s, &id, &gen))
            return 1;
        if (id >= pdf->count || pdf->objects[id].generation != gen || !objectValue(pdf, id, s))
            return 0;
    }
    return 0;
}
/** Resolve a PDF size field to a complete nonnegative integer. @author kai */
static int sizeValue(Pdf *pdf, Slice s, size_t *v) {
    size_t at = 0;
    return resolve(pdf, &s) && uintAt(s.p, s.n, &at, v) && space(s.p, s.n, at) == s.n;
}
/** Read a direct or indirect integer dictionary field. @author kai */
static int getSize(Pdf *pdf, PdfObject *o, const char *key, size_t *v) {
    Slice s;
    return dictValue(o->value, key, &s) == 1 && sizeValue(pdf, s, v);
}
/** Read an existing object reference with a matching generation. @author kai */
static int getRef(Pdf *pdf, PdfObject *o, const char *key, unsigned *id) {
    Slice s;
    unsigned gen;
    if (dictValue(o->value, key, &s) != 1 || !reference(s, id, &gen) || *id >= pdf->count ||
        !pdf->objects[*id].live || pdf->objects[*id].generation != gen)
        return 0;
    return 1;
}
// CROSS-REFERENCE VALIDATION

// CROSS REFERENCES
/** Validate classic xrefs without searching binary stream contents.
 * Unsupported structures request source preservation instead of speculative rewriting.
 * @author kai
 */
static int parsePdf(Pdf *pdf) {
    size_t at, xref = (size_t)-1, i, trailer = 0, trailerEnd;
    Slice s;
    size_t count;
    int found = 0;

    // VALIDATE THE FILE ENVELOPE
    if (pdf->n < 12 || memcmp(pdf->p, "%PDF-", 5))
        return 0;
    at = pdf->n;
    while (at && white(pdf->p[at - 1]))
        at--;
    if (at < 5 || memcmp(pdf->p + at - 5, "%%EOF", 5))
        return 0;

    // LOCATE THE ACTIVE CROSS-REFERENCE TABLE
    // Use the final startxref offset instead of matching object-like bytes inside image streams.
    for (i = at - 5; i > 0; i--)
        if (token(pdf->p, pdf->n, i, "startxref")) {
            size_t pos = i + 9;
            if (uintAt(pdf->p, pdf->n, &pos, &xref)) {
                found = 1;
                break;
            }
        }
    if (!found || xref >= pdf->n)
        return 0;
    at = xref;
    if (!token(pdf->p, pdf->n, at, "xref")) {
        pdf->unsupported = 1;
        return 1;
    }
    at += 4;
    /* Find the trailer by traversing the actual xref sections, not binary streams. */
    {
        // FIND THE TRAILER THROUGH XREF SECTIONS
        size_t scan = at;
        for (;;) {
            size_t first, n, j, a, b;
            scan = space(pdf->p, pdf->n, scan);
            if (token(pdf->p, pdf->n, scan, "trailer")) {
                trailer = space(pdf->p, pdf->n, scan + 7);
                break;
            }
            if (!uintAt(pdf->p, pdf->n, &scan, &first) || !uintAt(pdf->p, pdf->n, &scan, &n) ||
                first > 100000 || n > 100000 - first)
                return 0;
            for (j = 0; j < n; j++) {
                if (!uintAt(pdf->p, pdf->n, &scan, &a) || !uintAt(pdf->p, pdf->n, &scan, &b))
                    return 0;
                scan = space(pdf->p, pdf->n, scan);
                if (scan >= pdf->n || (pdf->p[scan] != 'n' && pdf->p[scan] != 'f'))
                    return 0;
                scan++;
            }
        }
    }

    // VALIDATE TRAILER CAPABILITIES
    // Incremental, hybrid, or encrypted documents use unchanged-source fallback.
    trailerEnd = valueEnd(pdf->p, pdf->n, trailer, 0);
    if (trailerEnd > pdf->n)
        return 0;
    pdf->trailer = (Slice){pdf->p + trailer, trailerEnd - trailer};
    if (dictValue(pdf->trailer, "/Size", &s) != 1 || !sizeValue(pdf, s, &count) || !count ||
        count > 100000)
        return 0;
    pdf->count = (unsigned)count;
    if (dictValue(pdf->trailer, "/Prev", &s) == 1 || dictValue(pdf->trailer, "/XRefStm", &s) == 1 ||
        dictValue(pdf->trailer, "/Encrypt", &s) == 1) {
        pdf->unsupported = 1;
        return 1;
    }

    // INDEX DECLARED OBJECT LOCATIONS
    pdf->objects = calloc(count, sizeof(*pdf->objects));
    if (!pdf->objects)
        return 0;
    while ((at = space(pdf->p, pdf->n, at)) < trailer) {
        size_t first, n, j;
        if (token(pdf->p, pdf->n, at, "trailer"))
            break;
        if (!uintAt(pdf->p, pdf->n, &at, &first) || !uintAt(pdf->p, pdf->n, &at, &n) ||
            first >= count || n > count - first)
            return 0;
        for (j = 0; j < n; j++) {
            size_t offset, gen;
            PdfObject *o = &pdf->objects[first + j];
            if (!uintAt(pdf->p, pdf->n, &at, &offset) || !uintAt(pdf->p, pdf->n, &at, &gen) ||
                gen > 65535)
                return 0;
            at = space(pdf->p, pdf->n, at);
            o->live = pdf->p[at] == 'n';
            o->offset = offset;
            o->generation = (unsigned)gen;
            at++;
            if (o->live && offset >= xref)
                return 0;
        }
    }

    // VALIDATE LIVE OBJECT BOUNDARIES
    for (i = 1; i < count; i++)
        if (pdf->objects[i].live) {
            PdfObject *o = &pdf->objects[i];
            size_t pos;
            if (!objectValue(pdf, (unsigned)i, &o->value))
                return 0;
            pos = space(pdf->p, pdf->n, (size_t)(o->value.p - pdf->p) + o->value.n);

            // BOUND BINARY STREAMS BY THEIR LENGTH
            // Resolve /Length before seeking terminators; encoded pixels may contain PDF-like
            // words.
            if (token(pdf->p, pdf->n, pos, "stream")) {
                pos += 6;
                if (pos >= pdf->n)
                    return 0;
                if (pdf->p[pos] == '\r') {
                    pos++;
                    if (pos < pdf->n && pdf->p[pos] == '\n')
                        pos++;
                } else if (pdf->p[pos] == '\n')
                    pos++;
                else
                    return 0;
                o->stream = pos;
                if (!getSize(pdf, o, "/Length", &o->length) || o->length > pdf->n - pos)
                    return 0;
                pos = space(pdf->p, pdf->n, pos + o->length);
                if (!token(pdf->p, pdf->n, pos, "endstream"))
                    return 0;
                pos = space(pdf->p, pdf->n, pos + 9);
            }
            if (!token(pdf->p, pdf->n, pos, "endobj"))
                return 0;
            o->end = pdf->p + pos;

            // PRESERVE SIGNED AND PACKED STRUCTURES
            if (dictValue(o->value, "/ByteRange", &s) == 1) {
                pdf->unsupported = 1;
                return 1;
            }
            if (dictValue(o->value, "/Type", &s) == 1 &&
                (nameIs(s, "/ObjStm") || nameIs(s, "/XRef"))) {
                pdf->unsupported = 1;
                return 1;
            }
        }
    return 1;
}

/*//////////////////////////////////////////////////
IMAGES
//////////////////////////////////////////////////*/

// FILTERS
/** Decode PDF ASCII85 with tuple and terminator validation. @author kai */
static int ascii85(Slice s, RcBuffer *out) {
    unsigned long value = 0;
    unsigned count = 0;
    size_t i;
    int ended = 0;

    // DECODE COMPLETE TUPLES
    // The z shorthand is valid only between tuples; five base-85 digits produce four bytes.
    for (i = 0; i < s.n; i++) {
        unsigned c = s.p[i];
        if (white((unsigned char)c))
            continue;
        if (c == '~') {
            if (i + 1 >= s.n || s.p[++i] != '>')
                return 0;
            ended = 1;
            i++;
            break;
        }
        if (c == 'z') {
            unsigned char zeros[4] = {0};
            if (count || !rcAppend(out, zeros, 4))
                return 0;
            continue;
        }
        if (c < 33 || c > 117)
            return 0;
        value = value * 85 + c - 33;
        count++;
        if (count == 5) {
            unsigned char bytes[4];
            unsigned k;
            if (value > 0xffffffffUL)
                return 0;
            for (k = 0; k < 4; k++)
                bytes[k] = (unsigned char)(value >> (24 - k * 8));
            if (!rcAppend(out, bytes, 4))
                return 0;
            count = 0;
            value = 0;
        }
    }

    // VALIDATE THE TERMINATOR
    if (!ended || count == 1)
        return 0;
    while (i < s.n)
        if (!white(s.p[i++]))
            return 0;

    // DECODE THE FINAL PARTIAL TUPLE
    // Pad with the largest digit and emit one fewer byte than the original digit count.
    if (count) {
        unsigned char bytes[4];
        unsigned k, original = count;
        while (count++ < 5)
            value = value * 85 + 84;
        if (value > 0xffffffffUL)
            return 0;
        for (k = 0; k < 4; k++)
            bytes[k] = (unsigned char)(value >> (24 - k * 8));
        if (!rcAppend(out, bytes, original - 1))
            return 0;
    }
    return 1;
}
/** Convert an ASCII hexadecimal digit, returning -1 for invalid input. @author kai */
static int hexDigit(unsigned c) {
    if (c >= '0' && c <= '9')
        return (int)c - '0';
    if (c >= 'A' && c <= 'F')
        return (int)c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return (int)c - 'a' + 10;
    return -1;
}
/** Decode a terminated PDF hexadecimal byte sequence. @author kai */
static int asciiHex(Slice s, RcBuffer *out) {
    int prior = -1;
    size_t i;
    for (i = 0; i < s.n; i++) {
        int v;
        if (white(s.p[i]))
            continue;
        if (s.p[i] == '>')
            break;
        v = hexDigit(s.p[i]);
        if (v < 0)
            return 0;
        if (prior < 0)
            prior = v;
        else {
            unsigned char c = (unsigned char)(prior * 16 + v);
            if (!rcAppend(out, &c, 1))
                return 0;
            prior = -1;
        }
    }
    if (i == s.n)
        return 0;
    if (prior >= 0) {
        unsigned char c = (unsigned char)(prior * 16);
        if (!rcAppend(out, &c, 1))
            return 0;
    }
    return 1;
}
/** Decode an ordered filter chain while retaining its JPEG boundary.
 * @return 1 on success, 0 for unsupported filters, -1 for corrupt supported encodings.
 * @author kai
 */
static int decodeFilters(Pdf *pdf, PdfObject *o, RcBuffer *out, int *jpeg) {
    Slice filter, items[8];
    unsigned count = 0, i;
    RcBuffer current = {0};
    int ok = 0;
    *jpeg = 0;

    // RESOLVE THE ORDERED FILTER LIST
    if (dictValue(o->value, "/Filter", &filter) == 1) {
        size_t at = 0;
        if (!resolve(pdf, &filter))
            return 0;
        if (filter.n && filter.p[0] == '[')
            at = 1;
        while ((at = space(filter.p, filter.n, at)) < filter.n && filter.p[at] != ']') {
            size_t end = valueEnd(filter.p, filter.n, at, 0);
            if (end > filter.n || count == 8)
                return 0;
            items[count].p = filter.p + at;
            items[count++].n = end - at;
            at = end;
        }
    }
    if (!rcAppend(&current, pdf->p + o->stream, o->length))
        goto done;

    // DECODE FILTERS IN DECLARED ORDER
    // DCT remains encoded until the shared JPEG decoder receives its byte stream.
    for (i = 0; i < count; i++) {
        RcBuffer next = {0};
        Slice bytes = {current.data, current.size};
        int supported = 0, known = 1;
        if (nameIs(items[i], "/FlateDecode") || nameIs(items[i], "/Fl"))
            supported = rcInflate(current.data, current.size, &next);
        else if (nameIs(items[i], "/ASCII85Decode") || nameIs(items[i], "/A85"))
            supported = ascii85(bytes, &next);
        else if (nameIs(items[i], "/ASCIIHexDecode") || nameIs(items[i], "/AHx"))
            supported = asciiHex(bytes, &next);
        else if ((nameIs(items[i], "/DCTDecode") || nameIs(items[i], "/DCT")) && i + 1 == count) {
            *jpeg = 1;
            break;
        } else
            known = 0;

        // DISTINGUISH UNSUPPORTED DATA FROM CORRUPTION
        if (!supported) {
            free(next.data);
            ok = known ? -1 : 0;
            goto done;
        }
        free(current.data);
        current = next;
    }
    *out = current;
    memset(&current, 0, sizeof(current));
    ok = 1;
done:
    free(current.data);
    return ok;
}

// SAMPLE LAYOUT
/** Reverse TIFF or PNG predictors while checking row dimensions. @author kai */
static int predictor(Pdf *pdf, PdfObject *o, RcBuffer *data, unsigned w, unsigned h,
                     unsigned channels, unsigned bits) {
    Slice parms, s;
    size_t pred = 1, columns = w, colors = channels, bpc = bits;
    size_t row = ((size_t)w * channels * bits + 7) / 8, bpp = (channels * bits + 7) / 8, y, x;

    // RESOLVE PREDICTOR SETTINGS
    if (dictValue(o->value, "/DecodeParms", &parms) != 1)
        return 1;
    if (!resolve(pdf, &parms))
        return 0;
    if (parms.n && parms.p[0] == '[') {
        size_t at = 1;
        int found = 0;
        while ((at = space(parms.p, parms.n, at)) < parms.n && parms.p[at] != ']') {
            size_t end = valueEnd(parms.p, parms.n, at, 0);
            if (end > parms.n)
                return 0;
            if (parms.p[at] == '<') {
                Slice chosen = {parms.p + at, end - at};
                parms = chosen;
                found = 1;
                break;
            }
            at = end;
        }
        if (!found)
            return 1;
    }
    if (parms.n < 2 || parms.p[0] != '<')
        return nameIs(parms, "null");
    if (dictValue(parms, "/Predictor", &s) == 1 && !sizeValue(pdf, s, &pred))
        return 0;

    // RESOLVE THE ACTIVE SAMPLE LAYOUT
    // An omitted predictor means the decoded stream already contains raw rows.
    if (pred == 1)
        return 1;
    if (dictValue(parms, "/Columns", &s) == 1 && !sizeValue(pdf, s, &columns))
        return 0;
    if (dictValue(parms, "/Colors", &s) == 1 && !sizeValue(pdf, s, &colors))
        return 0;
    if (dictValue(parms, "/BitsPerComponent", &s) == 1 && !sizeValue(pdf, s, &bpc))
        return 0;

    // VALIDATE ROW GEOMETRY
    if (columns != w || colors != channels || bpc != bits)
        return 0;

    // REVERSE TIFF SAMPLE DIFFERENCES
    // Work in sample units, including packed and sixteen-bit components.
    if (pred == 2) {
        if (data->size != row * h)
            return 0;
        for (y = 0; y < h; y++)
            for (x = channels; x < (size_t)w * channels; x++) {
                unsigned char *r = data->data + y * row;
                unsigned value =
                    (rcSample(r, x, bits) + rcSample(r, x - channels, bits)) & ((1U << bits) - 1);
                if (bits == 8)
                    r[x] = (unsigned char)value;
                else if (bits == 16) {
                    r[x * 2] = (unsigned char)(value >> 8);
                    r[x * 2 + 1] = (unsigned char)value;
                } else {
                    size_t index = x * bits / 8;
                    unsigned shift = 8 - bits - x * bits % 8, mask = ((1U << bits) - 1) << shift;
                    r[index] = (unsigned char)((r[index] & ~mask) | (value << shift));
                }
            }
        return 1;
    }

    // REVERSE PNG BYTE FILTERS
    // Each encoded row begins with a filter byte before its pixel bytes.
    if (pred < 10 || pred > 15 || data->size != (row + 1) * h)
        return 0;
    for (y = 0; y < h; y++) {
        unsigned filter = data->data[y * (row + 1)];
        unsigned char *decoded = data->data + y * row;
        // Compact away the filter byte; earlier decoded rows cannot overlap unread input.
        memmove(decoded, data->data + y * (row + 1) + 1, row);
        if (!rcUnfilter(decoded, y ? decoded - row : NULL, row, bpp, filter))
            return 0;
    }
    data->size = row * h;
    return 1;
}

// IMAGE OBJECTS
/** Decode an image object and its supported colour/transparency metadata.
 * @return 1 with owned pixels, 0 for unsupported objects, -1 for corrupt payloads.
 * @author kai
 */
static int pdfImage(Pdf *pdf, unsigned id, RcImage *im, unsigned depth) {
    PdfObject *o = &pdf->objects[id];
    Slice color, s, decode = {0}, mask = {0};
    size_t w, h, bits = 8;
    unsigned channels = 0, indexed = 0, high = 0, maskId, gen;
    unsigned char lookup[768];
    RcBuffer data = {0};
    int jpeg = 0, ok = 0;
    double ranges[3][2] = {{0, 1}, {0, 1}, {0, 1}};
    size_t x, y, row;
    unsigned i;

    // VALIDATE IMAGE CAPABILITIES
    // Unsupported matte, mask, or colour metadata must preserve the original object.
    memset(im, 0, sizeof(*im));
    if (dictValue(o->value, "/Matte", &s) == 1)
        return 0;
    if (dictValue(o->value, "/SMask", &s) == 1 && !nameIs(s, "/None") &&
        !getRef(pdf, o, "/SMask", &maskId))
        return 0;
    if (depth > 2 || !o->stream || !getSize(pdf, o, "/Width", &w) ||
        !getSize(pdf, o, "/Height", &h) || !w || !h || w > RC_PIXELS / h)
        return 0;
    if (dictValue(o->value, "/ImageMask", &s) == 1 && !nameIs(s, "false"))
        return 0;
    if (dictValue(o->value, "/ColorSpace", &color) != 1 || !resolve(pdf, &color))
        return 0;

    // RESOLVE THE COLOUR SPACE
    if (nameIs(color, "/DeviceRGB"))
        channels = 3;
    else if (nameIs(color, "/DeviceGray"))
        channels = 1;
    else if (color.n > 2 && color.p[0] == '[') {
        size_t at = space(color.p, color.n, 1), end;
        Slice entry;
        end = valueEnd(color.p, color.n, at, 0);
        if (end > color.n)
            return 0;
        entry = (Slice){color.p + at, end - at};
        if (!nameIs(entry, "/Indexed"))
            return 0;
        at = space(color.p, color.n, end);
        end = valueEnd(color.p, color.n, at, 0);
        if (end > color.n)
            return 0;
        entry = (Slice){color.p + at, end - at};
        if (!nameIs(entry, "/DeviceRGB"))
            return 0;

        // READ THE INDEXED PALETTE LIMIT
        // The maximum index bounds both pixel values and the required lookup byte count.
        at = space(color.p, color.n, end);
        {
            size_t value;
            if (!uintAt(color.p, color.n, &at, &value) || value > 255)
                return 0;
            high = (unsigned)value;
        }
        at = space(color.p, color.n, at);
        end = valueEnd(color.p, color.n, at, 0);
        if (end > color.n)
            return 0;
        entry = (Slice){color.p + at, end - at};

        // LOAD AN INDEXED COLOUR LOOKUP
        // A palette can be a referenced stream or an inline hexadecimal byte sequence.
        {
            RcBuffer lut = {0};
            int decoded = 0, jpegLookup = 0;
            if (reference(entry, &maskId, &gen))
                decoded = maskId < pdf->count &&
                          decodeFilters(pdf, &pdf->objects[maskId], &lut, &jpegLookup) == 1 &&
                          !jpegLookup;
            else if (entry.n >= 2 && entry.p[0] == '<' && entry.p[1] != '<') {
                Slice bytes = {entry.p + 1, entry.n - 1};
                decoded = asciiHex(bytes, &lut);
            }
            if (!decoded || lut.size != (high + 1) * 3) {
                free(lut.data);
                return 0;
            }
            memcpy(lookup, lut.data, lut.size);
            free(lut.data);
        }
        channels = 1;
        indexed = 1;
    } else
        return 0;
    {
        // DECODE THE IMAGE PAYLOAD
        int status = decodeFilters(pdf, o, &data, &jpeg);
        if (status <= 0) {
            ok = status;
            goto done;
        }
    }

    // DECODE JPEG OR RECONSTRUCT RAW SAMPLES
    if (jpeg) {
        if (dictValue(o->value, "/Decode", &s) == 1)
            goto done;
        if (indexed)
            goto done;
        if (!rcJpeg(data.data, data.size, im, 0) || im->width != w || im->height != h) {
            ok = -1;
            goto done;
        }
    } else {
        if (!getSize(pdf, o, "/BitsPerComponent", &bits) ||
            (bits != 1 && bits != 2 && bits != 4 && bits != 8 && bits != 16) ||
            !predictor(pdf, o, &data, (unsigned)w, (unsigned)h, channels, (unsigned)bits))
            goto done;
        row = (w * channels * bits + 7) / 8;
        if (data.size != row * h)
            goto done;
        im->rgba = malloc(w * h * 4);
        if (!im->rgba)
            goto done;
        im->width = (unsigned)w;
        im->height = (unsigned)h;

        // READ RAW-SAMPLE DECODE RANGES
        if (dictValue(o->value, "/Decode", &decode) == 1) {
            size_t at = 1;
            if (!resolve(pdf, &decode) || decode.p[0] != '[' || indexed)
                goto done;
            for (i = 0; i < channels; i++) {
                unsigned endpoint;
                for (endpoint = 0; endpoint < 2; endpoint++) {
                    char *end;
                    at = space(decode.p, decode.n, at);
                    ranges[i][endpoint] = strtod((const char *)decode.p + at, &end);
                    if ((const unsigned char *)end == decode.p + at ||
                        (const unsigned char *)end > decode.p + decode.n ||
                        !isfinite(ranges[i][endpoint]))
                        goto done;
                    at = (size_t)((const unsigned char *)end - decode.p);
                }
            }
        }

        // EXPAND RASTER SAMPLES
        // Indexed samples address the lookup; direct samples use their declared decode ranges.
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                unsigned char *pixel = im->rgba + (y * w + x) * 4;
                const unsigned char *r = data.data + y * row;
                unsigned values[3] = {0};
                for (i = 0; i < channels; i++)
                    values[i] = rcSample(r, x * channels + i, (unsigned)bits);
                pixel[3] = 255;
                if (indexed) {
                    if (values[0] > high)
                        goto done;
                    memcpy(pixel, lookup + values[0] * 3, 3);
                } else {
                    for (i = 0; i < channels; i++) {
                        double value = (ranges[i][0] + values[i] * (ranges[i][1] - ranges[i][0]) /
                                                           ((1U << bits) - 1)) *
                                       255;
                        if (value < 0)
                            value = 0;
                        if (value > 255)
                            value = 255;
                        pixel[i] = (unsigned char)floor(value + 0.5);
                    }
                    if (channels == 1)
                        pixel[1] = pixel[2] = pixel[0];
                }
            }
    }

    // APPLY COLOUR-KEY TRANSPARENCY
    // Compare the original packed samples before their colour-range mapping.
    if (dictValue(o->value, "/Mask", &mask) == 1) {
        if (!resolve(pdf, &mask) || !mask.n || mask.p[0] != '[')
            goto done; /* Colour-key masking: compare original samples, before decode mapping. */
        {
            size_t at = 1, limits[6];
            if (jpeg)
                goto done;
            for (i = 0; i < channels * 2; i++)
                if (!uintAt(mask.p, mask.n, &at, &limits[i]))
                    goto done;
            row = (w * channels * bits + 7) / 8;
            for (y = 0; y < h; y++)
                for (x = 0; x < w; x++) {
                    int transparent = 1;
                    for (i = 0; i < channels; i++) {
                        unsigned value =
                            rcSample(data.data + y * row, x * channels + i, (unsigned)bits);
                        if (value < limits[i * 2] || value > limits[i * 2 + 1])
                            transparent = 0;
                    }
                    if (transparent)
                        im->rgba[(y * w + x) * 4 + 3] = 0;
                }
        }
    }

    // APPLY THE SOFT MASK
    // Decode the mask recursively, then copy its grayscale coverage into straight alpha.
    if (getRef(pdf, o, "/SMask", &maskId)) {
        RcImage alpha = {0};
        if (!pdfImage(pdf, maskId, &alpha, depth + 1) || alpha.width != w || alpha.height != h) {
            free(alpha.rgba);
            goto done;
        }
        for (x = 0; x < w * h; x++)
            im->rgba[x * 4 + 3] = alpha.rgba[x * 4];
        free(alpha.rgba);
    }
    ok = 1;
done:
    // RELEASE IMAGE WORKSPACE
    free(data.data);
    if (ok <= 0) {
        free(im->rgba);
        memset(im, 0, sizeof(*im));
    }
    return ok;
}

/*//////////////////////////////////////////////////
OUTPUT
//////////////////////////////////////////////////*/

// OBJECT SERIALIZATION
/** Rewrite live objects and their xref offsets while retaining the original trailer. @author kai */
static int serialize(Pdf *pdf, RcBuffer *out) {
    unsigned i;
    char text[128];
    int n;
    size_t *offsets = calloc(pdf->count, sizeof(*offsets));
    size_t xref;
    int ok = 0;
    if (!offsets)
        return 0;
    if (!rcAppend(out, "%PDF-1.4\n%\342\343\317\323\n", 15))
        goto done;

    // WRITE LIVE OBJECT BODIES
    // Replacement forms keep the original object number and generation.
    for (i = 1; i < pdf->count; i++)
        if (pdf->objects[i].live) {
            PdfObject *o = &pdf->objects[i];
            offsets[i] = out->size;
            n = snprintf(text, sizeof(text), "%u %u obj\n", i, o->generation);
            if (!rcAppend(out, text, (size_t)n))
                goto done;
            if (o->replacement.size) {
                if (!rcAppend(out, o->replacement.data, o->replacement.size))
                    goto done;
            } else if (!rcAppend(out, o->value.p, (size_t)(o->end - o->value.p)))
                goto done;
            if (!rcAppend(out, "\nendobj\n", 8))
                goto done;
        }

    // REBUILD OFFSETS AND RETAIN THE TRAILER
    xref = out->size;
    n = snprintf(text, sizeof(text), "xref\n0 %u\n0000000000 65535 f \n", pdf->count);
    if (!rcAppend(out, text, (size_t)n))
        goto done;
    for (i = 1; i < pdf->count; i++) {
        n = snprintf(text, sizeof(text), "%010lu %05u %c \n", (unsigned long)offsets[i],
                     pdf->objects[i].generation, pdf->objects[i].live ? 'n' : 'f');
        if (!rcAppend(out, text, (size_t)n))
            goto done;
    }
    if (!rcAppend(out, "trailer\n", 8) || !rcAppend(out, pdf->trailer.p, pdf->trailer.n))
        goto done;
    n = snprintf(text, sizeof(text), "\nstartxref\n%lu\n%%%%EOF\n", (unsigned long)xref);
    if (!rcAppend(out, text, (size_t)n))
        goto done;
    ok = 1;
done:
    free(offsets);
    return ok;
}

// PDF PIPELINE
/** Build one candidate and SVG sidecars using the original resource identities.
 * @param candidate Receives rebuilt bytes, or stays empty when the source should be retained.
 * @param images Receives owned buffers; masks belong to their image rather than separate exports.
 * @return 1 for a candidate or unchanged fallback, 0 for malformed input or allocation failure.
 * @author kai
 */
int processPdf(const unsigned char *data, size_t length, const MediaOptions *options,
               RcBuffer *candidate, RcBuffer **images, size_t *imageCount, unsigned *unsupported) {
    Pdf pdf = {0};
    RcImage image = {0};
    RcVector vector = {0};
    RcBuffer compressed = {0};
    unsigned i;
    int ok = 0;
    pdf.p = data;
    pdf.n = length;
    *images = NULL;
    *imageCount = 0;
    *unsupported = 0;

    // VALIDATE THE DOCUMENT AND MODE
    if (!parsePdf(&pdf))
        goto done;
    if (pdf.unsupported || !options->vectorMode) {
        *unsupported = (unsigned)pdf.unsupported;
        ok = 1;
        goto done;
    }

    // Soft masks belong to their owning images and must not become standalone forms.
    for (i = 1; i < pdf.count; i++)
        if (pdf.objects[i].live) {
            unsigned mask;
            if (getRef(&pdf, &pdf.objects[i], "/SMask", &mask) ||
                getRef(&pdf, &pdf.objects[i], "/Mask", &mask))
                pdf.objects[mask].mask = 1;
        }

    // Reuse each image object ID so existing page resource references remain valid.
    for (i = 1; i < pdf.count; i++) {
        PdfObject *o = &pdf.objects[i];
        Slice subtype;
        char text[256];
        int n;
        if (!o->live || o->mask || dictValue(o->value, "/Subtype", &subtype) != 1 ||
            !nameIs(subtype, "/Image"))
            continue;
        {
            // DECODE ONE IMAGE
            // Unsupported metadata preserves the object; corrupt supported payloads abort the
            // candidate.
            int decoded = pdfImage(&pdf, i, &image, 0);
            if (decoded < 0)
                goto done;
            if (!decoded) {
                (*unsupported)++;
                continue;
            }
        }
        if (!rcVectorize(&image, options->colors, options->tolerance, &vector))
            goto done;
        free(image.rgba);
        image.rgba = NULL;
        {
            // TRANSFER SVG OWNERSHIP
            // Once registered, the common cleanup path owns this sidecar even if the PDF later
            // fails.
            RcBuffer *more = realloc(*images, (*imageCount + 1) * sizeof(**images));
            if (!more)
                goto done;
            *images = more;
            (*images)[*imageCount] = vector.svg;
            memset(&vector.svg, 0, sizeof(vector.svg));
            (*imageCount)++;
        }

        // ENCODE THE REPLACEMENT FORM
        // Compress drawing commands separately from the SVG representation.
        if (!rcDeflate(vector.pdf.data, vector.pdf.size, &compressed))
            goto done;
        n = snprintf(text, sizeof(text),
                     "<< /Type /XObject /Subtype /Form /FormType 1 /BBox [0 0 1 1] /Resources << ");
        if (!rcAppend(&o->replacement, text, (size_t)n) ||
            !rcAppend(&o->replacement, vector.resources.data, vector.resources.size))
            goto done;
        n = snprintf(text, sizeof(text), ">> /Filter /FlateDecode /Length %lu >>\nstream\n",
                     (unsigned long)compressed.size);
        if (!rcAppend(&o->replacement, text, (size_t)n) ||
            !rcAppend(&o->replacement, compressed.data, compressed.size) ||
            !rcAppend(&o->replacement, "\nendstream\n", 11))
            goto done;
        free(compressed.data);
        memset(&compressed, 0, sizeof(compressed));
        rcFreeVector(&vector);
    }

    // SELECT AN UNCHANGED OR REBUILT CANDIDATE
    if (!*imageCount) {
        ok = 1;
        goto done;
    }

    // Validate regenerated object offsets before exposing the PDF candidate.
    ok = serialize(&pdf, candidate);

    // REPARSE THE GENERATED CROSS-REFERENCE TABLE
    if (ok) {
        Pdf check = {0};
        check.p = candidate->data;
        check.n = candidate->size;
        ok = parsePdf(&check) && !check.unsupported;
        free(check.objects);
    }
done:
    // RELEASE THE CURRENT IMAGE AND DOCUMENT
    // Partial form construction uses the same cleanup path as a complete image.
    free(image.rgba);
    free(compressed.data);
    rcFreeVector(&vector);
    if (pdf.objects) {
        for (i = 0; i < pdf.count; i++)
            free(pdf.objects[i].replacement.data);
        free(pdf.objects);
    }
    if (!ok) {
        for (i = 0; i < *imageCount; i++)
            free((*images)[i].data);
        free(*images);
        *images = NULL;
        *imageCount = 0;
    }
    return ok;
}
/** @author kai */
