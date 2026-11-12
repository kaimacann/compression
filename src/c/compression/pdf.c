/**
 * @file pdf.c Classic PDF image processing with immutable-source fallback.
 * @author kai
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* SHARED CONTRACT*/
#include "codec.h"
#include "media.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

enum {
    PDF_WHITESPACE_TAB = 9,
    PDF_WHITESPACE_LINE_FEED = 10,
    PDF_WHITESPACE_FORM_FEED = 12,
    PDF_WHITESPACE_CARRIAGE_RETURN = 13,
    PDF_WHITESPACE_SPACE = 32,
    PDF_DECIMAL_RADIX = 10,
    PDF_MAX_NESTING_DEPTH = 32,
    PDF_MAX_OBJECT_COUNT = 100000,
    PDF_MAX_GENERATION = 65535,
    PDF_MAX_REFERENCE_DEPTH = 8,
    PDF_MIN_HEADER_LENGTH = 12,
    PDF_VERSION_MARKER_LENGTH = 5,
    PDF_EOF_MARKER_LENGTH = 5,
    PDF_STARTXREF_KEYWORD_LENGTH = 9,
    PDF_TRAILER_KEYWORD_LENGTH = 7,
    PDF_STREAM_KEYWORD_LENGTH = 6,
    PDF_ENDSTREAM_KEYWORD_LENGTH = 9,
    PDF_MAX_FILTER_COUNT = 8,
    PDF_ASCII85_MINIMUM = 33,
    PDF_ASCII85_MAXIMUM = 117,
    PDF_ASCII85_RADIX = 85,
    PDF_ASCII85_TUPLE_DIGITS = 5,
    PDF_ASCII85_PADDING_DIGIT = 84,
    PDF_BITS_PER_BYTE = 8,
    PDF_HEX_RADIX = 16,
    PDF_SIXTEEN_BITS = 16,
    PNG_PREDICTOR_MINIMUM = 10,
    PNG_PREDICTOR_MAXIMUM = 15,
    PDF_SAMPLE_MAXIMUM = 255,
    PDF_RGB_LOOKUP_CAPACITY = 768,
    PDF_COLOR_KEY_RANGE_COUNT = 6,
    PDF_SERIALIZE_TEXT_CAPACITY = 128,
    PDF_REPLACEMENT_TEXT_CAPACITY = 256,
    PDF_XREF_OFFSET_WIDTH = 10,
    PDF_XREF_GENERATION_WIDTH = 5,
    PDF_HEADER_BYTES_LENGTH = 15,
    PDF_END_OBJECT_TEXT_LENGTH = 8,
    PDF_TRAILER_TEXT_LENGTH = 8,
    PDF_ENDSTREAM_TEXT_LENGTH = 11
};

#define PDF_SAMPLE_ROUNDING_BIAS 0.5

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

/* PROCESSING STATE*/
typedef struct {
    const unsigned char *sourceBytes;
    size_t sourceLength;
} Slice;
typedef struct {
    size_t offset, stream, length;
    Slice value; /* The first value bounds dictionary lookups, excluding any binary stream.*/
    const unsigned char *end; /* Original object bytes continue up to the endobj token.*/
    unsigned generation;
    int live, mask;
    RcBuffer replacement;
} PdfObject;
typedef struct {
    const unsigned char *sourceBytes;
    size_t sourceLength;
    PdfObject *objects;
    unsigned count;
    Slice trailer;
    int unsupported;
} Pdf;

/*//////////////////////////////////////////////////
PARSING
//////////////////////////////////////////////////*/

/* TOKENS*/
/**
 * Recognize PDF whitespace bytes.
 * @param byteValue Byte tested for PDF whitespace.
 * @return 1 for a PDF whitespace byte; otherwise 0.
 * @author kai
 */
static int white(unsigned char byteValue) {
    return byteValue == 0 || byteValue == PDF_WHITESPACE_TAB ||
           byteValue == PDF_WHITESPACE_LINE_FEED || byteValue == PDF_WHITESPACE_FORM_FEED ||
           byteValue == PDF_WHITESPACE_CARRIAGE_RETURN || byteValue == PDF_WHITESPACE_SPACE;
}
/**
 * Recognize PDF token delimiters, including whitespace.
 * @param byteValue Byte tested for PDF token delimiting.
 * @return 1 for a delimiter byte; otherwise 0.
 * @author kai
 */
static int delimiter(unsigned char byteValue) {
    return white(byteValue) || strchr("()<>[]{}/%", byteValue) != NULL;
}

/**
 * Copy a bounded byte range after the caller has validated both buffers.
 * @param destination Storage receiving copied bytes.
 * @param sourceBytes Valid source storage.
 * @param length Number of bytes to copy.
 * @author kai
 */
static void copyPdfBytes(unsigned char *destination, const unsigned char *sourceBytes,
                         size_t length) {
    size_t itemIndex;
    for (itemIndex = 0; itemIndex < length; itemIndex++) {
        destination[itemIndex] = sourceBytes[itemIndex];
    }
}

/**
 * Move a bounded byte range while preserving overlapping source bytes.
 * @param destination Storage receiving moved bytes.
 * @param sourceBytes Valid source storage.
 * @param length Number of bytes to move.
 * @author kai
 */
static void movePdfBytes(unsigned char *destination, const unsigned char *sourceBytes,
                         size_t length) {
    size_t itemIndex;
    uintptr_t destinationAddress = (uintptr_t)destination;
    uintptr_t sourceAddress = (uintptr_t)sourceBytes;
    if (destinationAddress < sourceAddress) {
        for (itemIndex = 0; itemIndex < length; itemIndex++) {
            destination[itemIndex] = sourceBytes[itemIndex];
        }
    } else if (destinationAddress > sourceAddress) {
        for (itemIndex = length; itemIndex > 0; itemIndex--) {
            destination[itemIndex - 1] = sourceBytes[itemIndex - 1];
        }
    }
}
/**
 * Skip PDF whitespace and comments, returning the next token offset.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Offset from which whitespace and comments are skipped.
 * @return Offset of the next token, or n when only whitespace remains.
 * @author kai
 */
static size_t space(const unsigned char *sourceBytes, size_t sourceLength, size_t cursor) {
    for (;;) {
        while (cursor < sourceLength && white(sourceBytes[cursor])) {
            cursor++;
        }
        if (cursor < sourceLength && sourceBytes[cursor] == '%') {
            while (cursor < sourceLength && sourceBytes[cursor] != '\r' &&
                   sourceBytes[cursor] != '\n') {
                cursor++;
            }
        } else {
            return cursor;
        }
    }
}
/**
 * Match a complete PDF keyword at an offset without matching a longer name.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Offset where the keyword must begin.
 * @param word Keyword that must end at a delimiter.
 * @return 1 for an exact delimited keyword match; otherwise 0.
 * @author kai
 */
static int token(const unsigned char *sourceBytes, size_t sourceLength, size_t cursor,
                 const char *word) {
    size_t len = strlen(word);
    return cursor <= sourceLength && len <= sourceLength - cursor &&
           !memcmp(sourceBytes + cursor, word, len) &&
           (cursor + len == sourceLength || delimiter(sourceBytes[cursor + len]));
}
/**
 * Parse an unsigned decimal integer and advance the input cursor.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Input cursor; advanced past the integer on success.
 * @param parsedValue Receives the parsed nonnegative integer.
 * @return 1 when a bounded integer is parsed; otherwise 0.
 * @author kai
 */
static int uintAt(const unsigned char *sourceBytes, size_t sourceLength, size_t *cursor,
                  size_t *parsedValue) {
    size_t loopIndex = space(sourceBytes, sourceLength, *cursor);
    size_t value = 0;
    size_t start = loopIndex;
    while (loopIndex < sourceLength && sourceBytes[loopIndex] >= '0' &&
           sourceBytes[loopIndex] <= '9') {
        unsigned decimalDigit = sourceBytes[loopIndex++] - '0';
        if (value > ((size_t)-1 - decimalDigit) / PDF_DECIMAL_RADIX) {
            return 0;
        }
        value = value * PDF_DECIMAL_RADIX + decimalDigit;
    }
    if (loopIndex == start || (loopIndex < sourceLength && !delimiter(sourceBytes[loopIndex]))) {
        return 0;
    }
    *cursor = loopIndex;
    *parsedValue = value;
    return 1;
}
/**
 * Find the end of one PDF value while bounding nested arrays and dictionaries.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Offset at the first byte of the value.
 * @param depth Recursion depth, bounded to prevent excessive nesting.
 * @return Offset after the value, or n + 1 for malformed or over-nested input.
 * @author kai
 */
