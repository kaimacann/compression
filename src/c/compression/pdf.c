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
    PDF_XREF_KEYWORD_LENGTH = 4,
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
    PDF_RGB_LOOKUP_CHANNELS = 3,
    PDF_RGB_LOOKUP_CAPACITY = 768,
    PDF_COLOR_KEY_RANGE_COUNT = 6,
    PDF_SERIALIZE_TEXT_CAPACITY = 128,
    PDF_REPLACEMENT_TEXT_CAPACITY = 256,
    PDF_XREF_OFFSET_WIDTH = 10,
    PDF_XREF_GENERATION_WIDTH = 5,
    PDF_HEADER_BYTES_LENGTH = 15,
    PDF_END_OBJECT_TEXT_LENGTH = 8,
    PDF_TRAILER_TEXT_LENGTH = 8,
    PDF_ENDSTREAM_TEXT_LENGTH = 11,
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
    size_t cursor;
    size_t value;
} ParsedUnsigned;
typedef struct {
    unsigned objectId;
    unsigned generation;
} PdfReference;
typedef struct {
    unsigned width;
    unsigned height;
    unsigned channels;
    unsigned bitsPerSample;
} PredictorShape;
typedef enum {
    PDF_CONTAINER_ARRAY,
    PDF_CONTAINER_DICTIONARY,
} PdfContainerKind;
typedef struct {
    unsigned depth;
    PdfContainerKind kind;
} ContainerContext;
typedef struct {
    size_t cursor;
    size_t trailer;
    size_t xref;
} XrefIndex;
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
typedef struct {
    Slice source;
    RcBuffer *output;
    size_t cursor;
    unsigned long value;
    unsigned count;
    int ended;
} Ascii85State;
typedef struct {
    unsigned long value;
    size_t outputLength;
} Ascii85Emission;

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
 * @param parsed Cursor advanced past the integer and parsed nonnegative value.
 * @return 1 when a bounded integer is parsed; otherwise 0.
 * @author kai
 */
static int uintAt(const unsigned char *sourceBytes, size_t sourceLength, ParsedUnsigned *parsed) {
    size_t loopIndex = space(sourceBytes, sourceLength, parsed->cursor);
    size_t value = 0;
    size_t start = loopIndex;
    while (loopIndex < sourceLength && sourceBytes[loopIndex] >= '0' &&
           sourceBytes[loopIndex] <= '9') {
        unsigned decimalDigit = sourceBytes[loopIndex++] - '0';
        if (value > ((size_t)-1 - decimalDigit) / PDF_DECIMAL_RADIX) {
            return 0;
        }
        value = (value * PDF_DECIMAL_RADIX) + decimalDigit;
    }
    if (loopIndex == start || (loopIndex < sourceLength && !delimiter(sourceBytes[loopIndex]))) {
        return 0;
    }
    parsed->cursor = loopIndex;
    parsed->value = value;
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
                       unsigned depth);

/**
 * Find the end of a nested PDF literal string while accounting for escaped bytes.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Cursor at the opening parenthesis.
 * @return Offset after the string, or sourceLength + 1 when unclosed.
 * @author kai
 */
static size_t literalStringEnd(const unsigned char *sourceBytes, size_t sourceLength,
                               size_t cursor) {
    unsigned nesting = 1;
    cursor++;
    while (cursor < sourceLength && nesting) {
        if (sourceBytes[cursor] == '\\') {
            cursor++;
            if (cursor < sourceLength) {
                if (sourceBytes[cursor] == '\r' && cursor + 1 < sourceLength &&
                    sourceBytes[cursor + 1] == '\n') {
                    cursor++;
                }
                cursor++;
            }
        } else {
            if (sourceBytes[cursor] == '(') {
                nesting++;
            } else if (sourceBytes[cursor] == ')') {
                nesting--;
            }
            cursor++;
        }
    }
    return nesting ? sourceLength + 1 : cursor;
}

/**
 * Find the end of a PDF hexadecimal string.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Cursor at the opening angle bracket.
 * @return Offset after the string, or sourceLength + 1 when unclosed.
 * @author kai
 */
static size_t hexStringEnd(const unsigned char *sourceBytes, size_t sourceLength, size_t cursor) {
    cursor++;
    while (cursor < sourceLength && sourceBytes[cursor] != '>') {
        cursor++;
    }
    return cursor < sourceLength ? cursor + 1 : sourceLength + 1;
}

/**
 * Find the end of a PDF array or dictionary, recursively bounding its values.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Cursor at the opening delimiter.
 * @param context Nesting depth and array/dictionary kind.
 * @return Offset after the container, or sourceLength + 1 if malformed.
 * @author kai
 */
static size_t containerEnd(const unsigned char *sourceBytes, size_t sourceLength, size_t cursor,
                           ContainerContext context) {
    int dictionary = context.kind == PDF_CONTAINER_DICTIONARY;
    cursor += dictionary ? 2U : 1U;
    for (;;) {
        cursor = space(sourceBytes, sourceLength, cursor);
        if (cursor >= sourceLength) {
            return sourceLength + 1;
        }
        if ((!dictionary && sourceBytes[cursor] == ']') ||
            (dictionary && cursor + 1 < sourceLength && sourceBytes[cursor] == '>' &&
             sourceBytes[cursor + 1] == '>')) {
            return cursor + (dictionary ? 2U : 1U);
        }
        cursor = valueEnd(sourceBytes, sourceLength, cursor, context.depth + 1);
        if (cursor > sourceLength) {
            return cursor;
        }
    }
}

/**
 * Find the end of a scalar PDF token or a two-integer indirect reference.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Cursor at the first token byte.
 * @return Offset after the scalar/reference, or sourceLength + 1 if malformed.
 * @author kai
 */
static size_t scalarEnd(const unsigned char *sourceBytes, size_t sourceLength, size_t cursor) {
    size_t start = cursor;
    if (sourceBytes[cursor] == '/') {
        cursor++;
    } else if (strchr("]>)", sourceBytes[cursor])) {
        return sourceLength + 1;
    }
    while (cursor < sourceLength && !delimiter(sourceBytes[cursor])) {
        cursor++;
    }
    if (cursor == start) {
        return sourceLength + 1;
    }
    if (sourceBytes[start] >= '0' && sourceBytes[start] <= '9') {
        ParsedUnsigned parsed = {cursor, 0};
        if (uintAt(sourceBytes, sourceLength, &parsed)) {
            size_t next = space(sourceBytes, sourceLength, parsed.cursor);
            if (token(sourceBytes, sourceLength, next, "R")) {
                return next + 1;
            }
        }
    }
    return cursor;
}

/**
 * Find the end of one PDF value while bounding nested arrays and dictionaries.
 * @param sourceBytes PDF source bytes.
 * @param sourceLength Source length.
 * @param cursor Offset at the first byte of the value.
 * @param depth Recursion depth, bounded to prevent excessive nesting.
 * @return Offset after the value, or sourceLength + 1 for malformed input.
 * @author kai
 */
static size_t valueEnd(const unsigned char *sourceBytes, size_t sourceLength, size_t cursor,
                       unsigned depth) {
    cursor = space(sourceBytes, sourceLength, cursor);
    if (depth > PDF_MAX_NESTING_DEPTH || cursor == sourceLength) {
        return sourceLength + 1;
    }
    if (sourceBytes[cursor] == '(') {
        return literalStringEnd(sourceBytes, sourceLength, cursor);
    }
    if (sourceBytes[cursor] == '<' && cursor + 1 < sourceLength && sourceBytes[cursor + 1] != '<') {
        return hexStringEnd(sourceBytes, sourceLength, cursor);
    }
    if (sourceBytes[cursor] == '[' || (sourceBytes[cursor] == '<' && cursor + 1 < sourceLength &&
                                       sourceBytes[cursor + 1] == '<')) {
        int dictionary = sourceBytes[cursor] == '<';
        return containerEnd(
            sourceBytes, sourceLength, cursor,
            (ContainerContext){depth, dictionary ? PDF_CONTAINER_DICTIONARY : PDF_CONTAINER_ARRAY});
    }
    return scalarEnd(sourceBytes, sourceLength, cursor);
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
    ParsedUnsigned parsed;
    if (objectId >= pdf->count || !pdf->objects[objectId].live) {
        return 0;
    }
    cursor = pdf->objects[objectId].offset;
    parsed = (ParsedUnsigned){cursor, 0};
    if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed) || parsed.value != objectId) {
        return 0;
    }
    cursor = parsed.cursor;
    parsed = (ParsedUnsigned){cursor, 0};
    if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed) ||
        parsed.value != pdf->objects[objectId].generation) {
        return 0;
    }
    cursor = parsed.cursor;
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
 * @param result Receives the referenced object and generation numbers.
 * @return 1 for a valid bounded reference; otherwise 0.
 * @author kai
 */
