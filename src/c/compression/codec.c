/** @file codec.c Original PNG and DEFLATE codecs. @author kai */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// SHARED CONTRACT
#include "codec.h"

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

// BIT STREAMS
typedef struct {
    const unsigned char *p;
    size_t n, at;
    unsigned long bits;
    unsigned count;
    int bad;
} Bits;
typedef struct {
    unsigned count[16], symbols[320];
} Huff;
typedef struct {
    RcBuffer *out;
    unsigned long bits;
    unsigned count;
    int bad;
} Writer;

// CANONICAL TABLES
static const unsigned lbase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const unsigned char lextra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                         2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned dbase[30] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                   33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                   1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const unsigned char dextra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/*//////////////////////////////////////////////////
BUFFERS
//////////////////////////////////////////////////*/

// STORAGE
/** Grow a bounded buffer and append bytes without losing its current allocation. @author kai */
int rcAppend(RcBuffer *b, const void *p, size_t n) {
    size_t cap;
    unsigned char *q;
    if (n > RC_LIMIT || b->size > RC_LIMIT - n)
        return 0;
    if (b->size + n > b->capacity) {
        cap = b->capacity ? b->capacity : 256;
        while (cap < b->size + n)
            cap = cap > RC_LIMIT / 2 ? RC_LIMIT : cap * 2;
        q = realloc(b->data, cap);
        if (!q)
            return 0;
        b->data = q;
        b->capacity = cap;
    }
    if (n)
        memcpy(b->data + b->size, p, n);
    b->size += n;
    return 1;
}

// CHECKSUMS
/** Calculate PNG-compatible CRC-32 over the supplied bytes. @author kai */
unsigned long rcCrc(const unsigned char *p, size_t n) {
    unsigned long c = 0xffffffffUL;
    size_t i;
    int k;
    for (i = 0; i < n; i++) {
        c ^= p[i];
        for (k = 0; k < 8; k++)
            c = (c >> 1) ^ ((c & 1) ? 0xedb88320UL : 0);
    }
    return c ^ 0xffffffffUL;
}
/** Calculate the zlib payload checksum. @author kai */
static unsigned long adler(const unsigned char *p, size_t n) {
    unsigned long a = 1, b = 0;
    size_t i;
    for (i = 0; i < n; i++) {
        a = (a + p[i]) % 65521UL;
        b = (b + a) % 65521UL;
    }
    return (b << 16) | a;
}
/** Read a big-endian 32-bit field. @author kai */
static unsigned long be32(const unsigned char *p) {
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) |
           p[3];
}

/*//////////////////////////////////////////////////
DEFLATE
//////////////////////////////////////////////////*/

// DECODING
/** Consume low-order DEFLATE bits and flag truncated input. @author kai */
static unsigned readBits(Bits *b, unsigned n) {
    unsigned v;
    while (b->count < n) {
        if (b->at == b->n) {
            b->bad = 1;
            return 0;
        }
        b->bits |= (unsigned long)b->p[b->at++] << b->count;
        b->count += 8;
    }
    v = (unsigned)(b->bits & ((1UL << n) - 1));
    b->bits >>= n;
    b->count -= n;
    return v;
}
/** Build canonical symbol offsets and reject oversubscribed code lengths. @author kai */
static int makeHuff(Huff *h, const unsigned char *lengths, unsigned n) {
    unsigned offsets[16] = {0}, i;
    int left = 1;
    memset(h, 0, sizeof(*h));
    for (i = 0; i < n; i++) {
        if (lengths[i] > 15)
            return 0;
        h->count[lengths[i]]++;
    }
    for (i = 1; i <= 15; i++) {
        left = left * 2 - (int)h->count[i];
        if (left < 0)
            return 0;
    }
    if (h->count[0] == n)
        return 0;
    for (i = 1; i < 15; i++)
        offsets[i + 1] = offsets[i] + h->count[i];
    for (i = 0; i < n; i++)
        if (lengths[i])
            h->symbols[offsets[lengths[i]]++] = i;
    return 1;
}
/** Read one canonical DEFLATE symbol without speculative input reads. @author kai */
static int decodeHuff(Bits *b, const Huff *h) {
    unsigned code = 0, first = 0, index = 0, n;
    for (n = 1; n <= 15; n++) {
        code |= readBits(b, 1);
        if (b->bad)
            return -1;
        if (code >= first && code - first < h->count[n])
            return (int)h->symbols[index + code - first];
        index += h->count[n];
        first = (first + h->count[n]) << 1;
        code <<= 1;
    }
    b->bad = 1;
    return -1;
}
/** Decode and validate one zlib stream.
 * @param out Empty owned buffer; callers release it even when decoding fails.
 * @return 1 only when payload and Adler checksum agree.
 * @author kai
 */