static size_t valueEnd(const unsigned char *sourceBytes, size_t sourceLength, size_t cursor,
                       unsigned depth) {
    size_t loopIndex = space(sourceBytes, sourceLength, cursor);
    size_t start = loopIndex;
    if (depth > PDF_MAX_NESTING_DEPTH || loopIndex == sourceLength) {
        return sourceLength + 1;
    }

    /* SKIP LITERAL STRINGS*/
    /* Escapes and nested parentheses belong to the string rather than the surrounding PDF syntax.*/
    if (sourceBytes[loopIndex] == '(') {
        unsigned nesting = 1;
        loopIndex++;
        while (loopIndex < sourceLength && nesting) {
            if (sourceBytes[loopIndex] == '\\') {
                loopIndex++;
                if (loopIndex < sourceLength) {
                    if (sourceBytes[loopIndex] == '\r' && loopIndex + 1 < sourceLength &&
                        sourceBytes[loopIndex + 1] == '\n') {
                        loopIndex++;
                    }
                    loopIndex++;
                }
            } else {
                if (sourceBytes[loopIndex] == '(') {
                    nesting++;
                }
                if (sourceBytes[loopIndex] == ')') {
                    nesting--;
                }
                loopIndex++;
            }
        }
        return nesting ? sourceLength + 1 : loopIndex;
    }

    /* SKIP HEXADECIMAL STRINGS*/
    if (sourceBytes[loopIndex] == '<' && loopIndex + 1 < sourceLength &&
        sourceBytes[loopIndex + 1] != '<') {
        loopIndex++;
        while (loopIndex < sourceLength && sourceBytes[loopIndex] != '>') {
            loopIndex++;
        }
        return loopIndex < sourceLength ? loopIndex + 1 : sourceLength + 1;
    }

    /* SKIP NESTED CONTAINERS*/
    /* Recurse through arrays and dictionaries while enforcing the shared nesting limit.*/
    if (sourceBytes[loopIndex] == '[' ||
        (sourceBytes[loopIndex] == '<' && loopIndex + 1 < sourceLength &&
         sourceBytes[loopIndex + 1] == '<')) {
        int dict = sourceBytes[loopIndex] == '<';
        loopIndex += dict ? 2 : 1;
        for (;;) {
            loopIndex = space(sourceBytes, sourceLength, loopIndex);
            if (loopIndex >= sourceLength) {
                return sourceLength + 1;
            }
            if (!dict && sourceBytes[loopIndex] == ']') {
                return loopIndex + 1;
            }
            if (dict && loopIndex + 1 < sourceLength && sourceBytes[loopIndex] == '>' &&
                sourceBytes[loopIndex + 1] == '>') {
                return loopIndex + 2;
            }
            loopIndex = valueEnd(sourceBytes, sourceLength, loopIndex, depth + 1);
            if (loopIndex > sourceLength) {
                return loopIndex;
            }
        }
    }

    /* READ A SCALAR OR INDIRECT REFERENCE*/
    /* An integer pair followed by R is one value, so callers must advance past all three tokens.*/
    if (sourceBytes[loopIndex] == '/') {
        loopIndex++;
    } else if (strchr("]>)", sourceBytes[loopIndex])) {
        return sourceLength + 1;
    }
    while (loopIndex < sourceLength && !delimiter(sourceBytes[loopIndex])) {
        loopIndex++;
    }
    if (loopIndex == start) {
        return sourceLength + 1;
    }
    if (sourceBytes[start] >= '0' && sourceBytes[start] <= '9') {
        size_t next = loopIndex;
        size_t generation;
        if (uintAt(sourceBytes, sourceLength, &next, &generation)) {
            next = space(sourceBytes, sourceLength, next);
            if (token(sourceBytes, sourceLength, next, "R")) {
                return next + 1;
            }
        }
    }
    return loopIndex;
}
/**
 * Find a named value in a PDF dictionary slice.
 * @param dictionary Dictionary slice to search.
 * @param key Dictionary key or codec key.
 * @param result Receives the value slice when the key exists.
 * @return 1 when found, 0 when absent, or -1 when the dictionary is malformed.
 * @author kai
 */
static int dictValue(Slice dictionary, const char *key, Slice *result) {
    const unsigned char *sourceBytes = dictionary.sourceBytes;
    size_t end = dictionary.sourceLength;
    size_t cursor = space(sourceBytes, end, 0);
    size_t keyLength = strlen(key);
    if (cursor + 2 > end || memcmp(sourceBytes + cursor, "<<", 2) != 0) {
        return 0;
    }
    cursor += 2;
    while ((cursor = space(sourceBytes, end, cursor)) < end) {
        size_t start = cursor;
        size_t keyEnd;
        size_t value;
        size_t finish;
        if (cursor + 1 < end && sourceBytes[cursor] == '>' && sourceBytes[cursor + 1] == '>') {
            return 0;
        }
        if (sourceBytes[cursor] != '/') {
            return -1;
        }
        keyEnd = valueEnd(sourceBytes, end, cursor, 0);
        if (keyEnd > end) {
            return -1;
        }
        value = space(sourceBytes, end, keyEnd);
        finish = valueEnd(sourceBytes, end, value, 0);
        if (finish > end) {
            return -1;
        }
        if (keyEnd - start == keyLength && !memcmp(sourceBytes + start, key, keyLength)) {
            result->sourceBytes = sourceBytes + value;
            result->sourceLength = finish - value;
            return 1;
        }
        cursor = finish;
    }
    return -1;
}
/**
 * Compare a PDF name slice with an unescaped name.
 * @param valueSlice PDF name slice to compare.
 * @param parsedValue Expected decoded name without the leading slash.
 * @return 1 when the complete name matches; otherwise 0.
 * @author kai
 */
static int nameIs(Slice valueSlice, const char *parsedValue) {
    return valueSlice.sourceLength == strlen(parsedValue) &&
           !memcmp(valueSlice.sourceBytes, parsedValue, valueSlice.sourceLength);
}

/* REFERENCES*/
/**
 * Return the parsed value slice for a live indirect object.
 * @param pdf Parsed document object table.
 * @param objectId Indirect object number to look up.
 * @param value Receives the object value slice.
 * @return 1 when the live object is valid; otherwise 0.
 * @author kai
 */
static int objectValue(Pdf *pdf, unsigned objectId, Slice *value) {
    size_t cursor;
    size_t num;
    size_t generationValue;
    if (objectId >= pdf->count || !pdf->objects[objectId].live) {
        return 0;
    }
    cursor = pdf->objects[objectId].offset;
    if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &cursor, &num) ||
        !uintAt(pdf->sourceBytes, pdf->sourceLength, &cursor, &generationValue) ||
        num != objectId || generationValue != pdf->objects[objectId].generation) {
        return 0;
    }
    cursor = space(pdf->sourceBytes, pdf->sourceLength, cursor);
    if (!token(pdf->sourceBytes, pdf->sourceLength, cursor, "obj")) {
        return 0;
    }
    cursor = space(pdf->sourceBytes, pdf->sourceLength, cursor + 3);
    {
        size_t end = valueEnd(pdf->sourceBytes, pdf->sourceLength, cursor, 0);
        if (end > pdf->sourceLength) {
            return 0;
        }
        value->sourceBytes = pdf->sourceBytes + cursor;
        value->sourceLength = end - cursor;
    }
    return 1;
}
/**
 * Parse an indirect object reference and return its object and generation numbers.
 * @param valueSlice Value slice expected to contain an indirect reference.
 * @param objectId Receives the referenced object number.
 * @param generationValue Receives the referenced generation number.
 * @return 1 for a valid bounded reference; otherwise 0.
 * @author kai
 */