static int reference(Slice valueSlice, PdfReference *result) {
    ParsedUnsigned parsed = {0, 0};
    size_t firstNumber;
    size_t secondNumber;
    if (!uintAt(valueSlice.sourceBytes, valueSlice.sourceLength, &parsed)) {
        return 0;
    }
    firstNumber = parsed.value;
    parsed.value = 0;
    if (!uintAt(valueSlice.sourceBytes, valueSlice.sourceLength, &parsed)) {
        return 0;
    }
    secondNumber = parsed.value;
    if (firstNumber > PDF_MAX_OBJECT_COUNT || secondNumber > PDF_MAX_GENERATION) {
        return 0;
    }
    parsed.cursor = space(valueSlice.sourceBytes, valueSlice.sourceLength, parsed.cursor);
    if (!token(valueSlice.sourceBytes, valueSlice.sourceLength, parsed.cursor, "R")) {
        return 0;
    }
    result->objectId = (unsigned)firstNumber;
    result->generation = (unsigned)secondNumber;
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
    PdfReference referenceValue;
    unsigned depth;
    for (depth = 0; depth < PDF_MAX_REFERENCE_DEPTH; depth++) {
        if (!reference(*valueSlice, &referenceValue)) {
            return 1;
        }
        if (referenceValue.objectId >= pdf->count ||
            pdf->objects[referenceValue.objectId].generation != referenceValue.generation ||
            !objectValue(pdf, referenceValue.objectId, valueSlice)) {
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
    ParsedUnsigned parsed = {0, 0};
    if (!resolve(pdf, &valueSlice) ||
        !uintAt(valueSlice.sourceBytes, valueSlice.sourceLength, &parsed) ||
        space(valueSlice.sourceBytes, valueSlice.sourceLength, parsed.cursor) !=
            valueSlice.sourceLength) {
        return 0;
    }
    *parsedValue = parsed.value;
    return 1;
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
    PdfReference referenceValue;
    if (dictValue(pdfObject->value, key, &valueSlice) != 1 ||
        !reference(valueSlice, &referenceValue) || referenceValue.objectId >= pdf->count ||
        !pdf->objects[referenceValue.objectId].live ||
        pdf->objects[referenceValue.objectId].generation != referenceValue.generation) {
        return 0;
    }
    *objectId = referenceValue.objectId;
    return 1;
}
/* CROSS-REFERENCE VALIDATION*/

/* CROSS REFERENCES*/
/*
 * Index classic PDF objects and trailer data; flag structures that cannot be safely rewritten.
 * @param pdf Document state to populate from the source bytes.
 * @return 1 when the PDF structure is accepted; otherwise 0.
 * @author kai
 */
/**
 * Locate the active classic cross-reference table from the PDF trailer marker.
 * @param pdf Document source bytes.
 * @param xref Receives the active xref byte offset.
 * @return 1 for a supported table, 0 for malformed input, or 2 for unsupported xref streams.
 * @author kai
 */
static int locateXref(Pdf *pdf, size_t *xref) {
    size_t cursor;
    size_t itemIndex;
    int found = 0;
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
    for (itemIndex = cursor - PDF_EOF_MARKER_LENGTH; itemIndex > 0; itemIndex--) {
        if (token(pdf->sourceBytes, pdf->sourceLength, itemIndex, "startxref")) {
            ParsedUnsigned parsed = {itemIndex + PDF_STARTXREF_KEYWORD_LENGTH, 0};
            if (uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
                *xref = parsed.value;
                found = 1;
                break;
            }
        }
    }
    if (!found || *xref >= pdf->sourceLength) {
        return 0;
    }
    if (!token(pdf->sourceBytes, pdf->sourceLength, *xref, "xref")) {
        pdf->unsupported = 1;
        return 2;
    }
    return 1;
}

/**
 * Traverse classic cross-reference sections and find the trailer dictionary.
 * @param pdf Document whose xref bytes are being read.
 * @param xref Byte offset after the xref keyword.
 * @param trailer Receives the trailer dictionary byte offset.
 * @return Nonzero for a complete xref traversal.
 * @author kai
 */
static int findTrailer(Pdf *pdf, size_t xref, size_t *trailer) {
    size_t scan = xref;
    for (;;) {
        ParsedUnsigned parsed;
        size_t first;
        size_t sourceLength;
        size_t entryIndex;
        scan = space(pdf->sourceBytes, pdf->sourceLength, scan);
        if (token(pdf->sourceBytes, pdf->sourceLength, scan, "trailer")) {
            *trailer =
                space(pdf->sourceBytes, pdf->sourceLength, scan + PDF_TRAILER_KEYWORD_LENGTH);
            return 1;
        }
        parsed = (ParsedUnsigned){scan, 0};
        if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
            return 0;
        }
        first = parsed.value;
        parsed.value = 0;
        if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
            return 0;
        }
        sourceLength = parsed.value;
        scan = parsed.cursor;
        if (first > PDF_MAX_OBJECT_COUNT || sourceLength > PDF_MAX_OBJECT_COUNT - first) {
            return 0;
        }
        for (entryIndex = 0; entryIndex < sourceLength; entryIndex++) {
            parsed = (ParsedUnsigned){scan, 0};
            if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
                return 0;
            }
            parsed.value = 0;
            if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
                return 0;
            }
            scan = space(pdf->sourceBytes, pdf->sourceLength, parsed.cursor);
            if (scan >= pdf->sourceLength ||
                (pdf->sourceBytes[scan] != 'n' && pdf->sourceBytes[scan] != 'f')) {
                return 0;
            }
            scan++;
        }
    }
}

/**
 * Parse and validate trailer capabilities, allocating the declared object table.
 * @param pdf Document whose trailer is validated.
 * @param trailer Trailer dictionary byte offset.
 * @param count Receives the validated number of object slots.
 * @return 1 for a supported trailer, 2 for an unsupported feature, or 0 if malformed.
 * @author kai
 */
static int prepareTrailer(Pdf *pdf, size_t trailer, size_t *count) {
    Slice valueSlice;
    size_t trailerEnd = valueEnd(pdf->sourceBytes, pdf->sourceLength, trailer, 0);
    if (trailerEnd > pdf->sourceLength) {
        return 0;
    }
    pdf->trailer = (Slice){pdf->sourceBytes + trailer, trailerEnd - trailer};
    if (dictValue(pdf->trailer, "/Size", &valueSlice) != 1 || !sizeValue(pdf, valueSlice, count) ||
        !*count || *count > PDF_MAX_OBJECT_COUNT) {
        return 0;
    }
    pdf->count = (unsigned)*count;
    if (dictValue(pdf->trailer, "/Prev", &valueSlice) == 1 ||
        dictValue(pdf->trailer, "/XRefStm", &valueSlice) == 1 ||
        dictValue(pdf->trailer, "/Encrypt", &valueSlice) == 1) {
        pdf->unsupported = 1;
        return 2;
    }
    pdf->objects = calloc(*count, sizeof(*pdf->objects));
    return pdf->objects ? 1 : 0;
}

/**
 * Validate and store one classic xref entry in the object table.
 * @param pdf Document receiving object metadata.
 * @param index Mutable xref cursor and active table offset.
 * @param objectId Object slot to update.
 * @return Nonzero when the entry is valid.
 * @author kai
 */
static int readXrefEntry(Pdf *pdf, XrefIndex *index, size_t objectId) {
    ParsedUnsigned parsed = {index->cursor, 0};
    size_t offset;
    size_t generationValue;
    PdfObject *pdfObject = &pdf->objects[objectId];
    if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
        return 0;
    }
    offset = parsed.value;
    parsed.value = 0;
    if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
        return 0;
    }
    generationValue = parsed.value;
    if (generationValue > PDF_MAX_GENERATION) {
        return 0;
    }
    index->cursor = space(pdf->sourceBytes, pdf->sourceLength, parsed.cursor);
    if (index->cursor >= pdf->sourceLength ||
        (pdf->sourceBytes[index->cursor] != 'n' && pdf->sourceBytes[index->cursor] != 'f')) {
        return 0;
    }
    pdfObject->live = pdf->sourceBytes[index->cursor] == 'n';
    pdfObject->offset = offset;
    pdfObject->generation = (unsigned)generationValue;
    index->cursor++;
    return !pdfObject->live || offset < index->xref;
}