int rcInflate(const unsigned char *p, size_t n, RcBuffer *out) {
    Bits b = {0};
    int final = 0;

    // VALIDATE THE ZLIB ENVELOPE
    // Reject unsupported windows and preset dictionaries before reading entropy bits.
    if (n < 6 || (p[0] & 15) != 8 || (p[0] >> 4) > 7 || (((unsigned)p[0] << 8) + p[1]) % 31 ||
        (p[1] & 32))
        return 0;
    b.p = p + 2;
    b.n = n - 6;

    // Decode block alphabets before copying literals or bounded history matches.
    while (!final && !b.bad) {
        unsigned type;
        Huff lit, dist;
        unsigned char lens[320] = {0}, dl[32] = {0};
        unsigned i;

        // READ THE BLOCK HEADER
        final = (int)readBits(&b, 1);
        type = readBits(&b, 2);
        if (type == 0) {
            // COPY STORED BYTES
            // Discard bit padding and verify the stored length against its complement.
            unsigned len, inv;
            b.bits = 0;
            b.count = 0;
            len = readBits(&b, 16);
            inv = readBits(&b, 16);
            if (b.bad || (len ^ inv) != 65535 || len > b.n - b.at ||
                !rcAppend(out, b.p + b.at, len))
                return 0;
            b.at += len;
            continue;
        }
        if (type == 1) {
            // BUILD FIXED ALPHABETS
            for (i = 0; i < 288; i++)
                lens[i] = (unsigned char)(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
            for (i = 0; i < 32; i++)
                dl[i] = 5;
            if (!makeHuff(&lit, lens, 288) || !makeHuff(&dist, dl, 32))
                return 0;
        } else if (type == 2) {
            // READ DYNAMIC ALPHABETS
            // The code-length alphabet describes both the literal/length and distance trees.
            static const unsigned proper[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                11, 4,  12, 3, 13, 2, 14, 1, 15};
            unsigned nl = readBits(&b, 5) + 257, nd = readBits(&b, 5) + 1, nc = readBits(&b, 4) + 4,
                     at = 0;
            unsigned char cl[19] = {0};
            Huff ch;
            if (nl > 286 || nd > 32)
                return 0;
            for (i = 0; i < nc; i++)
                cl[proper[i]] = (unsigned char)readBits(&b, 3);
            if (!makeHuff(&ch, cl, 19))
                return 0;

            // EXPAND REPEATED CODE LENGTHS
            // Repeat symbols must stay within the two declared alphabet lengths.
            while (at < nl + nd && !b.bad) {
                int s = decodeHuff(&b, &ch);
                unsigned run, value;
                if (s < 0)
                    return 0;
                if (s < 16) {
                    lens[at++] = (unsigned char)s;
                    continue;
                }
                if (s == 16) {
                    if (!at)
                        return 0;
                    run = readBits(&b, 2) + 3;
                    value = lens[at - 1];
                } else if (s == 17) {
                    run = readBits(&b, 3) + 3;
                    value = 0;
                } else if (s == 18) {
                    run = readBits(&b, 7) + 11;
                    value = 0;
                } else
                    return 0;
                if (run > nl + nd - at)
                    return 0;
                while (run--)
                    lens[at++] = (unsigned char)value;
            }
            if (!lens[256] || !makeHuff(&lit, lens, nl))
                return 0;
            memcpy(dl, lens + nl, nd);
            /* A distance alphabet is unnecessary for literal-only blocks. */
            if (!makeHuff(&dist, dl, nd))
                memset(&dist, 0, sizeof(dist));
        } else
            return 0;

        // DECODE LITERALS AND MATCHES
        for (;;) {
            int s = decodeHuff(&b, &lit);
            unsigned len, distance, k;
            unsigned char byte;
            if (s < 0)
                return 0;
            if (s == 256)
                break;
            if (s < 256) {
                byte = (unsigned char)s;
                if (!rcAppend(out, &byte, 1))
                    return 0;
                continue;
            }
            if (s > 285)
                return 0;
            k = (unsigned)s - 257;
            len = lbase[k] + readBits(&b, lextra[k]);
            s = decodeHuff(&b, &dist);
            if (s < 0 || s >= 30)
                return 0;
            distance = dbase[s] + readBits(&b, dextra[s]);
            if (b.bad || distance > out->size || len > RC_LIMIT - out->size)
                return 0;

            // COPY THE HISTORY MATCH
            // Read from the growing output so overlapping LZ77 matches expand correctly.
            while (len--) {
                byte = out->data[out->size - distance];
                if (!rcAppend(out, &byte, 1))
                    return 0;
            }
        }
    }

    // VERIFY THE COMPLETE STREAM
    // Check input consumption and the Adler checksum before reporting success.
    return !b.bad && b.at == b.n && adler(out->data, out->size) == be32(p + n - 4);
}

// ENCODING
/** Flush low-order encoded bits into the bounded output buffer. @author kai */
static void putBits(Writer *w, unsigned value, unsigned n) {
    w->bits |= (unsigned long)value << w->count;
    w->count += n;
    while (w->count >= 8) {
        unsigned char c = (unsigned char)w->bits;
        if (!rcAppend(w->out, &c, 1))
            w->bad = 1;
        w->bits >>= 8;
        w->count -= 8;
    }
}
/** Reverse a canonical code for DEFLATE transmission order. @author kai */
static unsigned reverseBits(unsigned v, unsigned n) {
    unsigned r = 0;
    while (n--) {
        r = (r << 1) | (v & 1);
        v >>= 1;
    }
    return r;
}
/** Emit one symbol from the fixed DEFLATE alphabet. @author kai */
static void fixedSymbol(Writer *w, unsigned s) {
    unsigned code, n;
    if (s < 144) {
        code = 48 + s;
        n = 8;
    } else if (s < 256) {
        code = 400 + s - 144;
        n = 9;
    } else if (s < 280) {
        code = s - 256;
        n = 7;
    } else {
        code = 192 + s - 280;
        n = 8;
    }
    putBits(w, reverseBits(code, n), n);
}
/** Encode zlib data using fixed codes and a bounded LZ77 history search. @author kai */
int rcDeflate(const unsigned char *p, size_t n, RcBuffer *out) {
    size_t *head = malloc(65536 * sizeof(*head)), *prev;
    size_t at = 0, i;
    Writer w = {out, 0, 0, 0};
    unsigned char header[2] = {0x78, 0x01}, tail[4];
    unsigned long sum;

    // PREPARE THE HISTORY WINDOW
    // Hash heads and history links share one allocation and one lifetime.
    if (!head)
        return 0;
    prev = head + 32768;
    for (i = 0; i < 32768; i++)
        head[i] = prev[i] = (size_t)-1;
    if (!rcAppend(out, header, 2)) {
        free(head);
        return 0;
    }
    putBits(&w, 3, 3);

    // Bound hash-chain searches so repetitive inputs cannot cause unbounded work.
    while (at < n && !w.bad) {
        unsigned best = 0, dist = 0, hash = 0;
        size_t candidate = (size_t)-1;
        unsigned attempts = 0, step = 1, j;
        if (at + 2 < n) {
            hash = ((unsigned)p[at] * 251U + (unsigned)p[at + 1] * 31U + p[at + 2]) & 32767U;
            candidate = head[hash];
        }

        // SEARCH PREVIOUS MATCHES
        // The chain is bounded by both the 32 KiB window and a fixed attempt count.
        while (candidate != (size_t)-1 && candidate < at && at - candidate <= 32768 &&
               attempts++ < 64) {
            unsigned match = 0;
            while (match < 258 && at + match < n && p[candidate + match] == p[at + match])
                match++;
            if (match >= 3 && match > best) {
                best = match;
                dist = (unsigned)(at - candidate);
                if (match == 258)
                    break;
            }
            {
                size_t next = prev[candidate & 32767];
                if (next >= candidate)
                    break;
                candidate = next;
            }
        }

        // EMIT THE SELECTED REPRESENTATION
        // A match emits length/distance codes; otherwise emit one literal byte.
        if (best >= 3) {
            unsigned k = 0;
            while (k < 28 && best >= lbase[k + 1])
                k++;
            fixedSymbol(&w, k + 257);
            putBits(&w, best - lbase[k], lextra[k]);
            k = 0;
            while (k < 29 && dist >= dbase[k + 1])
                k++;
            putBits(&w, reverseBits(k, 5), 5);
            putBits(&w, dist - dbase[k], dextra[k]);
            step = best;
        } else
            fixedSymbol(&w, p[at]);

        // INDEX THE CONSUMED POSITIONS
        for (j = 0; j < step; j++) {
            size_t pos = at + j;
            if (pos + 2 < n) {
                hash = ((unsigned)p[pos] * 251U + (unsigned)p[pos + 1] * 31U + p[pos + 2]) & 32767U;
                prev[pos & 32767] = head[hash];
                head[hash] = pos;
            }
        }
        at += step;
    }

    // CLOSE THE ZLIB STREAM
    // Finish the block, pad its last byte, and append the checksum of the original bytes.
    fixedSymbol(&w, 256);
    if (w.count)
        putBits(&w, 0, 8 - w.count);
    sum = adler(p, n);
    for (i = 0; i < 4; i++)
        tail[i] = (unsigned char)(sum >> (24 - i * 8));
    free(head);
    return !w.bad && rcAppend(out, tail, 4);
}

/*//////////////////////////////////////////////////
PNG
//////////////////////////////////////////////////*/

// FILTERS
/** Choose the nearest PNG predictor neighbour. @author kai */
static int paeth(int a, int b, int c) {
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}
/** Read one packed PNG sample at its declared bit depth. @author kai */
unsigned rcSample(const unsigned char *row, size_t sample, unsigned depth) {
    if (depth == 16)
        return ((unsigned)row[sample * 2] << 8) | row[sample * 2 + 1];
    if (depth == 8)
        return row[sample];
    return (row[sample * depth / 8] >> (8 - depth - (sample * depth % 8))) & ((1U << depth) - 1);
}

/** Reverse a PNG byte filter in place; NULL prior denotes the first row.
 * @return 0 for an unknown filter; row and prior must each cover length bytes.
 * @author kai
 */
int rcUnfilter(unsigned char *row, const unsigned char *prior, size_t length, size_t bpp,
               unsigned filter) {
    size_t x;
    if (filter > 4)
        return 0;
    // RECONSTRUCT LEFT TO RIGHT
    // Left neighbours already contain decoded bytes, including for packed colour samples.
    for (x = 0; x < length; x++) {
        int a = x >= bpp ? row[x - bpp] : 0, b = prior ? prior[x] : 0,
            c = prior && x >= bpp ? prior[x - bpp] : 0;
        int prediction = filter == 0   ? 0
                         : filter == 1 ? a
                         : filter == 2 ? b
                         : filter == 3 ? (a + b) / 2
                                       : paeth(a, b, c);
        row[x] = (unsigned char)(row[x] + prediction);
    }
    return 1;
}

// IMAGE DECODING
/** Validate PNG chunks and reconstruct RGBA pixels across Adam7 passes. @author kai */
static int pngDecode(const unsigned char *p, size_t n, RcImage *im) {
    unsigned w = 0, h = 0, depth = 0, type = 0, interlace = 0, channels = 0, paletteCount = 0;
    unsigned char palette[256][4];
    unsigned transparency[3] = {65536, 65536, 65536};
    RcBuffer packed = {0}, raw = {0};
    size_t at = 8, cursor = 0;
    int header = 0, ended = 0, haveData = 0, closedData = 0, ok = 0;
    unsigned pass, passes;
    static const unsigned xs[7] = {0, 4, 0, 2, 0, 1, 0}, ys[7] = {0, 0, 4, 0, 2, 0, 1},
                          dx[7] = {8, 8, 4, 4, 2, 2, 1}, dy[7] = {8, 8, 8, 4, 4, 2, 2};
    memset(palette, 255, sizeof(palette));

    // Validate chunk order and CRCs before interpreting image samples.
    while (at + 12 <= n) {
        // VALIDATE THE NEXT CHUNK
        // CRC validation covers the chunk type and payload, before either is interpreted.
        unsigned long length = be32(p + at);
        const unsigned char *name = p + at + 4, *body = p + at + 8;
        if (length > n - at - 12 || rcCrc(name, (size_t)length + 4) != be32(body + length))
            goto done;
        if (!header && memcmp(name, "IHDR", 4))
            goto done;
        if (!memcmp(name, "IHDR", 4)) {
            // READ IMAGE GEOMETRY
            // Colour type controls channel count; bit depth controls packed sample extraction.
            if (header || length != 13)
                goto done;
            header = 1;
            w = (unsigned)be32(body);
            h = (unsigned)be32(body + 4);
            depth = body[8];
            type = body[9];
            interlace = body[12];
            if (!w || !h || (size_t)w > RC_PIXELS / h || body[10] || body[11] || interlace > 1)
                goto done;
            // CHECK THE COLOUR-TYPE CONTRACT
            // Zero entries denote reserved colour types; low depths require gray or indexed data.
            {
                static const unsigned char channelCount[7] = {1, 0, 3, 1, 2, 0, 4};
                if (type > 6 || !(channels = channelCount[type]) ||
                    !(depth == 8 || (depth == 16 && type != 3) ||
                      ((type == 0 || type == 3) && (depth == 1 || depth == 2 || depth == 4))))
                    goto done;
            }
        } else if (!memcmp(name, "PLTE", 4)) {
            unsigned i;
            if (haveData || !length || length % 3 || length > 768)
                goto done;

            // LOAD PALETTE ENTRIES
            // PNG palettes store RGB triples; transparency is applied by the later tRNS chunk.
            paletteCount = (unsigned)length / 3;
            for (i = 0; i < paletteCount; i++)
                memcpy(palette[i], body + i * 3, 3);
        } else if (!memcmp(name, "tRNS", 4)) {
            unsigned i;
            if (haveData)
                goto done;

            // READ TRANSPARENCY METADATA
            // Palette alpha and raw sample transparency keys follow different layouts.
            if (type == 3) {
                if (!paletteCount || length > paletteCount)
                    goto done;
                for (i = 0; i < length; i++)
                    palette[i][3] = body[i];
            } else if (type == 0 && length == 2)
                transparency[0] = ((unsigned)body[0] << 8) | body[1];
            else if (type == 2 && length == 6)
                for (i = 0; i < 3; i++)
                    transparency[i] = ((unsigned)body[i * 2] << 8) | body[i * 2 + 1];
            else
                goto done;
        } else if (!memcmp(name, "IDAT", 4)) {
            // COLLECT COMPRESSED PIXELS
            // IDAT chunks form one zlib stream and must remain contiguous.
            if (closedData || !rcAppend(&packed, body, length))
                goto done;
            haveData = 1;
        } else if (!memcmp(name, "acTL", 4)) {
            goto done;
        } else if (!memcmp(name, "IEND", 4)) {
            if (length || !haveData)
                goto done;
            ended = 1;
            at += 12;
            break;
        } else {
            if (haveData)
                closedData = 1;
            if (!(name[0] & 32))
                goto done;
        }
        at += (size_t)length + 12;
    }

    // ALLOCATE THE DECODED IMAGE
    // Only inflate after the full chunk envelope and terminal marker have been checked.
    if (!ended || at != n || !rcInflate(packed.data, packed.size, &raw) ||
        (type == 3 && !paletteCount))
        goto done;
    im->rgba = calloc((size_t)w * h, 4);
    if (!im->rgba)
        goto done;
    im->width = w;
    im->height = h;
    passes = interlace ? 7 : 1;

    // Each Adam7 pass has an independent previous-row filter context.
    for (pass = 0; pass < passes; pass++) {
        // PREPARE ONE INTERLACE PASS
        // Each pass writes its smaller raster into strided positions of the final image.
        unsigned x0 = interlace ? xs[pass] : 0, y0 = interlace ? ys[pass] : 0,
                 sx = interlace ? dx[pass] : 1, sy = interlace ? dy[pass] : 1;
        unsigned pw = w > x0 ? (w - x0 + sx - 1) / sx : 0, ph = h > y0 ? (h - y0 + sy - 1) / sy : 0,
                 y, x;
        size_t rowBytes = ((size_t)pw * channels * depth + 7) / 8, bpp = (channels * depth + 7) / 8;
        unsigned char *prior = NULL, *row = NULL;
        if (!pw || !ph)
            continue;
        if (rowBytes + 1 > raw.size || ph > (raw.size - cursor) / (rowBytes + 1))
            goto done;
        for (y = 0; y < ph; y++) {
            // RECONSTRUCT ONE ROW
            // Reuse the inflated storage; earlier rows remain available as predictor context.
            unsigned filter = raw.data[cursor++];
            row = raw.data + cursor;
            cursor += rowBytes;
            if (!rcUnfilter(row, prior, rowBytes, bpp, filter))
                goto done;

            // EXPAND SAMPLES TO RGBA
            // Retain alpha and compare transparency keys before reducing source precision.
            for (x = 0; x < pw; x++) {
                unsigned samples[4] = {0, 0, 0, 255}, j;
                unsigned char *pixel = im->rgba + ((size_t)(y0 + y * sy) * w + x0 + x * sx) * 4;
                for (j = 0; j < channels; j++)
                    samples[j] = rcSample(row, (size_t)x * channels + j, depth);
                if (type == 3) {
                    if (samples[0] >= paletteCount)
                        goto done;
                    memcpy(pixel, palette[samples[0]], 4);
                } else {
                    unsigned max = (1U << depth) - 1;
                    pixel[3] = 255;
                    if (type == 0 || type == 4) {
                        pixel[0] = pixel[1] = pixel[2] = (unsigned char)(samples[0] * 255U / max);
                        if (type == 4)
                            pixel[3] = (unsigned char)(samples[1] * 255U / max);
                        else if (samples[0] == transparency[0])
                            pixel[3] = 0;
                    } else {
                        for (j = 0; j < 3; j++)
                            pixel[j] = (unsigned char)(samples[j] * 255U / max);
                        if (type == 6)
                            pixel[3] = (unsigned char)(samples[3] * 255U / max);
                        else if (samples[0] == transparency[0] && samples[1] == transparency[1] &&
                                 samples[2] == transparency[2])
                            pixel[3] = 0;
                    }
                }
            }
            prior = row;
        }
    }

    // VERIFY AND RELEASE DECODER STATE
    if (cursor != raw.size)
        goto done;
    ok = 1;
done:
    free(packed.data);
    free(raw.data);
    if (!ok) {
        free(im->rgba);
        memset(im, 0, sizeof(*im));
    }
    return ok;
}

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

// FORMAT DISPATCH
/** Detect PNG/JPEG contents and decode straight-alpha RGBA8 pixels.
 * @param im Receives owned pixels; source precision is preserved by lossless file copying.
 * @return 0 for malformed, unsupported, or over-limit images.
 * @author kai
 */
int rcDecode(const unsigned char *p, size_t n, RcImage *im) {
    memset(im, 0, sizeof(*im));
    if (n >= 8 && !memcmp(p, "\211PNG\r\n\032\n", 8))
        return pngDecode(p, n, im);
    if (n >= 2 && p[0] == 255 && p[1] == 216)
        return rcJpeg(p, n, im, 1);
    return 0;
}