static int reference(Slice valueSlice, unsigned *objectId, unsigned *generationValue) {
    size_t cursor = 0;
    size_t firstNumber;
    size_t secondNumber;
    if (!uintAt(valueSlice.sourceBytes, valueSlice.sourceLength, &cursor, &firstNumber) ||
        !uintAt(valueSlice.sourceBytes, valueSlice.sourceLength, &cursor, &secondNumber) ||
        firstNumber > PDF_MAX_OBJECT_COUNT || secondNumber > PDF_MAX_GENERATION) {
        return 0;
    }
    cursor = space(valueSlice.sourceBytes, valueSlice.sourceLength, cursor);
    if (!token(valueSlice.sourceBytes, valueSlice.sourceLength, cursor, "R")) {
        return 0;
    }
    *objectId = (unsigned)firstNumber;
    *generationValue = (unsigned)secondNumber;
    return 1;
}
/**
 * Replace a reference slice with its referenced object value when resolvable.
 * @param pdf Document used to look up indirect objects.
 * @param valueSlice Reference slice, replaced by its resolved value on success.
 * @return 1 when resolved or already direct; otherwise 0 for invalid or cyclic references.
 * @author kai
 */
static int resolve(Pdf *pdf, Slice *valueSlice) {
    unsigned objectId;
    unsigned generationValue;
    unsigned depth;
    for (depth = 0; depth < PDF_MAX_REFERENCE_DEPTH; depth++) {
        if (!reference(*valueSlice, &objectId, &generationValue)) {
            return 1;
        }
        if (objectId >= pdf->count || pdf->objects[objectId].generation != generationValue ||
            !objectValue(pdf, objectId, valueSlice)) {
            return 0;
        }
    }
    return 0;
}
/**
 * Resolve a nonnegative integer PDF value, including an indirect reference.
 * @param pdf Document used to resolve indirect values.
 * @param valueSlice PDF value expected to encode a nonnegative integer.
 * @param parsedValue Receives the integer result.
 * @return 1 when the full value is a valid integer; otherwise 0.
 * @author kai
 */
static int sizeValue(Pdf *pdf, Slice valueSlice, size_t *parsedValue) {
    size_t cursor = 0;
    return resolve(pdf, &valueSlice) &&
           uintAt(valueSlice.sourceBytes, valueSlice.sourceLength, &cursor, parsedValue) &&
           space(valueSlice.sourceBytes, valueSlice.sourceLength, cursor) ==
               valueSlice.sourceLength;
}
/**
 * Read an integer entry from an image object dictionary.
 * @param pdf Document used to resolve dictionary values.
 * @param pdfObject PDF object to inspect.
 * @param key Dictionary key to retrieve.
 * @param parsedValue Receives the resolved integer.
 * @return 1 when the key resolves to an integer; otherwise 0.
 * @author kai
 */
static int getSize(Pdf *pdf, PdfObject *pdfObject, const char *key, size_t *parsedValue) {
    Slice valueSlice;
    return dictValue(pdfObject->value, key, &valueSlice) == 1 &&
           sizeValue(pdf, valueSlice, parsedValue);
}
/**
 * Read an indirect object identifier from an image dictionary entry.
 * @param pdf Document containing indirect objects.
 * @param pdfObject PDF object to inspect.
 * @param key Dictionary key to retrieve.
 * @param objectId Receives the referenced object number.
 * @return 1 for a live reference with a matching generation; otherwise 0.
 * @author kai
 */
static int getRef(Pdf *pdf, PdfObject *pdfObject, const char *key, unsigned *objectId) {
    Slice valueSlice;
    unsigned generationValue;
    if (dictValue(pdfObject->value, key, &valueSlice) != 1 ||
        !reference(valueSlice, objectId, &generationValue) || *objectId >= pdf->count ||
        !pdf->objects[*objectId].live || pdf->objects[*objectId].generation != generationValue) {
        return 0;
    }
    return 1;
}
/* CROSS-REFERENCE VALIDATION*/

/* CROSS REFERENCES*/
/**
 * Index classic PDF objects and trailer data; flag structures that cannot be safely rewritten.
 * @param pdf Document state to populate from the source bytes.
 * @return 1 when the PDF structure is accepted; otherwise 0.
 * @author kai
 */
static int parsePdf(Pdf *pdf) {
    size_t cursor;
    size_t xref = (size_t)-1;
    size_t itemIndex;
    size_t trailer = 0;
    size_t trailerEnd;
    Slice valueSlice;
    size_t count;
    int found = 0;

    /* VALIDATE THE FILE ENVELOPE*/
    if (pdf->sourceLength < PDF_MIN_HEADER_LENGTH ||
        memcmp(pdf->sourceBytes, "%PDF-", PDF_VERSION_MARKER_LENGTH) != 0) {
        return 0;
    }
    cursor = pdf->sourceLength;
    while (cursor && white(pdf->sourceBytes[cursor - 1])) {
        cursor--;
    }
    if (cursor < PDF_EOF_MARKER_LENGTH || memcmp(pdf->sourceBytes + cursor - PDF_EOF_MARKER_LENGTH,
                                                 "%%EOF", PDF_EOF_MARKER_LENGTH) != 0) {
        return 0;
    }

    /* LOCATE THE ACTIVE CROSS-REFERENCE TABLE*/
    /* Use the final startxref offset instead of matching object-like bytes inside image streams.*/
    for (itemIndex = cursor - PDF_EOF_MARKER_LENGTH; itemIndex > 0; itemIndex--) {
        if (token(pdf->sourceBytes, pdf->sourceLength, itemIndex, "startxref")) {
            size_t pos = itemIndex + PDF_STARTXREF_KEYWORD_LENGTH;
            if (uintAt(pdf->sourceBytes, pdf->sourceLength, &pos, &xref)) {
                found = 1;
                break;
            }
        }
    }
    if (!found || xref >= pdf->sourceLength) {
        return 0;
    }
    cursor = xref;
    if (!token(pdf->sourceBytes, pdf->sourceLength, cursor, "xref")) {
        pdf->unsupported = 1;
        return 1;
    }
    cursor += 4;
    /* Find the trailer by traversing the actual xref sections, not binary streams. */
    {
        /* FIND THE TRAILER THROUGH XREF SECTIONS*/
        size_t scan = cursor;
        for (;;) {
            size_t first;
            size_t sourceLength;
            size_t entryIndex;
            size_t firstNumber;
            size_t secondNumber;
            scan = space(pdf->sourceBytes, pdf->sourceLength, scan);
            if (token(pdf->sourceBytes, pdf->sourceLength, scan, "trailer")) {
                trailer =
                    space(pdf->sourceBytes, pdf->sourceLength, scan + PDF_TRAILER_KEYWORD_LENGTH);
                break;
            }
            if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &scan, &first) ||
                !uintAt(pdf->sourceBytes, pdf->sourceLength, &scan, &sourceLength) ||
                first > PDF_MAX_OBJECT_COUNT || sourceLength > PDF_MAX_OBJECT_COUNT - first) {
                return 0;
            }
            for (entryIndex = 0; entryIndex < sourceLength; entryIndex++) {
                if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &scan, &firstNumber) ||
                    !uintAt(pdf->sourceBytes, pdf->sourceLength, &scan, &secondNumber)) {
                    return 0;
                }
                scan = space(pdf->sourceBytes, pdf->sourceLength, scan);
                if (scan >= pdf->sourceLength ||
                    (pdf->sourceBytes[scan] != 'n' && pdf->sourceBytes[scan] != 'f')) {
                    return 0;
                }
                scan++;
            }
        }
    }

    /* VALIDATE TRAILER CAPABILITIES*/
    /* Incremental, hybrid, or encrypted documents use unchanged-source fallback.*/
    trailerEnd = valueEnd(pdf->sourceBytes, pdf->sourceLength, trailer, 0);
    if (trailerEnd > pdf->sourceLength) {
        return 0;
    }
    pdf->trailer = (Slice){pdf->sourceBytes + trailer, trailerEnd - trailer};
    if (dictValue(pdf->trailer, "/Size", &valueSlice) != 1 || !sizeValue(pdf, valueSlice, &count) ||
        !count || count > PDF_MAX_OBJECT_COUNT) {
        return 0;
    }
    pdf->count = (unsigned)count;
    if (dictValue(pdf->trailer, "/Prev", &valueSlice) == 1 ||
        dictValue(pdf->trailer, "/XRefStm", &valueSlice) == 1 ||
        dictValue(pdf->trailer, "/Encrypt", &valueSlice) == 1) {
        pdf->unsupported = 1;
        return 1;
    }

    /* INDEX DECLARED OBJECT LOCATIONS*/
    pdf->objects = calloc(count, sizeof(*pdf->objects));
    if (!pdf->objects) {
        return 0;
    }
    while ((cursor = space(pdf->sourceBytes, pdf->sourceLength, cursor)) < trailer) {
        size_t first;
        size_t sourceLength;
        size_t entryIndex;
        if (token(pdf->sourceBytes, pdf->sourceLength, cursor, "trailer")) {
            break;
        }
        if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &cursor, &first) ||
            !uintAt(pdf->sourceBytes, pdf->sourceLength, &cursor, &sourceLength) ||
            first >= count || sourceLength > count - first) {
            return 0;
        }
        for (entryIndex = 0; entryIndex < sourceLength; entryIndex++) {
            size_t offset;
            size_t generationValue;
            PdfObject *pdfObject = &pdf->objects[first + entryIndex];
            if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &cursor, &offset) ||
                !uintAt(pdf->sourceBytes, pdf->sourceLength, &cursor, &generationValue) ||
                generationValue > PDF_MAX_GENERATION) {
                return 0;
            }
            cursor = space(pdf->sourceBytes, pdf->sourceLength, cursor);
            pdfObject->live = pdf->sourceBytes[cursor] == 'n';
            pdfObject->offset = offset;
            pdfObject->generation = (unsigned)generationValue;
            cursor++;
            if (pdfObject->live && offset >= xref) {
                return 0;
            }
        }
    }

    /* VALIDATE LIVE OBJECT BOUNDARIES*/
    for (itemIndex = 1; itemIndex < count; itemIndex++) {
        if (pdf->objects[itemIndex].live) {
            PdfObject *pdfObject = &pdf->objects[itemIndex];
            size_t pos;
            if (!objectValue(pdf, (unsigned)itemIndex, &pdfObject->value)) {
                return 0;
            }
            pos = space(pdf->sourceBytes, pdf->sourceLength,
                        (size_t)(pdfObject->value.sourceBytes - pdf->sourceBytes) +
                            pdfObject->value.sourceLength);

            /* BOUND BINARY STREAMS BY THEIR LENGTH*/
            /* Resolve /Length before seeking terminators; encoded pixels may contain PDF-like*/
            /* words.*/
            if (token(pdf->sourceBytes, pdf->sourceLength, pos, "stream")) {
                pos += PDF_STREAM_KEYWORD_LENGTH;
                if (pos >= pdf->sourceLength) {
                    return 0;
                }
                if (pdf->sourceBytes[pos] == '\r') {
                    pos++;
                    if (pos < pdf->sourceLength && pdf->sourceBytes[pos] == '\n') {
                        pos++;
                    }
                } else if (pdf->sourceBytes[pos] == '\n') {
                    pos++;
                } else {
                    return 0;
                }
                pdfObject->stream = pos;
                if (!getSize(pdf, pdfObject, "/Length", &pdfObject->length) ||
                    pdfObject->length > pdf->sourceLength - pos) {
                    return 0;
                }
                pos = space(pdf->sourceBytes, pdf->sourceLength, pos + pdfObject->length);
                if (!token(pdf->sourceBytes, pdf->sourceLength, pos, "endstream")) {
                    return 0;
                }
                pos =
                    space(pdf->sourceBytes, pdf->sourceLength, pos + PDF_ENDSTREAM_KEYWORD_LENGTH);
            }
            if (!token(pdf->sourceBytes, pdf->sourceLength, pos, "endobj")) {
                return 0;
            }
            pdfObject->end = pdf->sourceBytes + pos;

            /* PRESERVE SIGNED AND PACKED STRUCTURES*/
            if (dictValue(pdfObject->value, "/ByteRange", &valueSlice) == 1) {
                pdf->unsupported = 1;
                return 1;
            }
            if (dictValue(pdfObject->value, "/Type", &valueSlice) == 1 &&
                (nameIs(valueSlice, "/ObjStm") || nameIs(valueSlice, "/XRef"))) {
                pdf->unsupported = 1;
                return 1;
            }
        }
    }
    return 1;
}