/**
 * Parse one xref subsection header and store each entry in the object table.
 * @param pdf Document receiving object metadata.
 * @param index Mutable xref cursor and active table offset.
 * @return Nonzero when the subsection range and entries are valid.
 * @author kai
 */
static int readXrefSubsection(Pdf *pdf, XrefIndex *index) {
    ParsedUnsigned parsed = {index->cursor, 0};
    size_t first;
    size_t sourceLength;
    size_t entryIndex;
    if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
        return 0;
    }
    first = parsed.value;
    parsed.value = 0;
    if (!uintAt(pdf->sourceBytes, pdf->sourceLength, &parsed)) {
        return 0;
    }
    sourceLength = parsed.value;
    index->cursor = parsed.cursor;
    if (first >= pdf->count || sourceLength > pdf->count - first) {
        return 0;
    }
    for (entryIndex = 0; entryIndex < sourceLength; entryIndex++) {
        if (!readXrefEntry(pdf, index, first + entryIndex)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Read each classic xref subsection into the allocated object table.
 * @param pdf Document receiving indexed object offsets.
 * @param index Mutable subsection cursor and validated trailer/xref offsets.
 * @return Nonzero when all subsection entries are valid.
 * @author kai
 */
static int indexXrefObjects(Pdf *pdf, XrefIndex *index) {
    while ((index->cursor = space(pdf->sourceBytes, pdf->sourceLength, index->cursor)) <
           index->trailer) {
        if (token(pdf->sourceBytes, pdf->sourceLength, index->cursor, "trailer")) {
            break;
        }
        if (!readXrefSubsection(pdf, index)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Resolve and validate a stream body's line ending, declared length, and terminator.
 * @param pdf Document source and object table.
 * @param pdfObject Object whose stream boundary is read.
 * @param position Cursor at the stream keyword; receives the position after endstream.
 * @return Nonzero when the bounded stream is valid.
 * @author kai
 */
static int validateStreamBoundary(Pdf *pdf, PdfObject *pdfObject, size_t *position) {
    *position += PDF_STREAM_KEYWORD_LENGTH;
    if (*position >= pdf->sourceLength) {
        return 0;
    }
    if (pdf->sourceBytes[*position] == '\r') {
        (*position)++;
        if (*position < pdf->sourceLength && pdf->sourceBytes[*position] == '\n') {
            (*position)++;
        }
    } else if (pdf->sourceBytes[*position] == '\n') {
        (*position)++;
    } else {
        return 0;
    }
    pdfObject->stream = *position;
    if (!getSize(pdf, pdfObject, "/Length", &pdfObject->length) ||
        pdfObject->length > pdf->sourceLength - *position) {
        return 0;
    }
    *position = space(pdf->sourceBytes, pdf->sourceLength, *position + pdfObject->length);
    if (!token(pdf->sourceBytes, pdf->sourceLength, *position, "endstream")) {
        return 0;
    }
    *position =
        space(pdf->sourceBytes, pdf->sourceLength, *position + PDF_ENDSTREAM_KEYWORD_LENGTH);
    return 1;
}

/**
 * Validate one live object and report structures that require unchanged-source fallback.
 * @param pdf Document with indexed object offsets.
 * @param objectIndex Index of the live object.
 * @return 1 when valid, 2 when unsupported, or 0 when malformed.
 * @author kai
 */
static int validateOneObject(Pdf *pdf, size_t objectIndex) {
    PdfObject *pdfObject = &pdf->objects[objectIndex];
    Slice valueSlice;
    size_t position;
    if (!objectValue(pdf, (unsigned)objectIndex, &pdfObject->value)) {
        return 0;
    }
    position = space(pdf->sourceBytes, pdf->sourceLength,
                     (size_t)(pdfObject->value.sourceBytes - pdf->sourceBytes) +
                         pdfObject->value.sourceLength);
    if (token(pdf->sourceBytes, pdf->sourceLength, position, "stream") &&
        !validateStreamBoundary(pdf, pdfObject, &position)) {
        return 0;
    }
    if (!token(pdf->sourceBytes, pdf->sourceLength, position, "endobj")) {
        return 0;
    }
    pdfObject->end = pdf->sourceBytes + position;
    if (dictValue(pdfObject->value, "/ByteRange", &valueSlice) == 1 ||
        (dictValue(pdfObject->value, "/Type", &valueSlice) == 1 &&
         (nameIs(valueSlice, "/ObjStm") || nameIs(valueSlice, "/XRef")))) {
        pdf->unsupported = 1;
        return 2;
    }
    return 1;
}

/**
 * Validate the boundaries and stream ranges for every live indirect object.
 * @param pdf Document with indexed object offsets.
 * @return 1 for valid objects, 2 for unsupported structures, otherwise 0.
 * @author kai
 */
static int validateLiveObjects(Pdf *pdf) {
    size_t itemIndex;
    for (itemIndex = 1; itemIndex < pdf->count; itemIndex++) {
        if (pdf->objects[itemIndex].live) {
            int status = validateOneObject(pdf, itemIndex);
            if (status != 1) {
                return status;
            }
        }
    }
    return 1;
}

/**
 * Validate the PDF cross-reference table, trailer, and live object boundaries.
 * @param pdf Document state to populate from source bytes.
 * @return 1 when the document is valid or safely unsupported; otherwise 0.
 * @author kai
 */
static int parsePdf(Pdf *pdf) {
    size_t cursor;
    size_t xref = 0;
    size_t trailer = 0;
    XrefIndex index;
    size_t count;
    int status = locateXref(pdf, &xref);
    if (status != 1) {
        return status == 2;
    }
    if (!findTrailer(pdf, xref + PDF_XREF_KEYWORD_LENGTH, &trailer)) {
        return 0;
    }
    status = prepareTrailer(pdf, trailer, &count);
    if (status != 1) {
        return status == 2;
    }
    cursor = xref + PDF_XREF_KEYWORD_LENGTH;
    index = (XrefIndex){cursor, trailer, xref};
    if (!indexXrefObjects(pdf, &index)) {
        return 0;
    }
    status = validateLiveObjects(pdf);
    return status == 2 || status == 1;
}

/*//////////////////////////////////////////////////
IMAGES
//////////////////////////////////////////////////*/

/* FILTERS*/
/**
 * Decode an ASCII85 PDF string.
 * @param valueSlice Encoded bytes.
 * @param out Receives decoded bytes.
 * @return Nonzero when decoding succeeds.
 * @author kai
 */
static int ascii85(Slice valueSlice, RcBuffer *out);
/**
 * Decode an ASCIIHex PDF string.
 * @param valueSlice Encoded bytes.
 * @param out Receives decoded bytes.
 * @return Nonzero when decoding succeeds.
 * @author kai
 */
static int asciiHex(Slice valueSlice, RcBuffer *out);

/**
 * Resolve and collect the ordered filter names from a PDF object.
 * @param pdf Document resolving indirect filter values.
 * @param pdfObject Stream object containing /Filter.
 * @param items Receives ordered filter-name slices.
 * @return Number of filters, or -1 for an invalid or excessive list.
 * @author kai
 */
static int collectFilterItems(Pdf *pdf, const PdfObject *pdfObject,
                              Slice items[PDF_MAX_FILTER_COUNT]) {
    Slice filter;
    unsigned count = 0;
    if (dictValue(pdfObject->value, "/Filter", &filter) != 1) {
        return 0;
    }
    if (!resolve(pdf, &filter)) {
        return -1;
    }
    {
        size_t cursor = filter.sourceLength && filter.sourceBytes[0] == '[' ? 1 : 0;
        while ((cursor = space(filter.sourceBytes, filter.sourceLength, cursor)) <
                   filter.sourceLength &&
               filter.sourceBytes[cursor] != ']') {
            size_t end = valueEnd(filter.sourceBytes, filter.sourceLength, cursor, 0);
            if (end > filter.sourceLength || count == PDF_MAX_FILTER_COUNT) {
                return -1;
            }
            items[count].sourceBytes = filter.sourceBytes + cursor;
            items[count++].sourceLength = end - cursor;
            cursor = end;
        }
    }
    return (int)count;
}

typedef struct {
    unsigned index;
    unsigned count;
    int *jpeg;
} FilterStep;

/**
 * Decode one PDF stream filter, preserving DCT data for the JPEG decoder.
 * @param filter Filter name to apply.
 * @param current Current stream bytes.
 * @param next Receives decoded bytes.
 * @param step Filter position and JPEG output flag.
 * @return 1 when decoded, 0 when unsupported, or -1 when malformed.
 * @author kai
 */
static int decodeFilterItem(Slice filter, const RcBuffer *current, RcBuffer *next,
                            FilterStep step) {
    Slice bytes = {current->data, current->size};
    int supported = 0;
    int known = 1;
    if (nameIs(filter, "/FlateDecode") || nameIs(filter, "/Fl")) {
        supported = rcInflate(current->data, current->size, next);
    } else if (nameIs(filter, "/ASCII85Decode") || nameIs(filter, "/A85")) {
        supported = ascii85(bytes, next);
    } else if (nameIs(filter, "/ASCIIHexDecode") || nameIs(filter, "/AHx")) {
        supported = asciiHex(bytes, next);
    } else if ((nameIs(filter, "/DCTDecode") || nameIs(filter, "/DCT")) &&
               step.index + 1 == step.count) {
        *step.jpeg = 1;
        return 2;
    } else {
        known = 0;
    }
    if (supported) {
        return 1;
    }
    return known ? -1 : 0;
}

/**
 * Append the high-order bytes of a completed ASCII85 tuple.
 * @param state Decoder state containing the destination buffer.
 * @param emission Tuple value and number of bytes to append.
 * @return Nonzero when the value fits and output append succeeds.
 * @author kai
 */
static int appendAscii85Value(Ascii85State *state, Ascii85Emission emission) {
    unsigned char bytes[PDF_ASCII85_TUPLE_DIGITS - 1];
    unsigned byteIndex;
    if (emission.value > UINT32_MAX) {
        return 0;
    }
    for (byteIndex = 0; byteIndex < PDF_ASCII85_TUPLE_DIGITS - 1; byteIndex++) {
        bytes[byteIndex] =
            (unsigned char)(emission.value >> ((3U - byteIndex) * (unsigned)PDF_BITS_PER_BYTE));
    }
    return rcAppend(state->output, bytes, emission.outputLength);
}

/**
 * Pad and append a final partial ASCII85 tuple.
 * @param state Decoder state containing the output destination and partial tuple.
 * @return Nonzero when any partial bytes are valid and appended.
 * @author kai
 */
static int appendAscii85Remainder(Ascii85State *state) {
    unsigned originalCount = state->count;
    if (!state->count) {
        return 1;
    }
    if (state->count == 1) {
        return 0;
    }
    while (state->count++ < PDF_ASCII85_TUPLE_DIGITS) {
        state->value = (state->value * PDF_ASCII85_RADIX) + PDF_ASCII85_PADDING_DIGIT;
    }
    return appendAscii85Value(state, (Ascii85Emission){state->value, originalCount - 1});
}

/**
 * Consume one non-whitespace ASCII85 byte and update its decoder state.
 * @param state Source, destination, cursor, and tuple state for ASCII85 decoding.
 * @return Zero for malformed input, one for a consumed byte, or two at the terminator.
 * @author kai
 */
static int consumeAscii85Byte(Ascii85State *state) {
    unsigned byteValue = state->source.sourceBytes[state->cursor];
    if (white((unsigned char)byteValue)) {
        state->cursor++;
        return 1;
    }
    if (byteValue == '~') {
        if (state->cursor + 1 >= state->source.sourceLength ||
            state->source.sourceBytes[state->cursor + 1] != '>') {
            return 0;
        }
        state->cursor += 2;
        state->ended = 1;
        return 2;
    }
    if (byteValue == 'z') {
        unsigned char zeros[PDF_ASCII85_TUPLE_DIGITS - 1] = {0};
        if (state->count || !rcAppend(state->output, zeros, sizeof(zeros))) {
            return 0;
        }
        state->cursor++;
        return 1;
    }
    if (byteValue < PDF_ASCII85_MINIMUM || byteValue > PDF_ASCII85_MAXIMUM) {
        return 0;
    }
    state->value = (state->value * PDF_ASCII85_RADIX) + byteValue - PDF_ASCII85_MINIMUM;
    state->count++;
    state->cursor++;
    if (state->count == PDF_ASCII85_TUPLE_DIGITS) {
        if (!appendAscii85Value(state,
                                (Ascii85Emission){state->value, PDF_ASCII85_TUPLE_DIGITS - 1})) {
            return 0;
        }
        state->count = 0;
        state->value = 0;
    }
    return 1;
}

/**
 * Decode an ASCII85 stream into the output buffer.
 * @param valueSlice Encoded ASCII85 stream slice.
 * @param out Receives decoded stream bytes.
 * @return 1 for a complete valid stream; otherwise 0 for malformed input or allocation failure.
 * @author kai
 */
static int ascii85(Slice valueSlice, RcBuffer *out) {
    Ascii85State state = {valueSlice, out, 0, 0, 0, 0};
    int consumed;
    while (state.cursor < state.source.sourceLength && !state.ended) {
        consumed = consumeAscii85Byte(&state);
        if (!consumed) {
            return 0;
        }
    }
    if (!state.ended) {
        return 0;
    }
    while (state.cursor < state.source.sourceLength) {
        if (!white(state.source.sourceBytes[state.cursor++])) {
            return 0;
        }
    }
    return appendAscii85Remainder(&state);
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
    Slice items[PDF_MAX_FILTER_COUNT];
    int collectedCount;
    unsigned count;
    unsigned itemIndex;
    RcBuffer current = {0};
    int successStatus = 0;
    *jpeg = 0;

    /* RESOLVE THE ORDERED FILTER LIST*/
    collectedCount = collectFilterItems(pdf, pdfObject, items);
    if (collectedCount < 0) {
        return 0;
    }
    count = (unsigned)collectedCount;
    if (!rcAppend(&current, pdf->sourceBytes + pdfObject->stream, pdfObject->length)) {
        goto done;
    }

    /* DECODE FILTERS IN DECLARED ORDER*/
    /* DCT remains encoded until the shared JPEG decoder receives its byte stream.*/
    for (itemIndex = 0; itemIndex < count; itemIndex++) {
        RcBuffer next = {0};
        int result = decodeFilterItem(items[itemIndex], &current, &next,
                                      (FilterStep){itemIndex, count, jpeg});
        if (result == 2) {
            break;
        }

        /* DISTINGUISH UNSUPPORTED DATA FROM CORRUPTION*/
        if (result != 1) {
            free(next.data);
            successStatus = result;
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
typedef struct {
    size_t predictor;
    size_t rowBytes;
    size_t bytesPerPixel;
} PredictorLayout;

/**
 * Select the first dictionary from an optional DecodeParms array.
 * @param parameters DecodeParms value, possibly replaced with its first dictionary.
 * @return 1 when a dictionary is selected, 0 when the array has none, or -1 when malformed.
 * @author kai
 */
static int selectPredictorDictionary(Slice *parameters) {
    size_t cursor = 1;
    while ((cursor = space(parameters->sourceBytes, parameters->sourceLength, cursor)) <
               parameters->sourceLength &&
           parameters->sourceBytes[cursor] != ']') {
        size_t end = valueEnd(parameters->sourceBytes, parameters->sourceLength, cursor, 0);
        if (end > parameters->sourceLength) {
            return -1;
        }
        if (parameters->sourceBytes[cursor] == '<') {
            *parameters = (Slice){parameters->sourceBytes + cursor, end - cursor};
            return 1;
        }
        cursor = end;
    }
    return 0;
}

/**
 * Read an optional positive integer from predictor parameters.
 * @param pdf Document resolving an indirect integer value.
 * @param parameters Predictor dictionary.
 * @param key Dictionary key.
 * @param value Receives the decoded integer when the key exists.
 * @return Nonzero when the key is absent or has a valid integer value.
 * @author kai
 */
static int readOptionalPredictorSize(Pdf *pdf, Slice parameters, const char *key, size_t *value) {
    Slice item;
    return dictValue(parameters, key, &item) != 1 || sizeValue(pdf, item, value);
}

/**
 * Resolve predictor settings and verify they match decoded image geometry.
 * @param pdf Document resolving indirect predictor values.
 * @param pdfObject Image stream with DecodeParms.
 * @param shape Image dimensions and sample layout.
 * @param layout Receives predictor type, row length, and bytes per pixel.
 * @return Nonzero when settings are valid.
 * @author kai
 */
static int resolvePredictorLayout(Pdf *pdf, PdfObject *pdfObject, PredictorShape shape,
                                  PredictorLayout *layout) {
    Slice parameters;
    layout->predictor = 1;
    if (dictValue(pdfObject->value, "/DecodeParms", &parameters) != 1) {
        return 1;
    }
    if (!resolve(pdf, &parameters)) {
        return 0;
    }
    if (parameters.sourceLength && parameters.sourceBytes[0] == '[') {
        int selected = selectPredictorDictionary(&parameters);
        if (selected < 0) {
            return 0;
        }
        if (!selected) {
            return 1;
        }
    }
    if (parameters.sourceLength < 2 || parameters.sourceBytes[0] != '<') {
        return nameIs(parameters, "null");
    }
    if (!readOptionalPredictorSize(pdf, parameters, "/Predictor", &layout->predictor)) {
        return 0;
    }
    if (layout->predictor == 1) {
        return 1;
    }
    {
        size_t columns = shape.width;
        size_t colors = shape.channels;
        size_t bitsPerComponent = shape.bitsPerSample;
        if (!readOptionalPredictorSize(pdf, parameters, "/Columns", &columns) ||
            !readOptionalPredictorSize(pdf, parameters, "/Colors", &colors) ||
            !readOptionalPredictorSize(pdf, parameters, "/BitsPerComponent", &bitsPerComponent) ||
            columns != shape.width || colors != shape.channels ||
            bitsPerComponent != shape.bitsPerSample) {
            return 0;
        }
    }
    layout->rowBytes =
        (((size_t)shape.width * shape.channels * shape.bitsPerSample) + (PDF_BITS_PER_BYTE - 1)) /
        PDF_BITS_PER_BYTE;
    layout->bytesPerPixel =
        ((shape.channels * shape.bitsPerSample) + (PDF_BITS_PER_BYTE - 1)) / PDF_BITS_PER_BYTE;
    return 1;
}

/**
 * Reverse TIFF horizontal differencing for one decoded image.
 * @param data Flate-decoded samples, updated in place.
 * @param shape Image dimensions and sample layout.
 * @param layout Predictor type and row geometry.
 * @return Nonzero when every sample row is valid and reconstructed.
 * @author kai
 */
static int reverseTiffPredictor(RcBuffer *data, PredictorShape shape, PredictorLayout layout) {
    size_t rowIndex;
    size_t columnIndex;
    unsigned bits = shape.bitsPerSample;
    unsigned channels = shape.channels;
    if (data->size != layout.rowBytes * shape.height) {
        return 0;
    }
    for (rowIndex = 0; rowIndex < shape.height; rowIndex++) {
        unsigned char *rowData = data->data + (rowIndex * layout.rowBytes);
        for (columnIndex = channels; columnIndex < (size_t)shape.width * channels; columnIndex++) {
            unsigned value = (rcSample(rowData, columnIndex, bits) +
                              rcSample(rowData, columnIndex - channels, bits)) &
                             ((1U << bits) - 1);
            if (bits == PDF_BITS_PER_BYTE) {
                rowData[columnIndex] = (unsigned char)value;
            } else if (bits == 2 * PDF_BITS_PER_BYTE) {
                rowData[columnIndex * 2] = (unsigned char)(value >> (unsigned)PDF_BITS_PER_BYTE);
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

/**
 * Reverse PNG byte filters in a predictor-encoded image.
 * @param data Flate-decoded filter bytes, updated in place.
 * @param shape Image dimensions and sample layout.
 * @param layout Predictor type and row geometry.
 * @return Nonzero when every row filter is supported.
 * @author kai
 */
static int reversePngPredictor(RcBuffer *data, PredictorShape shape, PredictorLayout layout) {
    size_t rowIndex;
    if (layout.predictor < PNG_PREDICTOR_MINIMUM || layout.predictor > PNG_PREDICTOR_MAXIMUM ||
        data->size != (layout.rowBytes + 1) * shape.height) {
        return 0;
    }
    for (rowIndex = 0; rowIndex < shape.height; rowIndex++) {
        unsigned filter = data->data[rowIndex * (layout.rowBytes + 1)];
        unsigned char *decoded = data->data + (rowIndex * layout.rowBytes);
        movePdfBytes(decoded, data->data + (rowIndex * (layout.rowBytes + 1)) + 1, layout.rowBytes);
        if (!rcUnfilter(decoded, rowIndex ? decoded - layout.rowBytes : NULL,
                        (RcUnfilterOptions){layout.rowBytes, layout.bytesPerPixel, filter})) {
            return 0;
        }
    }
    data->size = layout.rowBytes * shape.height;
    return 1;
}

/**
 * Reverse a PDF/PNG predictor using the image geometry and dictionary parameters.
 * @param pdf Document used to resolve DecodeParms values.
 * @param pdfObject Image object containing predictor parameters.
 * @param data Flate-decoded bytes, replaced with reconstructed samples.
 * @param shape Image width, height, channel count, and bits per sample.
 * @return 1 when samples are reconstructed or no predictor is present; otherwise 0.
 * @author kai
 */
static int predictor(Pdf *pdf, PdfObject *pdfObject, RcBuffer *data, PredictorShape shape) {
    PredictorLayout layout = {1, 0, 0};
    if (!resolvePredictorLayout(pdf, pdfObject, shape, &layout)) {
        return 0;
    }
    if (layout.predictor == 1) {
        return 1;
    }
    if (layout.predictor == 2) {
        return reverseTiffPredictor(data, shape, layout);
    }
    return reversePngPredictor(data, shape, layout);
}

/* IMAGE OBJECTS*/
typedef struct {
    size_t width;
    size_t height;
    size_t bits;
    unsigned channels;
    unsigned indexed;
    unsigned high;
} PdfImageSampleLayout;
typedef struct {
    unsigned channels;
    unsigned indexed;
    unsigned high;
    unsigned char lookup[PDF_RGB_LOOKUP_CAPACITY];
} PdfImageColor;
/**
 * Parse an optional PDF Decode array into per-channel output ranges.
 * @param pdf Document used to resolve indirect values.
 * @param pdfObject Image dictionary containing Decode.
 * @param layout PDF image sample layout.
 * @param ranges Receives the channel ranges.
 * @return Nonzero when the array is valid or absent.
 * @author kai
 */
static int readPdfDecodeRanges(Pdf *pdf, PdfObject *pdfObject, PdfImageSampleLayout layout,
                               double ranges[3][2]) {
    Slice decode = {0};
    size_t cursor = 1;
    unsigned channelIndex;
    if (dictValue(pdfObject->value, "/Decode", &decode) != 1) {
        return 1;
    }
    if (!resolve(pdf, &decode) || decode.sourceLength == 0 || decode.sourceBytes[0] != '[' ||
        layout.indexed) {
        return 0;
    }
    for (channelIndex = 0; channelIndex < layout.channels; channelIndex++) {
        unsigned endpoint;
        for (endpoint = 0; endpoint < 2; endpoint++) {
            char *end;
            cursor = space(decode.sourceBytes, decode.sourceLength, cursor);
            ranges[channelIndex][endpoint] =
                strtod((const char *)decode.sourceBytes + cursor, &end);
            if ((const unsigned char *)end == decode.sourceBytes + cursor ||
                (const unsigned char *)end > decode.sourceBytes + decode.sourceLength ||
                !isfinite(ranges[channelIndex][endpoint])) {
                return 0;
            }
            cursor = (size_t)((const unsigned char *)end - decode.sourceBytes);
        }
    }
    return 1;
}

/**
 * Expand one PDF pixel from packed samples and colour metadata.
 * @param pixel Receives one RGBA pixel.
 * @param rowData Packed sample row.
 * @param columnIndex Pixel index within the row.
 * @param layout PDF image sample layout.
 * @param lookup Indexed RGB palette.
 * @param ranges Direct colour decode ranges.
 * @return Nonzero when the pixel sample is valid.
 * @author kai
 */
static int expandPdfPixel(unsigned char *pixel, const unsigned char *rowData, size_t columnIndex,
                          PdfImageSampleLayout layout, const unsigned char *lookup,
                          double ranges[3][2]) {
    unsigned values[3] = {0};
    unsigned itemIndex;
    for (itemIndex = 0; itemIndex < layout.channels; itemIndex++) {
        values[itemIndex] =
            rcSample(rowData, (columnIndex * layout.channels) + itemIndex, (unsigned)layout.bits);
    }
    pixel[3] = PDF_SAMPLE_MAXIMUM;
    if (layout.indexed) {
        if (values[0] > layout.high) {
            return 0;
        }
        copyPdfBytes(pixel, lookup + ((size_t)values[0] * 3), 3);
        return 1;
    }
    for (itemIndex = 0; itemIndex < layout.channels; itemIndex++) {
        double value = (ranges[itemIndex][0] +
                        (values[itemIndex] * (ranges[itemIndex][1] - ranges[itemIndex][0]) /
                         ((1U << layout.bits) - 1))) *
                       PDF_SAMPLE_MAXIMUM;
        if (value < 0) {
            value = 0;
        }
        if (value > PDF_SAMPLE_MAXIMUM) {
            value = PDF_SAMPLE_MAXIMUM;
        }
        pixel[itemIndex] = (unsigned char)floor(value + PDF_SAMPLE_ROUNDING_BIAS);
    }
    if (layout.channels == 1) {
        pixel[1] = pixel[2] = pixel[0];
    }
    return 1;
}

/**
 * Expand unpacked PDF samples into the output RGBA image.
 * @param image Output image storage.
 * @param data Decoded packed samples.
 * @param layout PDF image sample layout.
 * @param lookup Indexed RGB palette.
 * @param ranges Direct colour decode ranges.
 * @return Nonzero when all samples are valid.
 * @author kai
 */
static int expandPdfSamples(RcImage *image, const RcBuffer *data, PdfImageSampleLayout layout,
                            const unsigned char *lookup, double ranges[3][2]) {
    size_t rowBytes = ((layout.width * layout.channels * layout.bits) + (PDF_BITS_PER_BYTE - 1)) /
                      PDF_BITS_PER_BYTE;
    size_t rowIndex;
    for (rowIndex = 0; rowIndex < layout.height; rowIndex++) {
        size_t columnIndex;
        for (columnIndex = 0; columnIndex < layout.width; columnIndex++) {
            unsigned char *pixel = image->rgba + (((rowIndex * layout.width) + columnIndex) * 4);
            const unsigned char *rowData = data->data + (rowIndex * rowBytes);
            if (!expandPdfPixel(pixel, rowData, columnIndex, layout, lookup, ranges)) {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Decode an indexed colour lookup from a stream reference or inline hex string.
 * @param pdf Document used to resolve lookup references.
 * @param entry Lookup object or hex value.
 * @param high Highest indexed colour.
 * @param lookup Receives RGB lookup bytes.
 * @return Nonzero when the lookup has the required number of bytes.
 * @author kai
 */
static int loadPdfLookup(Pdf *pdf, Slice entry, unsigned high,
                         unsigned char lookup[PDF_RGB_LOOKUP_CAPACITY]) {
    RcBuffer lut = {0};
    int decoded = 0;
    int jpegLookup = 0;
    PdfReference referenceValue;
    if (reference(entry, &referenceValue)) {
        unsigned objectId = referenceValue.objectId;
        decoded = objectId < pdf->count &&
                  decodeFilters(pdf, &pdf->objects[objectId], &lut, &jpegLookup) == 1 &&
                  !jpegLookup;
    } else if (entry.sourceLength >= 2 && entry.sourceBytes[0] == '<' &&
               entry.sourceBytes[1] != '<') {
        Slice bytes = {entry.sourceBytes + 1, entry.sourceLength - 1};
        decoded = asciiHex(bytes, &lut);
    }
    if (!decoded || lut.size != (size_t)(high + 1) * PDF_RGB_LOOKUP_CHANNELS ||
        lut.size > PDF_RGB_LOOKUP_CAPACITY) {
        free(lut.data);
        return 0;
    }
    copyPdfBytes(lookup, lut.data, lut.size);
    free(lut.data);
    return 1;
}

/**
 * Resolve a supported indexed DeviceRGB colour space.
 * @param pdf Document used to resolve lookup data.
 * @param color Indexed colour-space array.
 * @param result Receives channel and palette metadata.
 * @return Nonzero when the indexed colour space is supported.
 * @author kai
 */
static int readPdfIndexedColor(Pdf *pdf, Slice color, PdfImageColor *result) {
    size_t cursor = space(color.sourceBytes, color.sourceLength, 1);
    size_t end = valueEnd(color.sourceBytes, color.sourceLength, cursor, 0);
    Slice entry;
    ParsedUnsigned parsed;
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
    cursor = space(color.sourceBytes, color.sourceLength, end);
    parsed = (ParsedUnsigned){cursor, 0};
    if (!uintAt(color.sourceBytes, color.sourceLength, &parsed) ||
        parsed.value > PDF_SAMPLE_MAXIMUM) {
        return 0;
    }
    result->high = (unsigned)parsed.value;
    cursor = space(color.sourceBytes, color.sourceLength, parsed.cursor);
    end = valueEnd(color.sourceBytes, color.sourceLength, cursor, 0);
    if (end > color.sourceLength) {
        return 0;
    }
    entry = (Slice){color.sourceBytes + cursor, end - cursor};
    if (!loadPdfLookup(pdf, entry, result->high, result->lookup)) {
        return 0;
    }
    result->channels = 1;
    result->indexed = 1;
    return 1;
}

/**
 * Resolve DeviceRGB, DeviceGray, or Indexed PDF image colour metadata.
 * @param pdf Document used to resolve indirect colour values.
 * @param color Resolved colour-space value.
 * @param result Receives channel and palette metadata.
 * @return Nonzero when the colour space is supported.
 * @author kai
 */
static int readPdfImageColor(Pdf *pdf, Slice color, PdfImageColor *result) {
    if (nameIs(color, "/DeviceRGB")) {
        result->channels = PDF_RGB_LOOKUP_CHANNELS;
        return 1;
    }
    if (nameIs(color, "/DeviceGray")) {
        result->channels = 1;
        return 1;
    }
    return color.sourceLength > 2 && color.sourceBytes[0] == '[' &&
           readPdfIndexedColor(pdf, color, result);
}

/**
 * Apply a PDF colour-key mask against the original packed samples.
 * @param pdf Document used to resolve the mask array.
 * @param pdfObject Image object containing the mask.
 * @param image Decoded image whose alpha channel is updated.
 * @param data Original packed samples.
 * @param layout PDF image sample layout.
 * @param jpeg Whether the image uses JPEG encoding.
 * @return Nonzero when no mask is present or the mask is applied.
 * @author kai
 */
static int applyPdfColorKey(Pdf *pdf, PdfObject *pdfObject, RcImage *image, const RcBuffer *data,
                            PdfImageSampleLayout layout, int jpeg) {
    Slice mask = {0};
    size_t row = ((layout.width * layout.channels * layout.bits) + (PDF_BITS_PER_BYTE - 1)) /
                 PDF_BITS_PER_BYTE;
    size_t limits[PDF_COLOR_KEY_RANGE_COUNT] = {0};
    size_t cursor = 1;
    size_t rowIndex;
    unsigned index;
    if (dictValue(pdfObject->value, "/Mask", &mask) != 1) {
        return 1;
    }
    if (!resolve(pdf, &mask) || !mask.sourceLength || mask.sourceBytes[0] != '[' || jpeg) {
        return 0;
    }
    {
        ParsedUnsigned parsed = {cursor, 0};
        for (index = 0; index < layout.channels * 2; index++) {
            parsed.cursor = cursor;
            if (!uintAt(mask.sourceBytes, mask.sourceLength, &parsed)) {
                return 0;
            }
            cursor = parsed.cursor;
            limits[index] = parsed.value;
        }
    }
    for (rowIndex = 0; rowIndex < layout.height; rowIndex++) {
        size_t columnIndex;
        for (columnIndex = 0; columnIndex < layout.width; columnIndex++) {
            int transparent = 1;
            for (index = 0; index < layout.channels; index++) {
                unsigned value =
                    rcSample(data->data + (rowIndex * row), (columnIndex * layout.channels) + index,
                             (unsigned)layout.bits);
                if (value < limits[(size_t)index * 2] || value > limits[((size_t)index * 2) + 1]) {
                    transparent = 0;
                }
            }
            if (transparent) {
                image->rgba[(((rowIndex * layout.width) + columnIndex) * 4) + 3] = 0;
            }
        }
    }
    return 1;
}

/**
 * Decode JPEG bytes from a PDF image and validate the declared dimensions.
 * @param data JPEG payload.
 * @param image Receives decoded pixels.
 * @param layout Declared width and height.
 * @return 1 for a matching image, -1 for invalid encoded data.
 * @author kai
 */
static int decodePdfJpegImage(const RcBuffer *data, RcImage *image, PdfImageSampleLayout layout) {
    if (!rcJpeg(data->data, data->size, image, 0) || image->width != layout.width ||
        image->height != layout.height) {
        return -1;
    }
    return 1;
}

/**
 * Validate and decode raw PDF samples into RGBA pixels.
 * @param pdf Document used to resolve predictor and Decode parameters.
 * @param pdfObject Image dictionary.
 * @param image Receives the decoded raster.
 * @param data Filter-decoded packed samples.
 * @param layout Image dimensions and colour metadata; bits is updated from the dictionary.
 * @param lookup Indexed RGB palette.
 * @return Nonzero when the raw samples form a valid image.
 * @author kai
 */
static int decodePdfRawImage(Pdf *pdf, PdfObject *pdfObject, RcImage *image, RcBuffer *data,
                             PdfImageSampleLayout *layout,
                             const unsigned char lookup[PDF_RGB_LOOKUP_CAPACITY]) {
    double ranges[3][2] = {{0, 1}, {0, 1}, {0, 1}};
    size_t row;
    if (!getSize(pdf, pdfObject, "/BitsPerComponent", &layout->bits) ||
        (layout->bits != 1 && layout->bits != 2 && layout->bits != 4 &&
         layout->bits != PDF_BITS_PER_BYTE && layout->bits != PDF_SIXTEEN_BITS) ||
        !predictor(pdf, pdfObject, data,
                   (PredictorShape){
                       (unsigned)layout->width,
                       (unsigned)layout->height,
                       layout->channels,
                       (unsigned)layout->bits,
                   })) {
        return 0;
    }
    row = ((layout->width * layout->channels * layout->bits) + (PDF_BITS_PER_BYTE - 1)) /
          PDF_BITS_PER_BYTE;
    if (data->size != row * layout->height) {
        return 0;
    }
    image->rgba = malloc(layout->width * layout->height * 4);
    if (!image->rgba) {
        return 0;
    }
    image->width = (unsigned)layout->width;
    image->height = (unsigned)layout->height;
    return readPdfDecodeRanges(pdf, pdfObject, *layout, ranges) &&
           expandPdfSamples(image, data, *layout, lookup, ranges);
}

/**
 * Validate PDF image metadata and resolve its declared dimensions and colour space.
 * @param pdf Document containing the image.
 * @param pdfObject Image object being checked.
 * @param depth Current soft-mask recursion depth.
 * @param color Receives the resolved colour-space value.
 * @param width Receives the declared image width.
 * @param height Receives the declared image height.
 * @param maskId Receives the soft-mask object ID when present.
 * @return Nonzero when the image metadata is supported.
 * @author kai
 */
static int validatePdfImage(Pdf *pdf, PdfObject *pdfObject, unsigned depth, Slice *color,
                            size_t *width, size_t *height, unsigned *maskId) {
    Slice value;
    if (dictValue(pdfObject->value, "/Matte", &value) == 1) {
        return 0;
    }
    if (dictValue(pdfObject->value, "/SMask", &value) == 1 && !nameIs(value, "/None") &&
        !getRef(pdf, pdfObject, "/SMask", maskId)) {
        return 0;
    }
    if (depth > 2 || !pdfObject->stream || !getSize(pdf, pdfObject, "/Width", width) ||
        !getSize(pdf, pdfObject, "/Height", height) || !*width || !*height ||
        *width > RC_PIXELS / *height) {
        return 0;
    }
    if (dictValue(pdfObject->value, "/ImageMask", &value) == 1 && !nameIs(value, "false")) {
        return 0;
    }
    return dictValue(pdfObject->value, "/ColorSpace", color) == 1 && resolve(pdf, color);
}

/**
 * Decode one PDF image payload as JPEG or raw samples.
 * @param pdf Document used for predictor parameters.
 * @param pdfObject Image dictionary.
 * @param image Receives decoded RGBA pixels.
 * @param data Filter-decoded payload.
 * @param layout Image geometry and colour metadata; raw decoding updates bits.
 * @param lookup Indexed RGB palette.
 * @param jpeg Whether the payload is JPEG encoded.
 * @return 1 for success, 0 for unsupported metadata, or -1 for corrupt JPEG data.
 * @author kai
 */
static int decodePdfImageData(Pdf *pdf, PdfObject *pdfObject, RcImage *image, RcBuffer *data,
                              PdfImageSampleLayout *layout,
                              const unsigned char lookup[PDF_RGB_LOOKUP_CAPACITY], int jpeg) {
    Slice decode;
    if (jpeg) {
        if (dictValue(pdfObject->value, "/Decode", &decode) == 1 || layout->indexed) {
            return 0;
        }
        return decodePdfJpegImage(data, image, *layout);
    }
    return decodePdfRawImage(pdf, pdfObject, image, data, layout, lookup);
}

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
    size_t imageWidth;
    size_t imageHeight;
    size_t bits = PDF_BITS_PER_BYTE;
    unsigned channels = 0;
    unsigned indexed = 0;
    unsigned high = 0;
    unsigned maskId;
    unsigned char lookup[PDF_RGB_LOOKUP_CAPACITY];
    RcBuffer data = {0};
    int jpeg = 0;
    int successStatus = 0;

    /* VALIDATE IMAGE CAPABILITIES*/
    /* Unsupported matte, mask, or colour metadata must preserve the original object.*/
    *image = (RcImage){0};
    if (!validatePdfImage(pdf, pdfObject, depth, &color, &imageWidth, &imageHeight, &maskId)) {
        return 0;
    }

    {
        PdfImageColor imageColor = {0};
        if (!readPdfImageColor(pdf, color, &imageColor)) {
            return 0;
        }
        channels = imageColor.channels;
        indexed = imageColor.indexed;
        high = imageColor.high;
        copyPdfBytes(lookup, imageColor.lookup, sizeof(lookup));
    }
    {
        int status = decodeFilters(pdf, pdfObject, &data, &jpeg);
        if (status <= 0) {
            successStatus = status;
            goto done;
        }
    }

    /* DECODE JPEG OR RECONSTRUCT RAW SAMPLES*/
    {
        PdfImageSampleLayout layout = {imageWidth, imageHeight, bits, channels, indexed, high};
        successStatus = decodePdfImageData(pdf, pdfObject, image, &data, &layout, lookup, jpeg);
        if (successStatus <= 0) {
            goto done;
        }
        if (!jpeg) {
            bits = layout.bits;
        }
    }

    /* APPLY COLOUR-KEY TRANSPARENCY*/
    /* Compare original packed samples before their colour-range mapping.*/
    {
        PdfImageSampleLayout layout = {imageWidth, imageHeight, bits, channels, indexed, high};
        if (!applyPdfColorKey(pdf, pdfObject, image, &data, layout, jpeg)) {
            goto done;
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
        size_t pixelIndex;
        for (pixelIndex = 0; pixelIndex < imageWidth * imageHeight; pixelIndex++) {
            image->rgba[(pixelIndex * 4) + 3] = alpha.rgba[pixelIndex * 4];
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
typedef struct {
    size_t digits;
} PdfMinimumDigits;

/**
 * Append an unsigned decimal number, padding to a minimum field width when requested.
 * @param out Destination buffer.
 * @param value Number to append.
 * @param minimumDigits Minimum number of emitted digits.
 * @return 1 when every digit is appended; otherwise 0 on allocation failure.
 * @author kai
 */
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
 * Serialize one live PDF object and record its output offset.
 * @param pdf Parsed document.
 * @param out Candidate output buffer.
 * @param offsets Output object offsets.
 * @param itemIndex Object number to serialize.
 * @return Nonzero when the complete object is written.
 * @author kai
 */
static int writePdfObject(Pdf *pdf, RcBuffer *out, size_t offsets[], unsigned itemIndex) {
    PdfObject *pdfObject = &pdf->objects[itemIndex];
    if (!pdfObject->live) {
        return 1;
    }
    offsets[itemIndex] = out->size;
    if (!appendUnsigned(out, itemIndex, (PdfMinimumDigits){0}) ||
        !rcAppend(out, " ", sizeof(" ") - 1) ||
        !appendUnsigned(out, pdfObject->generation, (PdfMinimumDigits){0}) ||
        !rcAppend(out, " obj\n", sizeof(" obj\n") - 1)) {
        return 0;
    }
    if (pdfObject->replacement.size) {
        if (!rcAppend(out, pdfObject->replacement.data, pdfObject->replacement.size)) {
            return 0;
        }
    } else if (!rcAppend(out, pdfObject->value.sourceBytes,
                         (size_t)(pdfObject->end - pdfObject->value.sourceBytes))) {
        return 0;
    }
    return rcAppend(out, "\nendobj\n", PDF_END_OBJECT_TEXT_LENGTH);
}

/**
 * Write the rebuilt PDF cross-reference table and trailer.
 * @param pdf Parsed document.
 * @param out Candidate output buffer.
 * @param offsets Offsets recorded while writing objects.
 * @param xref Receives the xref table offset.
 * @return Nonzero when xref, trailer, and EOF marker are appended.
 * @author kai
 */
static int writePdfCrossReference(Pdf *pdf, RcBuffer *out, const size_t offsets[], size_t *xref) {
    unsigned itemIndex;
    *xref = out->size;
    if (!rcAppend(out, "xref\n0 ", sizeof("xref\n0 ") - 1) ||
        !appendUnsigned(out, pdf->count, (PdfMinimumDigits){0}) ||
        !rcAppend(out, "\n0000000000 65535 f \n", sizeof("\n0000000000 65535 f \n") - 1)) {
        return 0;
    }
    for (itemIndex = 1; itemIndex < pdf->count; itemIndex++) {
        if (!appendUnsigned(out, (unsigned long)offsets[itemIndex],
                            (PdfMinimumDigits){PDF_XREF_OFFSET_WIDTH}) ||
            !rcAppend(out, " ", sizeof(" ") - 1) ||
            !appendUnsigned(out, pdf->objects[itemIndex].generation,
                            (PdfMinimumDigits){PDF_XREF_GENERATION_WIDTH}) ||
            !rcAppend(out, pdf->objects[itemIndex].live ? " n \n" : " f \n", sizeof(" n \n") - 1)) {
            return 0;
        }
    }
    return rcAppend(out, "trailer\n", PDF_TRAILER_TEXT_LENGTH) &&
           rcAppend(out, pdf->trailer.sourceBytes, pdf->trailer.sourceLength) &&
           rcAppend(out, "\nstartxref\n", sizeof("\nstartxref\n") - 1) &&
           appendUnsigned(out, (unsigned long)*xref, (PdfMinimumDigits){0}) &&
           rcAppend(out, "\n%%EOF\n", sizeof("\n%%EOF\n") - 1);
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

    for (itemIndex = 1; itemIndex < pdf->count; itemIndex++) {
        if (!writePdfObject(pdf, out, offsets, itemIndex)) {
            goto done;
        }
    }
    if (!writePdfCrossReference(pdf, out, offsets, &xref)) {
        goto done;
    }
    successStatus = 1;
done:
    free(offsets);
    return successStatus;
}

/* PDF PIPELINE*/
/**
 * Mark soft and colour masks so they are not processed as standalone images.
 * @param pdf Parsed PDF document.
 * @author kai
 */
static void markPdfMasks(Pdf *pdf) {
    unsigned itemIndex;
    for (itemIndex = 1; itemIndex < pdf->count; itemIndex++) {
        if (pdf->objects[itemIndex].live) {
            unsigned mask;
            if (getRef(pdf, &pdf->objects[itemIndex], "/SMask", &mask) ||
                getRef(pdf, &pdf->objects[itemIndex], "/Mask", &mask)) {
                pdf->objects[mask].mask = 1;
            }
        }
    }
}

/**
 * Compress vector drawing commands into a replacement PDF form stream.
 * @param pdfObject Image object receiving the replacement form.
 * @param vector Vectorized image.
 * @return Nonzero when the complete form stream is assembled.
 * @author kai
 */
static int writePdfForm(PdfObject *pdfObject, const RcVector *vector) {
    RcBuffer compressed = {0};
    int success = 0;
    if (!rcDeflate(vector->pdf.data, vector->pdf.size, &compressed)) {
        goto done;
    }
    if (!rcAppend(
            &pdfObject->replacement,
            "<< /Type /XObject /Subtype /Form /FormType 1 /BBox [0 0 1 1] /Resources << ",
            sizeof("<< /Type /XObject /Subtype /Form /FormType 1 /BBox [0 0 1 1] /Resources << ") -
                1) ||
        !rcAppend(&pdfObject->replacement, vector->resources.data, vector->resources.size) ||
        !rcAppend(&pdfObject->replacement, ">> /Filter /FlateDecode /Length ",
                  sizeof(">> /Filter /FlateDecode /Length ") - 1) ||
        !appendUnsigned(&pdfObject->replacement, (unsigned long)compressed.size,
                        (PdfMinimumDigits){0}) ||
        !rcAppend(&pdfObject->replacement, " >>\nstream\n", sizeof(" >>\nstream\n") - 1) ||
        !rcAppend(&pdfObject->replacement, compressed.data, compressed.size) ||
        !rcAppend(&pdfObject->replacement, "\nendstream\n", PDF_ENDSTREAM_TEXT_LENGTH)) {
        goto done;
    }
    success = 1;
done:
    free(compressed.data);
    return success;
}

/**
 * Decode, vectorize, and register one PDF image object.
 * @param pdf Parsed document.
 * @param objectId Image object number.
 * @param options Vectorization options.
 * @param images Receives owned SVG sidecars.
 * @param imageCount Number of registered SVG sidecars.
 * @param unsupported Counter for skipped unsupported images.
 * @return Nonzero on success or a supported skip.
 * @author kai
 */
static int processPdfImageObject(Pdf *pdf, unsigned objectId, const MediaOptions *options,
                                 RcBuffer **images, size_t *imageCount, unsigned *unsupported) {
    PdfObject *pdfObject = &pdf->objects[objectId];
    RcImage image = {0};
    RcVector vector = {0};
    RcBuffer *more;
    int decoded = pdfImage(pdf, objectId, &image, 0);
    int success = 0;
    if (decoded < 0) {
        goto done;
    }
    if (!decoded) {
        (*unsupported)++;
        success = 1;
        goto done;
    }
    if (!rcVectorize(&image, (RcVectorOptions){options->colors, options->tolerance}, &vector)) {
        goto done;
    }
    free(image.rgba);
    image.rgba = NULL;
    more = realloc(*images, (*imageCount + 1) * sizeof(**images));
    if (!more) {
        goto done;
    }
    *images = more;
    (*images)[*imageCount] = vector.svg;
    vector.svg = (RcBuffer){0};
    (*imageCount)++;
    success = writePdfForm(pdfObject, &vector);
done:
    free(image.rgba);
    rcFreeVector(&vector);
    return success;
}

/**
 * Process all eligible PDF image objects.
 * @param pdf Parsed document with image replacements.
 * @param options Vectorization settings.
 * @param images Receives owned SVG sidecars.
 * @param imageCount Number of registered SVG sidecars.
 * @param unsupported Counter for skipped unsupported images.
 * @return Nonzero when all eligible images were processed.
 * @author kai
 */
static int processPdfImages(Pdf *pdf, const MediaOptions *options, RcBuffer **images,
                            size_t *imageCount, unsigned *unsupported) {
    unsigned itemIndex;
    markPdfMasks(pdf);
    for (itemIndex = 1; itemIndex < pdf->count; itemIndex++) {
        PdfObject *pdfObject = &pdf->objects[itemIndex];
        Slice subtype;
        if (!pdfObject->live || pdfObject->mask ||
            dictValue(pdfObject->value, "/Subtype", &subtype) != 1 || !nameIs(subtype, "/Image")) {
            continue;
        }
        if (!processPdfImageObject(pdf, itemIndex, options, images, imageCount, unsupported)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Release PDF object replacements and any sidecars after failed processing.
 * @param pdf Parsed document.
 * @author kai
 */
static void releasePdfObjects(Pdf *pdf) {
    unsigned itemIndex;
    if (pdf->objects) {
        for (itemIndex = 0; itemIndex < pdf->count; itemIndex++) {
            free(pdf->objects[itemIndex].replacement.data);
        }
        free(pdf->objects);
    }
}

/**
 * Release registered SVG sidecars after a failed PDF candidate.
 * @param images Registered SVG sidecars.
 * @param imageCount Number of registered sidecars.
 * @author kai
 */
static void releasePdfImages(RcBuffer **images, size_t *imageCount) {
    unsigned itemIndex;
    for (itemIndex = 0; itemIndex < *imageCount; itemIndex++) {
        free((*images)[itemIndex].data);
    }
    free(*images);
    *images = NULL;
    *imageCount = 0;
}

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
    int successStatus = 0;
    pdf.sourceBytes = data;
    pdf.sourceLength = length;
    *images = NULL;
    *imageCount = 0;
    *unsupported = 0;
    if (!parsePdf(&pdf)) {
        goto done;
    }
    if (pdf.unsupported || !options->vectorMode) {
        *unsupported = (unsigned)pdf.unsupported;
        successStatus = 1;
        goto done;
    }
    if (!processPdfImages(&pdf, options, images, imageCount, unsupported)) {
        goto done;
    }
    if (!*imageCount) {
        successStatus = 1;
        goto done;
    }
    successStatus = serialize(&pdf, candidate);
    if (successStatus) {
        Pdf check = {0};
        check.sourceBytes = candidate->data;
        check.sourceLength = candidate->size;
        successStatus = parsePdf(&check) && !check.unsupported;
        free(check.objects);
    }
done:
    releasePdfObjects(&pdf);
    if (!successStatus) {
        releasePdfImages(images, imageCount);
    }
    return successStatus;
}