/*//////////////////////////////////////////////////
IMAGES
//////////////////////////////////////////////////*/

/* FILTERS*/
/**
 * Decode an ASCII85 stream into the output buffer.
 * @param valueSlice Encoded ASCII85 stream slice.
 * @param out Receives decoded stream bytes.
 * @return 1 for a complete valid stream; otherwise 0 for malformed input or allocation failure.
 * @author kai
 */
static int ascii85(Slice valueSlice, RcBuffer *out) {
    unsigned long value = 0;
    unsigned count = 0;
    size_t itemIndex;
    int ended = 0;

    /* DECODE COMPLETE TUPLES*/
    /* The z shorthand is valid only between tuples; five base-85 digits produce four bytes.*/
    for (itemIndex = 0; itemIndex < valueSlice.sourceLength; itemIndex++) {
        unsigned byteValue = valueSlice.sourceBytes[itemIndex];
        if (white((unsigned char)byteValue)) {
            continue;
        }
        if (byteValue == '~') {
            if (itemIndex + 1 >= valueSlice.sourceLength ||
                valueSlice.sourceBytes[++itemIndex] != '>') {
                return 0;
            }
            ended = 1;
            itemIndex++;
            break;
        }
        if (byteValue == 'z') {
            unsigned char zeros[4] = {0};
            if (count || !rcAppend(out, zeros, sizeof(zeros))) {
                return 0;
            }
            continue;
        }
        if (byteValue < PDF_ASCII85_MINIMUM || byteValue > PDF_ASCII85_MAXIMUM) {
            return 0;
        }
        value = value * PDF_ASCII85_RADIX + byteValue - PDF_ASCII85_MINIMUM;
        count++;
        if (count == PDF_ASCII85_TUPLE_DIGITS) {
            unsigned char bytes[4];
            unsigned loopIndex;
            if (value > UINT32_MAX) {
                return 0;
            }
            for (loopIndex = 0; loopIndex < 4; loopIndex++) {
                bytes[loopIndex] = (unsigned char)(value >> (3 * PDF_BITS_PER_BYTE -
                                                             loopIndex * PDF_BITS_PER_BYTE));
            }
            if (!rcAppend(out, bytes, sizeof(bytes))) {
                return 0;
            }
            count = 0;
            value = 0;
        }
    }

    /* VALIDATE THE TERMINATOR*/
    if (!ended || count == 1) {
        return 0;
    }
    while (itemIndex < valueSlice.sourceLength) {
        if (!white(valueSlice.sourceBytes[itemIndex++])) {
            return 0;
        }
    }

    /* DECODE THE FINAL PARTIAL TUPLE*/
    /* Pad with the largest digit and emit one fewer byte than the original digit count.*/
    if (count) {
        unsigned char bytes[4];
        unsigned loopIndex;
        unsigned original = count;
        while (count++ < PDF_ASCII85_TUPLE_DIGITS) {
            value = value * PDF_ASCII85_RADIX + PDF_ASCII85_PADDING_DIGIT;
        }
        if (value > UINT32_MAX) {
            return 0;
        }
        for (loopIndex = 0; loopIndex < 4; loopIndex++) {
            bytes[loopIndex] =
                (unsigned char)(value >> (3 * PDF_BITS_PER_BYTE - loopIndex * PDF_BITS_PER_BYTE));
        }
        if (!rcAppend(out, bytes, original - 1)) {
            return 0;
        }
    }
    return 1;
}
/**
 * Convert one hexadecimal character to its numeric value.
 * @param byteValue ASCII character expected to be a hexadecimal digit.
 * @return Value from 0 through 15, or -1 when c is not hexadecimal.
 * @author kai
 */
static int hexDigit(unsigned byteValue) {
    if (byteValue >= '0' && byteValue <= '9') {
        return (int)byteValue - '0';
    }
    if (byteValue >= 'A' && byteValue <= 'F') {
        return (int)byteValue - 'A' + PDF_DECIMAL_RADIX;
    }
    if (byteValue >= 'a' && byteValue <= 'f') {
        return (int)byteValue - 'a' + PDF_DECIMAL_RADIX;
    }
    return -1;
}
/**
 * Decode a PDF ASCIIHex stream, accepting its optional terminator.
 * @param valueSlice Encoded ASCIIHex stream slice.
 * @param out Receives decoded stream bytes.
 * @return 1 for a valid terminated stream; otherwise 0 for invalid input or allocation failure.
 * @author kai
 */
static int asciiHex(Slice valueSlice, RcBuffer *out) {
    int prior = -1;
    size_t itemIndex;
    for (itemIndex = 0; itemIndex < valueSlice.sourceLength; itemIndex++) {
        int parsedValue;
        if (white(valueSlice.sourceBytes[itemIndex])) {
            continue;
        }
        if (valueSlice.sourceBytes[itemIndex] == '>') {
            break;
        }
        parsedValue = hexDigit(valueSlice.sourceBytes[itemIndex]);
        if (parsedValue < 0) {
            return 0;
        }
        if (prior < 0) {
            prior = parsedValue;
        } else {
            unsigned char byteValue = (unsigned char)((prior * PDF_HEX_RADIX) + parsedValue);
            if (!rcAppend(out, &byteValue, 1)) {
                return 0;
            }
            prior = -1;
        }
    }
    if (itemIndex == valueSlice.sourceLength) {
        return 0;
    }
    if (prior >= 0) {
        unsigned char byteValue = (unsigned char)(prior * PDF_HEX_RADIX);
        if (!rcAppend(out, &byteValue, 1)) {
            return 0;
        }
    }
    return 1;
}
/**
 * Decode supported PDF stream filters and report whether the result is JPEG data.
 * @param pdf Document used to resolve filter parameters.
 * @param pdfObject PDF object whose stream filters are decoded.
 * @param out Receives decoded stream bytes.
 * @param jpeg Set when the final stream is JPEG/DCT encoded.
 * @return 1 on success, 0 for unsupported filters, -1 for corrupt supported encodings.
 * @author kai
 */
static int decodeFilters(Pdf *pdf, PdfObject *pdfObject, RcBuffer *out, int *jpeg) {
    Slice filter;
    Slice items[PDF_MAX_FILTER_COUNT];
    unsigned count = 0;
    unsigned itemIndex;
    RcBuffer current = {0};
    int successStatus = 0;
    *jpeg = 0;

    /* RESOLVE THE ORDERED FILTER LIST*/
    if (dictValue(pdfObject->value, "/Filter", &filter) == 1) {
        size_t cursor = 0;
        if (!resolve(pdf, &filter)) {
            return 0;
        }
        if (filter.sourceLength && filter.sourceBytes[0] == '[') {
            cursor = 1;
        }
        while ((cursor = space(filter.sourceBytes, filter.sourceLength, cursor)) <
                   filter.sourceLength &&
               filter.sourceBytes[cursor] != ']') {
            size_t end = valueEnd(filter.sourceBytes, filter.sourceLength, cursor, 0);
            if (end > filter.sourceLength || count == PDF_MAX_FILTER_COUNT) {
                return 0;
            }
            items[count].sourceBytes = filter.sourceBytes + cursor;
            items[count++].sourceLength = end - cursor;
            cursor = end;
        }
    }
    if (!rcAppend(&current, pdf->sourceBytes + pdfObject->stream, pdfObject->length)) {
        goto done;
    }

    /* DECODE FILTERS IN DECLARED ORDER*/
    /* DCT remains encoded until the shared JPEG decoder receives its byte stream.*/
    for (itemIndex = 0; itemIndex < count; itemIndex++) {
        RcBuffer next = {0};
        Slice bytes = {current.data, current.size};
        int supported = 0;
        int known = 1;
        if (nameIs(items[itemIndex], "/FlateDecode") || nameIs(items[itemIndex], "/Fl")) {
            supported = rcInflate(current.data, current.size, &next);
        } else if (nameIs(items[itemIndex], "/ASCII85Decode") || nameIs(items[itemIndex], "/A85")) {
            supported = ascii85(bytes, &next);
        } else if (nameIs(items[itemIndex], "/ASCIIHexDecode") ||
                   nameIs(items[itemIndex], "/AHx")) {
            supported = asciiHex(bytes, &next);
        } else if ((nameIs(items[itemIndex], "/DCTDecode") || nameIs(items[itemIndex], "/DCT")) &&
                   itemIndex + 1 == count) {
            *jpeg = 1;
            break;
        } else {
            known = 0;
        }

        /* DISTINGUISH UNSUPPORTED DATA FROM CORRUPTION*/
        if (!supported) {
            free(next.data);
            successStatus = known ? -1 : 0;
            goto done;
        }
        free(current.data);
        current = next;
    }
    *out = current;
    current = (RcBuffer){0};
    successStatus = 1;
done:
    free(current.data);
    return successStatus;
}

/* SAMPLE LAYOUT*/
/**
 * Reverse a PDF/PNG predictor using the image geometry and dictionary parameters.
 * @param pdf Document used to resolve DecodeParms values.
 * @param pdfObject Image object containing predictor parameters.
 * @param data Flate-decoded bytes, replaced with reconstructed samples.
 * @param imageWidth Image width in pixels.
 * @param imageHeight Image height in rows.
 * @param channels Samples per pixel.
 * @param bits Bits per sample.
 * @return 1 when samples are reconstructed or no predictor is present; otherwise 0.
 * @author kai
 */
static int predictor(Pdf *pdf, PdfObject *pdfObject, RcBuffer *data, unsigned imageWidth,
                     unsigned imageHeight, unsigned channels, unsigned bits) {
    Slice parms;
    Slice valueSlice;
    size_t pred = 1;
    size_t columns = imageWidth;
    size_t colors = channels;
    size_t bpc = bits;
    size_t row =
        ((size_t)imageWidth * channels * bits + (PDF_BITS_PER_BYTE - 1)) / PDF_BITS_PER_BYTE;
    size_t bpp = (channels * bits + (PDF_BITS_PER_BYTE - 1)) / PDF_BITS_PER_BYTE;
    size_t rowIndex;
    size_t columnIndex;

    /* RESOLVE PREDICTOR SETTINGS*/
    if (dictValue(pdfObject->value, "/DecodeParms", &parms) != 1) {
        return 1;
    }
    if (!resolve(pdf, &parms)) {
        return 0;
    }
    if (parms.sourceLength && parms.sourceBytes[0] == '[') {
        size_t cursor = 1;
        int found = 0;
        while ((cursor = space(parms.sourceBytes, parms.sourceLength, cursor)) <
                   parms.sourceLength &&
               parms.sourceBytes[cursor] != ']') {
            size_t end = valueEnd(parms.sourceBytes, parms.sourceLength, cursor, 0);
            if (end > parms.sourceLength) {
                return 0;
            }
            if (parms.sourceBytes[cursor] == '<') {
                Slice chosen = {parms.sourceBytes + cursor, end - cursor};
                parms = chosen;
                found = 1;
                break;
            }
            cursor = end;
        }
        if (!found) {
            return 1;
        }
    }
    if (parms.sourceLength < 2 || parms.sourceBytes[0] != '<') {
        return nameIs(parms, "null");
    }
    if (dictValue(parms, "/Predictor", &valueSlice) == 1 && !sizeValue(pdf, valueSlice, &pred)) {
        return 0;
    }

    /* RESOLVE THE ACTIVE SAMPLE LAYOUT*/
    /* An omitted predictor means the decoded stream already contains raw rows.*/
    if (pred == 1) {
        return 1;
    }
    if (dictValue(parms, "/Columns", &valueSlice) == 1 && !sizeValue(pdf, valueSlice, &columns)) {
        return 0;
    }
    if (dictValue(parms, "/Colors", &valueSlice) == 1 && !sizeValue(pdf, valueSlice, &colors)) {
        return 0;
    }
    if (dictValue(parms, "/BitsPerComponent", &valueSlice) == 1 &&
        !sizeValue(pdf, valueSlice, &bpc)) {
        return 0;
    }

    /* VALIDATE ROW GEOMETRY*/
    if (columns != imageWidth || colors != channels || bpc != bits) {
        return 0;
    }

    /* REVERSE TIFF SAMPLE DIFFERENCES*/
    /* Work in sample units, including packed and sixteen-bit components.*/
    if (pred == 2) {
        if (data->size != row * imageHeight) {
            return 0;
        }
        for (rowIndex = 0; rowIndex < imageHeight; rowIndex++) {
            for (columnIndex = channels; columnIndex < (size_t)imageWidth * channels;
                 columnIndex++) {
                unsigned char *rowData = data->data + (rowIndex * row);
                unsigned value = (rcSample(rowData, columnIndex, bits) +
                                  rcSample(rowData, columnIndex - channels, bits)) &
                                 ((1U << bits) - 1);
                if (bits == PDF_BITS_PER_BYTE) {
                    rowData[columnIndex] = (unsigned char)value;
                } else if (bits == 2 * PDF_BITS_PER_BYTE) {
                    rowData[columnIndex * 2] = (unsigned char)(value >> PDF_BITS_PER_BYTE);
                    rowData[(columnIndex * 2) + 1] = (unsigned char)value;
                } else {
                    size_t index = columnIndex * bits / PDF_BITS_PER_BYTE;
                    unsigned shift =
                        PDF_BITS_PER_BYTE - bits - (columnIndex * bits % PDF_BITS_PER_BYTE);
                    unsigned mask = ((1U << bits) - 1) << shift;
                    rowData[index] = (unsigned char)((rowData[index] & ~mask) | (value << shift));
                }
            }
        }
        return 1;
    }

    /* REVERSE PNG BYTE FILTERS*/
    /* Each encoded row begins with a filter byte before its pixel bytes.*/
    if (pred < PNG_PREDICTOR_MINIMUM || pred > PNG_PREDICTOR_MAXIMUM ||
        data->size != (row + 1) * imageHeight) {
        return 0;
    }
    for (rowIndex = 0; rowIndex < imageHeight; rowIndex++) {
        unsigned filter = data->data[rowIndex * (row + 1)];
        unsigned char *decoded = data->data + (rowIndex * row);
        /* Compact away the filter byte; earlier decoded rows cannot overlap unread input.*/
        movePdfBytes(decoded, data->data + (rowIndex * (row + 1)) + 1, row);
        if (!rcUnfilter(decoded, rowIndex ? decoded - row : NULL, row, bpp, filter)) {
            return 0;
        }
    }
    data->size = row * imageHeight;
    return 1;
}

/* IMAGE OBJECTS*/
/**
 * Decode a supported PDF image XObject, including supported masks and filters.
 * @param pdf Document containing the image XObject and masks.
 * @param objectId Indirect object number of the image.
 * @param image Receives decoded RGBA pixels.
 * @param depth Mask-recursion depth, bounded to prevent cycles.
 * @return 1 with owned pixels, 0 for unsupported objects, -1 for corrupt payloads.
 * @author kai
 */
static int pdfImage(Pdf *pdf, unsigned objectId, RcImage *image, unsigned depth) {
    PdfObject *pdfObject = &pdf->objects[objectId];
    Slice color;
    Slice valueSlice;
    Slice decode = {0};
    Slice mask = {0};
    size_t imageWidth;
    size_t imageHeight;
    size_t bits = PDF_BITS_PER_BYTE;
    unsigned channels = 0;
    unsigned indexed = 0;
    unsigned high = 0;
    unsigned maskId;
    unsigned generationValue;
    unsigned char lookup[PDF_RGB_LOOKUP_CAPACITY];
    RcBuffer data = {0};
    int jpeg = 0;
    int successStatus = 0;
    double ranges[3][2] = {{0, 1}, {0, 1}, {0, 1}};
    size_t columnIndex;
    size_t rowIndex;
    size_t row;
    unsigned itemIndex;

    /* VALIDATE IMAGE CAPABILITIES*/
    /* Unsupported matte, mask, or colour metadata must preserve the original object.*/
    *image = (RcImage){0};
    if (dictValue(pdfObject->value, "/Matte", &valueSlice) == 1) {
        return 0;
    }
    if (dictValue(pdfObject->value, "/SMask", &valueSlice) == 1 && !nameIs(valueSlice, "/None") &&
        !getRef(pdf, pdfObject, "/SMask", &maskId)) {
        return 0;
    }
    if (depth > 2 || !pdfObject->stream || !getSize(pdf, pdfObject, "/Width", &imageWidth) ||
        !getSize(pdf, pdfObject, "/Height", &imageHeight) || !imageWidth || !imageHeight ||
        imageWidth > RC_PIXELS / imageHeight) {
        return 0;
    }
    if (dictValue(pdfObject->value, "/ImageMask", &valueSlice) == 1 &&
        !nameIs(valueSlice, "false")) {
        return 0;
    }
    if (dictValue(pdfObject->value, "/ColorSpace", &color) != 1 || !resolve(pdf, &color)) {
        return 0;
    }

    /* RESOLVE THE COLOUR SPACE*/
    if (nameIs(color, "/DeviceRGB")) {
        channels = 3;
    } else if (nameIs(color, "/DeviceGray")) {
        channels = 1;
    } else if (color.sourceLength > 2 && color.sourceBytes[0] == '[') {
        size_t cursor = space(color.sourceBytes, color.sourceLength, 1);
        size_t end;
        Slice entry;
        end = valueEnd(color.sourceBytes, color.sourceLength, cursor, 0);
        if (end > color.sourceLength) {
            return 0;
        }
        entry = (Slice){color.sourceBytes + cursor, end - cursor};
        if (!nameIs(entry, "/Indexed")) {
            return 0;
        }
        cursor = space(color.sourceBytes, color.sourceLength, end);
        end = valueEnd(color.sourceBytes, color.sourceLength, cursor, 0);
        if (end > color.sourceLength) {
            return 0;
        }
        entry = (Slice){color.sourceBytes + cursor, end - cursor};
        if (!nameIs(entry, "/DeviceRGB")) {
            return 0;
        }

        /* READ THE INDEXED PALETTE LIMIT*/
        /* The maximum index bounds both pixel values and the required lookup byte count.*/
        cursor = space(color.sourceBytes, color.sourceLength, end);
        {
            size_t value;
            if (!uintAt(color.sourceBytes, color.sourceLength, &cursor, &value) ||
                value > PDF_SAMPLE_MAXIMUM) {
                return 0;
            }
            high = (unsigned)value;
        }
        cursor = space(color.sourceBytes, color.sourceLength, cursor);
        end = valueEnd(color.sourceBytes, color.sourceLength, cursor, 0);
        if (end > color.sourceLength) {
            return 0;
        }
        entry = (Slice){color.sourceBytes + cursor, end - cursor};

        /* LOAD AN INDEXED COLOUR LOOKUP*/
        /* A palette can be a referenced stream or an inline hexadecimal byte sequence.*/
        {
            RcBuffer lut = {0};
            int decoded = 0;
            int jpegLookup = 0;
            if (reference(entry, &maskId, &generationValue)) {
                decoded = maskId < pdf->count &&
                          decodeFilters(pdf, &pdf->objects[maskId], &lut, &jpegLookup) == 1 &&
                          !jpegLookup;
            } else if (entry.sourceLength >= 2 && entry.sourceBytes[0] == '<' &&
                       entry.sourceBytes[1] != '<') {
                Slice bytes = {entry.sourceBytes + 1, entry.sourceLength - 1};
                decoded = asciiHex(bytes, &lut);
            }
            if (!decoded || lut.size != (size_t)(high + 1) * 3) {
                free(lut.data);
                return 0;
            }
            if (lut.size > sizeof(lookup)) {
                free(lut.data);
                return 0;
            }
            copyPdfBytes(lookup, lut.data, lut.size);
            free(lut.data);
        }
        channels = 1;
        indexed = 1;
    } else {
        return 0;
    }
    {
        /* DECODE THE IMAGE PAYLOAD*/
        int status = decodeFilters(pdf, pdfObject, &data, &jpeg);
        if (status <= 0) {
            successStatus = status;
            goto done;
        }
    }

    /* DECODE JPEG OR RECONSTRUCT RAW SAMPLES*/
    if (jpeg) {
        if (dictValue(pdfObject->value, "/Decode", &valueSlice) == 1) {
            goto done;
        }
        if (indexed) {
            goto done;
        }
        if (!rcJpeg(data.data, data.size, image, 0) || image->width != imageWidth ||
            image->height != imageHeight) {
            successStatus = -1;
            goto done;
        }
    } else {
        if (!getSize(pdf, pdfObject, "/BitsPerComponent", &bits) ||
            (bits != 1 && bits != 2 && bits != 4 && bits != PDF_BITS_PER_BYTE &&
             bits != PDF_SIXTEEN_BITS) ||
            !predictor(pdf, pdfObject, &data, (unsigned)imageWidth, (unsigned)imageHeight, channels,
                       (unsigned)bits)) {
            goto done;
        }
        row = (imageWidth * channels * bits + (PDF_BITS_PER_BYTE - 1)) / PDF_BITS_PER_BYTE;
        if (data.size != row * imageHeight) {
            goto done;
        }
        image->rgba = malloc(imageWidth * imageHeight * 4);
        if (!image->rgba) {
            goto done;
        }
        image->width = (unsigned)imageWidth;
        image->height = (unsigned)imageHeight;

        /* READ RAW-SAMPLE DECODE RANGES*/
        if (dictValue(pdfObject->value, "/Decode", &decode) == 1) {
            size_t cursor = 1;
            if (!resolve(pdf, &decode) || decode.sourceBytes[0] != '[' || indexed) {
                goto done;
            }
            for (itemIndex = 0; itemIndex < channels; itemIndex++) {
                unsigned endpoint;
                for (endpoint = 0; endpoint < 2; endpoint++) {
                    char *end;
                    cursor = space(decode.sourceBytes, decode.sourceLength, cursor);
                    ranges[itemIndex][endpoint] =
                        strtod((const char *)decode.sourceBytes + cursor, &end);
                    if ((const unsigned char *)end == decode.sourceBytes + cursor ||
                        (const unsigned char *)end > decode.sourceBytes + decode.sourceLength ||
                        !isfinite(ranges[itemIndex][endpoint])) {
                        goto done;
                    }
                    cursor = (size_t)((const unsigned char *)end - decode.sourceBytes);
                }
            }
        }

        /* EXPAND RASTER SAMPLES*/
        /* Indexed samples address the lookup; direct samples use their declared decode ranges.*/
        for (rowIndex = 0; rowIndex < imageHeight; rowIndex++) {
            for (columnIndex = 0; columnIndex < imageWidth; columnIndex++) {
                unsigned char *pixel = image->rgba + ((rowIndex * imageWidth + columnIndex) * 4);
                const unsigned char *rowData = data.data + (rowIndex * row);
                unsigned values[3] = {0};
                for (itemIndex = 0; itemIndex < channels; itemIndex++) {
                    values[itemIndex] =
                        rcSample(rowData, (columnIndex * channels) + itemIndex, (unsigned)bits);
                }
                pixel[3] = PDF_SAMPLE_MAXIMUM;
                if (indexed) {
                    if (values[0] > high) {
                        goto done;
                    }
                    copyPdfBytes(pixel, lookup + ((size_t)values[0] * 3), 3);
                } else {
                    for (itemIndex = 0; itemIndex < channels; itemIndex++) {
                        double value =
                            (ranges[itemIndex][0] +
                             values[itemIndex] * (ranges[itemIndex][1] - ranges[itemIndex][0]) /
                                 ((1U << bits) - 1)) *
                            PDF_SAMPLE_MAXIMUM;
                        if (value < 0) {
                            value = 0;
                        }
                        if (value > PDF_SAMPLE_MAXIMUM) {
                            value = PDF_SAMPLE_MAXIMUM;
                        }
                        pixel[itemIndex] = (unsigned char)floor(value + PDF_SAMPLE_ROUNDING_BIAS);
                    }
                    if (channels == 1) {
                        pixel[1] = pixel[2] = pixel[0];
                    }
                }
            }
        }
    }

    /* APPLY COLOUR-KEY TRANSPARENCY*/
    /* Compare the original packed samples before their colour-range mapping.*/
    if (dictValue(pdfObject->value, "/Mask", &mask) == 1) {
        if (!resolve(pdf, &mask) || !mask.sourceLength || mask.sourceBytes[0] != '[') {
            goto done; /* Colour-key masking: compare original samples, before decode mapping. */
        }
        {
            size_t cursor = 1;
            size_t limits[PDF_COLOR_KEY_RANGE_COUNT];
            if (jpeg) {
                goto done;
            }
            for (itemIndex = 0; itemIndex < channels * 2; itemIndex++) {
                if (!uintAt(mask.sourceBytes, mask.sourceLength, &cursor, &limits[itemIndex])) {
                    goto done;
                }
            }
            row = (imageWidth * channels * bits + (PDF_BITS_PER_BYTE - 1)) / PDF_BITS_PER_BYTE;
            for (rowIndex = 0; rowIndex < imageHeight; rowIndex++) {
                for (columnIndex = 0; columnIndex < imageWidth; columnIndex++) {
                    int transparent = 1;
                    for (itemIndex = 0; itemIndex < channels; itemIndex++) {
                        unsigned value =
                            rcSample(data.data + (rowIndex * row),
                                     (columnIndex * channels) + itemIndex, (unsigned)bits);
                        if (value < limits[(size_t)itemIndex * 2] ||
                            value > limits[((size_t)itemIndex * 2) + 1]) {
                            transparent = 0;
                        }
                    }
                    if (transparent) {
                        image->rgba[((rowIndex * imageWidth + columnIndex) * 4) + 3] = 0;
                    }
                }
            }
        }
    }

    /* APPLY THE SOFT MASK*/
    /* Decode the mask recursively, then copy its grayscale coverage into straight alpha.*/
    if (getRef(pdf, pdfObject, "/SMask", &maskId)) {
        RcImage alpha = {0};
        if (!pdfImage(pdf, maskId, &alpha, depth + 1) || alpha.width != imageWidth ||
            alpha.height != imageHeight) {
            free(alpha.rgba);
            goto done;
        }
        for (columnIndex = 0; columnIndex < imageWidth * imageHeight; columnIndex++) {
            image->rgba[(columnIndex * 4) + 3] = alpha.rgba[columnIndex * 4];
        }
        free(alpha.rgba);
    }
    successStatus = 1;
done:
    /* RELEASE IMAGE WORKSPACE*/
    free(data.data);
    if (successStatus <= 0) {
        free(image->rgba);
        *image = (RcImage){0};
    }
    return successStatus;
}

/*//////////////////////////////////////////////////
OUTPUT
//////////////////////////////////////////////////*/

/* OBJECT SERIALIZATION*/
/**
 * Append an unsigned decimal number, padding to a minimum field width when requested.
 * @param out Destination buffer.
 * @param value Number to append.
 * @param minimumDigits Minimum number of emitted digits.
 * @return 1 when every digit is appended; otherwise 0 on allocation failure.
 * @author kai
 */
typedef struct {
    size_t digits;
} PdfMinimumDigits;

static int appendUnsigned(RcBuffer *out, unsigned long value, PdfMinimumDigits minimumDigits) {
    char digits[sizeof(value) * CHAR_BIT];
    size_t digitCount = 0;
    do {
        digits[digitCount++] = (char)('0' + (value % PDF_DECIMAL_RADIX));
        value /= PDF_DECIMAL_RADIX;
    } while (value);
    while (digitCount < minimumDigits.digits) {
        digits[digitCount++] = '0';
    }
    while (digitCount) {
        if (!rcAppend(out, &digits[--digitCount], sizeof(digits[0]))) {
            return 0;
        }
    }
    return 1;
}

/**
 * Write indexed PDF objects, substituting only successfully processed image streams.
 * @param pdf Parsed document with any accepted stream replacements.
 * @param out Receives the serialized PDF candidate.
 * @return 1 when all objects and cross-reference data are written; otherwise 0.
 * @author kai
 */
static int serialize(Pdf *pdf, RcBuffer *out) {
    unsigned itemIndex;
    size_t *offsets = calloc(pdf->count, sizeof(*offsets));
    size_t xref;
    int successStatus = 0;
    if (!offsets) {
        return 0;
    }
    if (!rcAppend(out, "%PDF-1.4\n%\342\343\317\323\n", PDF_HEADER_BYTES_LENGTH)) {
        goto done;
    }

    /* WRITE LIVE OBJECT BODIES*/
    /* Replacement forms keep the original object number and generation.*/
    for (itemIndex = 1; itemIndex < pdf->count; itemIndex++) {
        if (pdf->objects[itemIndex].live) {
            PdfObject *pdfObject = &pdf->objects[itemIndex];
            offsets[itemIndex] = out->size;
            if (!appendUnsigned(out, itemIndex, (PdfMinimumDigits){0}) ||
                !rcAppend(out, " ", sizeof(" ") - 1) ||
                !appendUnsigned(out, pdfObject->generation, (PdfMinimumDigits){0}) ||
                !rcAppend(out, " obj\n", sizeof(" obj\n") - 1)) {
                goto done;
            }
            if (pdfObject->replacement.size) {
                if (!rcAppend(out, pdfObject->replacement.data, pdfObject->replacement.size)) {
                    goto done;
                }
            } else if (!rcAppend(out, pdfObject->value.sourceBytes,
                                 (size_t)(pdfObject->end - pdfObject->value.sourceBytes))) {
                goto done;
            }
            if (!rcAppend(out, "\nendobj\n", PDF_END_OBJECT_TEXT_LENGTH)) {
                goto done;
            }
        }
    }

    /* REBUILD OFFSETS AND RETAIN THE TRAILER*/
    xref = out->size;
    if (!rcAppend(out, "xref\n0 ", sizeof("xref\n0 ") - 1) ||
        !appendUnsigned(out, pdf->count, (PdfMinimumDigits){0}) ||
        !rcAppend(out, "\n0000000000 65535 f \n", sizeof("\n0000000000 65535 f \n") - 1)) {
        goto done;
    }
    for (itemIndex = 1; itemIndex < pdf->count; itemIndex++) {
        if (!appendUnsigned(out, (unsigned long)offsets[itemIndex],
                            (PdfMinimumDigits){PDF_XREF_OFFSET_WIDTH}) ||
            !rcAppend(out, " ", sizeof(" ") - 1) ||
            !appendUnsigned(out, pdf->objects[itemIndex].generation,
                            (PdfMinimumDigits){PDF_XREF_GENERATION_WIDTH}) ||
            !rcAppend(out, pdf->objects[itemIndex].live ? " n \n" : " f \n", sizeof(" n \n") - 1)) {
            goto done;
        }
    }
    if (!rcAppend(out, "trailer\n", PDF_TRAILER_TEXT_LENGTH) ||
        !rcAppend(out, pdf->trailer.sourceBytes, pdf->trailer.sourceLength)) {
        goto done;
    }
    if (!rcAppend(out, "\nstartxref\n", sizeof("\nstartxref\n") - 1) ||
        !appendUnsigned(out, (unsigned long)xref, (PdfMinimumDigits){0}) ||
        !rcAppend(out, "\n%%EOF\n", sizeof("\n%%EOF\n") - 1)) {
        goto done;
    }
    successStatus = 1;
done:
    free(offsets);
    return successStatus;
}

/* PDF PIPELINE*/
/**
 * Process eligible PDF image streams and preserve the original bytes on safe fallback.
 * @param data Complete source PDF bytes.
 * @param length Number of bytes available in data.
 * @param options Vectorization and output policy.
 * @param candidate Receives rebuilt bytes, or stays empty when the source should be retained.
 * @param images Receives owned buffers; masks belong to their image rather than separate exports.
 * @param imageCount Number of image outputs.
 * @param unsupported Counter for retained unsupported objects.
 * @return 1 for a candidate or unchanged fallback, 0 for malformed input or allocation failure.
 * @author kai
 */
int processPdf(const unsigned char *data, size_t length, const MediaOptions *options,
               RcBuffer *candidate, RcBuffer **images, size_t *imageCount, unsigned *unsupported) {
    Pdf pdf = {0};
    RcImage image = {0};
    RcVector vector = {0};
    RcBuffer compressed = {0};
    unsigned itemIndex;
    int successStatus = 0;
    pdf.sourceBytes = data;
    pdf.sourceLength = length;
    *images = NULL;
    *imageCount = 0;
    *unsupported = 0;

    /* VALIDATE THE DOCUMENT AND MODE*/
    if (!parsePdf(&pdf)) {
        goto done;
    }
    if (pdf.unsupported || !options->vectorMode) {
        *unsupported = (unsigned)pdf.unsupported;
        successStatus = 1;
        goto done;
    }

    /* Soft masks belong to their owning images and must not become standalone forms.*/
    for (itemIndex = 1; itemIndex < pdf.count; itemIndex++) {
        if (pdf.objects[itemIndex].live) {
            unsigned mask;
            if (getRef(&pdf, &pdf.objects[itemIndex], "/SMask", &mask) ||
                getRef(&pdf, &pdf.objects[itemIndex], "/Mask", &mask)) {
                pdf.objects[mask].mask = 1;
            }
        }
    }

    /* Reuse each image object ID so existing page resource references remain valid.*/
    for (itemIndex = 1; itemIndex < pdf.count; itemIndex++) {
        PdfObject *pdfObject = &pdf.objects[itemIndex];
        Slice subtype;
        if (!pdfObject->live || pdfObject->mask ||
            dictValue(pdfObject->value, "/Subtype", &subtype) != 1 || !nameIs(subtype, "/Image")) {
            continue;
        }
        {
            /* DECODE ONE IMAGE*/
            /* Unsupported metadata preserves the object; corrupt supported payloads abort the*/
            /* candidate.*/
            int decoded = pdfImage(&pdf, itemIndex, &image, 0);
            if (decoded < 0) {
                goto done;
            }
            if (!decoded) {
                (*unsupported)++;
                continue;
            }
        }
        if (!rcVectorize(&image, options->colors, options->tolerance, &vector)) {
            goto done;
        }
        free(image.rgba);
        image.rgba = NULL;
        {
            /* TRANSFER SVG OWNERSHIP*/
            /* Once registered, the common cleanup path owns this sidecar even if the PDF later*/
            /* fails.*/
            RcBuffer *more = realloc(*images, (*imageCount + 1) * sizeof(**images));
            if (!more) {
                goto done;
            }
            *images = more;
            (*images)[*imageCount] = vector.svg;
            vector.svg = (RcBuffer){0};
            (*imageCount)++;
        }

        /* ENCODE THE REPLACEMENT FORM*/
        /* Compress drawing commands separately from the SVG representation.*/
        if (!rcDeflate(vector.pdf.data, vector.pdf.size, &compressed)) {
            goto done;
        }
        if (!rcAppend(
                &pdfObject->replacement,
                "<< /Type /XObject /Subtype /Form /FormType 1 /BBox [0 0 1 1] /Resources << ",
                sizeof(
                    "<< /Type /XObject /Subtype /Form /FormType 1 /BBox [0 0 1 1] /Resources << ") -
                    1) ||
            !rcAppend(&pdfObject->replacement, vector.resources.data, vector.resources.size)) {
            goto done;
        }
        if (!rcAppend(&pdfObject->replacement, ">> /Filter /FlateDecode /Length ",
                      sizeof(">> /Filter /FlateDecode /Length ") - 1) ||
            !appendUnsigned(&pdfObject->replacement, (unsigned long)compressed.size,
                            (PdfMinimumDigits){0}) ||
            !rcAppend(&pdfObject->replacement, " >>\nstream\n", sizeof(" >>\nstream\n") - 1) ||
            !rcAppend(&pdfObject->replacement, compressed.data, compressed.size) ||
            !rcAppend(&pdfObject->replacement, "\nendstream\n", PDF_ENDSTREAM_TEXT_LENGTH)) {
            goto done;
        }
        free(compressed.data);
        compressed = (RcBuffer){0};
        rcFreeVector(&vector);
    }

    /* SELECT AN UNCHANGED OR REBUILT CANDIDATE*/
    if (!*imageCount) {
        successStatus = 1;
        goto done;
    }

    /* Validate regenerated object offsets before exposing the PDF candidate.*/
    successStatus = serialize(&pdf, candidate);

    /* REPARSE THE GENERATED CROSS-REFERENCE TABLE*/
    if (successStatus) {
        Pdf check = {0};
        check.sourceBytes = candidate->data;
        check.sourceLength = candidate->size;
        successStatus = parsePdf(&check) && !check.unsupported;
        free(check.objects);
    }
done:
    /* RELEASE THE CURRENT IMAGE AND DOCUMENT*/
    /* Partial form construction uses the same cleanup path as a complete image.*/
    free(image.rgba);
    free(compressed.data);
    rcFreeVector(&vector);
    if (pdf.objects) {
        for (itemIndex = 0; itemIndex < pdf.count; itemIndex++) {
            free(pdf.objects[itemIndex].replacement.data);
        }
        free(pdf.objects);
    }
    if (!successStatus) {
        for (itemIndex = 0; itemIndex < *imageCount; itemIndex++) {
            free((*images)[itemIndex].data);
        }
        free(*images);
        *images = NULL;
        *imageCount = 0;
    }
    return successStatus;
}
