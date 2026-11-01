/**
 * @file compression.c
 * @brief Reversible byte-container compression and conservative PDF image-stream RLE.
 *
 * @code
 * main() -> showMainMenu()
 * +-- compressFile() -> readInputFile()
 * |   +-- PDF: transformPdfDocument()
 * |   |   +-- parsePdfDocument() -> encode eligible image streams
 * |   |   +-- if changed: replacePdfImageStream() -> serializePdfDocument()
 * |   |   +-- unsupported or no size reduction: copy the valid PDF unchanged
 * |   +-- other files: encodeContainer() -> RLE or stored bytes with CRC-32
 * |   +-- writeOutputFile() [once after successful processing]
 * +-- decompressFile() -> readInputFile()
 *     +-- PDF: restore marked image streams through transformPdfDocument()
 *     +-- other files: decodeContainer() -> validate length and CRC-32
 *     +-- writeOutputFile() [once after successful restoration]
 * @endcode
 *
 * Image inputs stay as encoded bytes; this implementation does not decode pixels or rasterize PDFs.
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fileIO/fileInput/fileInput.h"
#include "../fileIO/fileOutput/fileOutput.h"
#include "compression.h"

/*//////////////////////////////////////////////////
PRIVATE TYPES
//////////////////////////////////////////////////*/

// BUFFER STORAGE
/**
 * Growable byte storage owned by the current compression operation.
 */
typedef struct ByteBuffer {
    unsigned char *data; /**< Allocated bytes, or NULL when the buffer is empty. */
    size_t size;         /**< Number of initialized bytes. */
    size_t capacity;     /**< Number of bytes allocated at data. */
} ByteBuffer;

// CONTAINER HEADER
/* CRL1 stores a mode byte, original size and CRC-32 before its payload. */
enum { CONTAINER_HEADER_SIZE = 17, RLE_LITERAL_LIMIT = 128, RLE_END_MARKER = 128 };
enum { CONTAINER_STORED = 0, CONTAINER_RLE = 1 };

// PDF OBJECT MODEL
/** One active or free entry in a classic PDF cross-reference table. */
typedef struct PdfXrefEntry {
    uint64_t offset;     /**< Object offset when in use; free-list link otherwise. */
    unsigned generation; /**< Object generation from the xref entry. */
    int inUse;           /**< Nonzero when the object is present in the file. */
} PdfXrefEntry;
/** Parsed object bytes plus offsets needed to edit a direct-length image stream. */
typedef struct PdfObject {
    unsigned char *data;    /**< Owned copy of the complete object. */
    size_t size;            /**< Number of bytes in data. */
    size_t dictionaryStart; /**< Dictionary start relative to data, or SIZE_MAX when absent. */
    size_t dictionaryEnd;   /**< Byte after the dictionary, or SIZE_MAX when absent. */
    size_t lengthStart;     /**< Direct Length digits relative to data. */
    size_t lengthEnd;       /**< Byte after the direct Length digits. */
    size_t streamStart;     /**< First stream byte relative to data. */
    size_t streamSize;      /**< Number of stream bytes from the direct Length entry. */
    size_t filterEnd;       /**< Byte after a RunLengthDecode name, or zero when absent. */
    size_t markerStart;     /**< CRL1 marker offset, or SIZE_MAX when absent. */
    int hasStream;          /**< Nonzero when this object has a stream. */
    int isImage;            /**< Nonzero when this is an Image XObject. */
    int hasFilter;          /**< Nonzero when the image already has a Filter entry. */
    int hasRleFilter;       /**< Nonzero when Filter is the supported RunLengthDecode name. */
} PdfObject;
/** Parsed xref, objects, and trailer ranges used by the PDF serializer. */
typedef struct PdfDocument {
    PdfXrefEntry *xref;       /**< Entries indexed by object number. */
    PdfObject *objects;       /**< Parsed object copies indexed by object number. */
    size_t objectCount;       /**< /Size value from the trailer. */
    size_t firstObjectOffset; /**< Earliest active object offset in the source PDF. */
    size_t trailerStart;      /**< Trailer dictionary offset in the source PDF. */
    size_t trailerSize;       /**< Trailer dictionary length. */
} PdfDocument;

/*//////////////////////////////////////////////////
BYTE BUFFERS
//////////////////////////////////////////////////*/

// OWNERSHIP AND GROWTH
/**
 * Reserves capacity for additional bytes without overflowing the current size.
 *
 * @param buffer Buffer to grow.
 * @param extra Number of additional bytes required.
 * @return One when capacity is available, or zero on overflow or allocation failure.
 */
static int bufferReserve(ByteBuffer *buffer, size_t extra) {
    size_t required;
    size_t capacity;
    unsigned char *resized;
    if (extra > SIZE_MAX - buffer->size)
        return 0;
    required = buffer->size + extra;
    if (required <= buffer->capacity)
        return 1;
    capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    resized = realloc(buffer->data, capacity);
    if (resized == NULL)
        return 0;
    buffer->data = resized;
    buffer->capacity = capacity;
    return 1;
}
/**
 * Appends bytes to a buffer.
 *
 * @param buffer Destination buffer.
 * @param data Bytes to append; may be NULL when size is zero.
 * @param size Number of bytes to append.
 * @return One when the bytes are appended, or zero on allocation failure.
 */
static int bufferAppend(ByteBuffer *buffer, const void *data, size_t size) {
    if (size == 0)
        return 1;
    if (data == NULL || !bufferReserve(buffer, size))
        return 0;
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    return 1;
}
/**
 * Frees a buffer's allocation and resets it to an empty state.
 *
 * @param buffer Buffer to release.
 */
static void bufferRelease(ByteBuffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

/*//////////////////////////////////////////////////
RUN-LENGTH ENCODING
//////////////////////////////////////////////////*/

// PACKET ENCODING
/**
 * Counts equal bytes from position, capped at the RLE packet limit.
 *
 * @param data Input bytes.
 * @param size Number of input bytes.
 * @param position First byte in the run.
 * @return Number of matching bytes starting at position.
 */
static size_t repeatedByteCount(const unsigned char *data, size_t size, size_t position) {
    size_t count = 1;
    while (count < RLE_LITERAL_LIMIT && count < size - position &&
           data[position + count] == data[position])
        count++;
    return count;
}
/**
 * Encodes bytes as a PDF-compatible RLE stream ending with the required marker.
 *
 * @param input Source bytes; may be NULL only when inputSize is zero.
 * @param inputSize Number of source bytes.
 * @param encoded Destination buffer for the encoded stream.
 * @return One on success, or zero for invalid input or allocation failure.
 */
static int runLengthEncode(const unsigned char *input, size_t inputSize, ByteBuffer *encoded) {
    unsigned char endMarker = RLE_END_MARKER;
    size_t position = 0;
    if (input == NULL && inputSize != 0)
        return 0;

    // ENCODE PACKETS
    while (position < inputSize) {
        size_t run = repeatedByteCount(input, inputSize, position);
        /* Three repeated bytes save space; shorter repeats join a literal block. */
        if (run >= 3) {
            unsigned char control = (unsigned char)(257 - run);
            if (!bufferAppend(encoded, &control, 1) || !bufferAppend(encoded, input + position, 1))
                return 0;
            position += run;
            continue;
        }
        {
            size_t literalStart = position;
            size_t literalSize = 0;
            unsigned char control;
            while (position < inputSize && literalSize < RLE_LITERAL_LIMIT) {
                run = repeatedByteCount(input, inputSize, position);
                if (run >= 3)
                    break;
                if (run > RLE_LITERAL_LIMIT - literalSize)
                    run = RLE_LITERAL_LIMIT - literalSize;
                position += run;
                literalSize += run;
            }
            if (literalSize == 0)
                return 0;
            control = (unsigned char)(literalSize - 1);
            if (!bufferAppend(encoded, &control, 1) ||
                !bufferAppend(encoded, input + literalStart, literalSize))
                return 0;
        }
    }
    return bufferAppend(encoded, &endMarker, 1);
}

// STREAM DECODING
/**
 * Decodes a complete RLE stream into caller-owned bytes.
 *
 * @param input Encoded stream bytes.
 * @param inputSize Number of encoded bytes.
 * @param expectedSize Exact decoded size, or SIZE_MAX when unknown.
 * @param decoded Receives the allocated bytes on success.
 * @param decodedSize Receives the decoded byte count.
 * @return One for a valid stream, or zero for malformed input or allocation failure.
 */
static int runLengthDecode(const unsigned char *input, size_t inputSize, size_t expectedSize,
                           unsigned char **decoded, size_t *decodedSize) {
    ByteBuffer output = {0};
    size_t position = 0;
    if (input == NULL || decoded == NULL || decodedSize == NULL)
        return 0;

    // DECODE AND VALIDATE PACKETS
    while (position < inputSize) {
        unsigned char control = input[position++];
        size_t count;
        if (control == RLE_END_MARKER) {
            if (position != inputSize || (expectedSize != SIZE_MAX && output.size != expectedSize))
                goto invalid_stream;
            *decoded = output.data;
            *decodedSize = output.size;
            return 1;
        }
        if (control <= 127) {
            count = (size_t)control + 1;
            if (count > inputSize - position ||
                (expectedSize != SIZE_MAX && count > expectedSize - output.size) ||
                !bufferAppend(&output, input + position, count))
                goto invalid_stream;
            position += count;
        } else {
            count = 257u - control;
            if (position >= inputSize ||
                (expectedSize != SIZE_MAX && count > expectedSize - output.size) ||
                !bufferReserve(&output, count))
                goto invalid_stream;
            memset(output.data + output.size, input[position++], count);
            output.size += count;
        }
    }
invalid_stream:
    bufferRelease(&output);
    return 0;
}

/*//////////////////////////////////////////////////
CONTAINER FORMAT
//////////////////////////////////////////////////*/

// HEADER FIELDS
/**
 * Stores an integer least-significant byte first.
 *
 * @param output Destination for byteCount bytes.
 * @param value Integer to encode.
 * @param byteCount Number of bytes to write, from one through eight.
 */
static void writeLittleEndian(unsigned char *output, uint64_t value, size_t byteCount) {
    size_t index;
    for (index = 0; index < byteCount; index++)
        output[index] = (unsigned char)(value >> (index * 8));
}
/**
 * Reads an integer stored least-significant byte first.
 *
 * @param input Encoded integer bytes.
 * @param byteCount Number of bytes to read, from one through eight.
 * @return The decoded integer.
 */
static uint64_t readLittleEndian(const unsigned char *input, size_t byteCount) {
    uint64_t value = 0;
    size_t index;
    for (index = 0; index < byteCount; index++)
        value |= (uint64_t)input[index] << (index * 8);
    return value;
}

// PAYLOAD INTEGRITY
/**
 * Computes CRC-32 over a byte range without an external library.
 *
 * @param data Bytes to checksum; may be NULL when size is zero.
 * @param size Number of bytes.
 * @return The CRC-32 value.
 */
static uint32_t checksum32(const unsigned char *data, size_t size) {
    uint32_t checksum = UINT32_C(0xffffffff);
    size_t index;
    for (index = 0; index < size; index++) {
        unsigned bit;
        checksum ^= data[index];
        for (bit = 0; bit < 8; bit++)
            checksum =
                (checksum >> 1) ^ (UINT32_C(0xedb88320) & (uint32_t)-(int32_t)(checksum & 1));
    }
    return ~checksum;
}

// CRL1 ENVELOPE
/**
 * Encodes bytes in a CRL1 container, using stored mode when RLE does not shrink the payload.
 *
 * @param input Source bytes; may be NULL only when inputSize is zero.
 * @param inputSize Number of source bytes.
 * @param output Receives the allocated container on success; the caller frees it.
 * @param outputSize Receives the complete container size.
 * @return One on success, or zero for invalid arguments or allocation failure.
 */
static int encodeContainer(const unsigned char *input, size_t inputSize, unsigned char **output,
                           size_t *outputSize) {
    ByteBuffer encoded = {0};
    const unsigned char *payload;
    size_t payloadSize;
    size_t totalSize;
    unsigned char mode;
    unsigned char *container;
    uint64_t encodedLength = (uint64_t)inputSize;
    if ((input == NULL && inputSize != 0) || output == NULL || outputSize == NULL ||
        (size_t)encodedLength != inputSize)
        return 0;
    if (!runLengthEncode(input, inputSize, &encoded)) {
        bufferRelease(&encoded);
        return 0;
    }

    // SELECT THE ENCODING MODE
    /* RLE can expand arbitrary data, so keep it only when the payload shrinks. */
    if (encoded.size < inputSize) {
        mode = CONTAINER_RLE;
        payload = encoded.data;
        payloadSize = encoded.size;
    } else {
        mode = CONTAINER_STORED;
        payload = input;
        payloadSize = inputSize;
    }

    // BUILD THE CONTAINER
    if (payloadSize > SIZE_MAX - CONTAINER_HEADER_SIZE) {
        bufferRelease(&encoded);
        return 0;
    }
    totalSize = CONTAINER_HEADER_SIZE + payloadSize;
    container = malloc(totalSize == 0 ? 1 : totalSize);
    if (container == NULL) {
        bufferRelease(&encoded);
        return 0;
    }
    memcpy(container, "CRL1", 4);
    container[4] = mode;
    writeLittleEndian(container + 5, encodedLength, 8);
    writeLittleEndian(container + 13, checksum32(input, inputSize), 4);
    if (payloadSize != 0)
        memcpy(container + CONTAINER_HEADER_SIZE, payload, payloadSize);
    bufferRelease(&encoded);
    *output = container;
    *outputSize = totalSize;
    return 1;
}
/**
 * Validates a CRL1 container's mode, length, RLE stream, and checksum before returning bytes.
 *
 * @param input Complete container bytes.
 * @param inputSize Number of container bytes.
 * @param output Receives allocated decoded bytes on success; the caller frees them.
 * @param outputSize Receives the decoded length.
 * @return One for a valid container, or zero without partial output for malformed data.
 */
static int decodeContainer(const unsigned char *input, size_t inputSize, unsigned char **output,
                           size_t *outputSize) {
    uint64_t originalSize64;
    size_t originalSize;
    size_t payloadSize;
    uint32_t expectedChecksum;
    unsigned char *decoded = NULL;
    size_t decodedSize = 0;

    // VALIDATE THE HEADER
    if (input == NULL || output == NULL || outputSize == NULL ||
        inputSize < CONTAINER_HEADER_SIZE || memcmp(input, "CRL1", 4) != 0)
        return 0;
    originalSize64 = readLittleEndian(input + 5, 8);
    if (originalSize64 > SIZE_MAX)
        return 0;
    originalSize = (size_t)originalSize64;
    expectedChecksum = (uint32_t)readLittleEndian(input + 13, 4);
    payloadSize = inputSize - CONTAINER_HEADER_SIZE;

    // DECODE THE SELECTED PAYLOAD MODE
    if (input[4] == CONTAINER_STORED) {
        if (payloadSize != originalSize)
            return 0;
        if (originalSize != 0) {
            decoded = malloc(originalSize);
            if (decoded == NULL)
                return 0;
            memcpy(decoded, input + CONTAINER_HEADER_SIZE, originalSize);
        }
        decodedSize = originalSize;
    } else if (input[4] == CONTAINER_RLE) {
        if (!runLengthDecode(input + CONTAINER_HEADER_SIZE, payloadSize, originalSize, &decoded,
                             &decodedSize))
            return 0;
    } else {
        return 0;
    }

    // VERIFY PAYLOAD INTEGRITY
    if (checksum32(decoded, decodedSize) != expectedChecksum) {
        free(decoded);
        return 0;
    }
    *output = decoded;
    *outputSize = decodedSize;
    return 1;
}

/*//////////////////////////////////////////////////
PDF TOKEN HELPERS
//////////////////////////////////////////////////*/

// TOKEN BOUNDARIES
/**
 * Tests whether a byte is one of the six PDF whitespace values.
 *
 * @param value Byte to test.
 * @return One for PDF whitespace, or zero otherwise.
 */
static int pdfWhitespace(unsigned char value) {
    return value == 0 || value == 9 || value == 10 || value == 12 || value == 13 || value == 32;
}
/**
 * Tests whether a byte separates PDF tokens.
 *
 * @param value Byte to test.
 * @return One for a PDF delimiter, or zero otherwise.
 */
static int pdfDelimiter(unsigned char value) {
    return pdfWhitespace(value) || value == '(' || value == ')' || value == '<' || value == '>' ||
           value == '[' || value == ']' || value == '{' || value == '}' || value == '/' ||
           value == '%';
}
/**
 * Skips PDF whitespace and comments up to the next token.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param position Starting byte offset.
 * @return First non-space, non-comment offset, or size when none remains.
 */
static size_t skipPdfSpace(const unsigned char *data, size_t size, size_t position) {
    while (position < size) {
        if (pdfWhitespace(data[position])) {
            position++;
        } else if (data[position] == '%') {
            while (position < size && data[position] != '\n' && data[position] != '\r')
                position++;
        } else {
            break;
        }
    }
    return position;
}
/**
 * Checks for a keyword bounded by PDF token delimiters.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param position Candidate keyword offset.
 * @param keyword NUL-terminated keyword.
 * @return One when the complete token is present, zero otherwise.
 */
static int pdfKeywordAt(const unsigned char *data, size_t size, size_t position,
                        const char *keyword) {
    size_t length = strlen(keyword);
    if (position > size || length > size - position ||
        memcmp(data + position, keyword, length) != 0)
        return 0;
    if (position > 0 && !pdfDelimiter(data[position - 1]))
        return 0;
    return position + length == size || pdfDelimiter(data[position + length]);
}

// NESTED VALUE SCANNING
/**
 * Skips a parenthesized string while honoring nested parentheses and backslash escapes.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param position Opening-parenthesis offset.
 * @return Byte after the closing parenthesis, or SIZE_MAX when the string is unterminated.
 */
static size_t skipPdfLiteral(const unsigned char *data, size_t size, size_t position) {
    size_t depth = 1;
    position++;
    while (position < size && depth != 0) {
        if (data[position] == '\\') {
            position += position + 1 < size ? 2 : 1;
        } else if (data[position] == '(') {
            depth++;
            position++;
        } else if (data[position] == ')') {
            depth--;
            position++;
        } else {
            position++;
        }
    }
    return depth == 0 ? position : SIZE_MAX;
}
/**
 * Skips a hexadecimal string.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param position Opening-angle-bracket offset.
 * @return Byte after the closing bracket, or SIZE_MAX when it is unterminated.
 */
static size_t skipPdfHexString(const unsigned char *data, size_t size, size_t position) {
    position++;
    while (position < size && data[position] != '>')
        position++;
    return position < size ? position + 1 : SIZE_MAX;
}
/**
 * Finds the end of a balanced dictionary, ignoring strings and comments inside it.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param start Opening-`<<` offset.
 * @return Byte after the matching `>>`, or SIZE_MAX when the dictionary is malformed.
 */
static size_t findPdfDictionaryEnd(const unsigned char *data, size_t size, size_t start) {
    size_t position;
    size_t depth = 1;
    if (start + 1 >= size || data[start] != '<' || data[start + 1] != '<')
        return SIZE_MAX;
    position = start + 2;
    while (position < size && depth != 0) {
        if (data[position] == '%') {
            position = skipPdfSpace(data, size, position);
        } else if (data[position] == '(') {
            position = skipPdfLiteral(data, size, position);
            if (position == SIZE_MAX)
                return SIZE_MAX;
        } else if (data[position] == '<' && position + 1 < size && data[position + 1] == '<') {
            depth++;
            position += 2;
        } else if (data[position] == '>' && position + 1 < size && data[position + 1] == '>') {
            depth--;
            position += 2;
        } else if (data[position] == '<') {
            position = skipPdfHexString(data, size, position);
            if (position == SIZE_MAX)
                return SIZE_MAX;
        } else {
            position++;
        }
    }
    return depth == 0 ? position : SIZE_MAX;
}
/**
 * Skips a nested array, including strings and dictionaries inside it.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param position Opening-bracket offset.
 * @return Byte after the matching `]`, or SIZE_MAX when the array is malformed.
 */
static size_t skipPdfArray(const unsigned char *data, size_t size, size_t position) {
    size_t depth = 1;
    position++;
    while (position < size && depth != 0) {
        position = skipPdfSpace(data, size, position);
        if (position >= size)
            break;
        if (data[position] == '[') {
            depth++;
            position++;
        } else if (data[position] == ']') {
            depth--;
            position++;
        } else if (data[position] == '(') {
            position = skipPdfLiteral(data, size, position);
        } else if (data[position] == '<' && position + 1 < size && data[position + 1] == '<') {
            position = findPdfDictionaryEnd(data, size, position);
        } else if (data[position] == '<') {
            position = skipPdfHexString(data, size, position);
        } else {
            position++;
        }
        if (position == SIZE_MAX)
            return SIZE_MAX;
    }
    return depth == 0 ? position : SIZE_MAX;
}
/**
 * Skips one PDF value, preserving nested strings, arrays, and dictionaries.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param position Start of the value, before optional whitespace.
 * @return Byte after the value, or SIZE_MAX when its syntax is malformed.
 */
static size_t skipPdfValue(const unsigned char *data, size_t size, size_t position) {
    position = skipPdfSpace(data, size, position);
    if (position >= size)
        return SIZE_MAX;
    if (data[position] == '(')
        return skipPdfLiteral(data, size, position);
    if (data[position] == '[')
        return skipPdfArray(data, size, position);
    if (data[position] == '/') {
        position++;
        while (position < size && !pdfDelimiter(data[position]))
            position++;
        return position;
    }
    if (data[position] == '<' && position + 1 < size && data[position + 1] == '<')
        return findPdfDictionaryEnd(data, size, position);
    if (data[position] == '<')
        return skipPdfHexString(data, size, position);
    while (position < size && !pdfDelimiter(data[position]))
        position++;
    return position;
}

// DICTIONARY LOOKUPS
/**
 * Finds a top-level dictionary key after skipping preceding nested values.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param dictionaryStart Opening-`<<` offset.
 * @param dictionaryEnd Byte after the matching `>>`.
 * @param wanted Key name without its leading slash.
 * @param keyStart Optional output for the key's slash offset.
 * @param valueStart Optional output for the value's first byte.
 * @return One when the key is found, or zero when absent or malformed.
 */
static int findPdfDictionaryEntry(const unsigned char *data, size_t size, size_t dictionaryStart,
                                  size_t dictionaryEnd, const char *wanted, size_t *keyStart,
                                  size_t *valueStart) {
    size_t position = dictionaryStart + 2;
    size_t limit = dictionaryEnd - 2;
    size_t wantedLength = strlen(wanted);
    while (position < limit) {
        size_t nameStart;
        size_t nameEnd;
        size_t valuePosition;
        size_t next;
        position = skipPdfSpace(data, size, position);
        if (position >= limit)
            break;
        if (data[position] != '/') {
            position++;
            continue;
        }
        nameStart = ++position;
        while (position < limit && !pdfDelimiter(data[position]))
            position++;
        nameEnd = position;
        valuePosition = skipPdfSpace(data, size, position);
        if (nameEnd - nameStart == wantedLength &&
            memcmp(data + nameStart, wanted, wantedLength) == 0) {
            if (keyStart != NULL)
                *keyStart = nameStart - 1;
            if (valueStart != NULL)
                *valueStart = valuePosition;
            return 1;
        }
        next = skipPdfValue(data, size, valuePosition);
        if (next == SIZE_MAX || next <= position)
            return 0;
        position = next;
    }
    return 0;
}
/**
 * Parses an unsigned decimal token and advances position past its digits.
 *
 * @param data PDF bytes.
 * @param size Number of bytes in data.
 * @param position Input offset and output offset after the number.
 * @param value Receives the parsed integer.
 * @return One for a valid in-range number, or zero if parsing fails.
 */
static int parsePdfUnsigned(const unsigned char *data, size_t size, size_t *position,
                            uint64_t *value) {
    uint64_t parsed = 0;
    size_t cursor = skipPdfSpace(data, size, *position);
    size_t start = cursor;
    while (cursor < size && data[cursor] >= '0' && data[cursor] <= '9') {
        unsigned digit = data[cursor] - '0';
        if (parsed > (UINT64_MAX - digit) / 10)
            return 0;
        parsed = parsed * 10 + digit;
        cursor++;
    }
    if (cursor == start)
        return 0;
    *position = cursor;
    *value = parsed;
    return 1;
}
/** Reads one direct unsigned integer entry and records its digit range when requested.
 *
 * @param data PDF bytes containing the dictionary.
 * @param size Number of bytes in data.
 * @param dictionaryStart Opening-`<<` offset.
 * @param dictionaryEnd Byte after the matching `>>`.
 * @param name Key name without its leading slash.
 * @param numberStart Optional output for the first digit.
 * @param numberEnd Optional output for the byte after the last digit.
 * @param value Receives the parsed integer.
 * @return One for a direct unsigned value, or zero when the entry is absent or unsupported.
 */
static int pdfEntryUnsigned(const unsigned char *data, size_t size, size_t dictionaryStart,
                            size_t dictionaryEnd, const char *name, size_t *numberStart,
                            size_t *numberEnd, uint64_t *value) {
    size_t valueStart;
    size_t position;
    size_t afterValue;
    if (!findPdfDictionaryEntry(data, size, dictionaryStart, dictionaryEnd, name, NULL,
                                &valueStart))
        return 0;
    position = valueStart;
    if (!parsePdfUnsigned(data, size, &position, value))
        return 0;
    afterValue = skipPdfSpace(data, size, position);
    if (afterValue >= dictionaryEnd ||
        !(data[afterValue] == '/' || (data[afterValue] == '>' && afterValue + 1 < dictionaryEnd &&
                                      data[afterValue + 1] == '>')))
        return 0;
    if (numberStart != NULL)
        *numberStart = valueStart;
    if (numberEnd != NULL)
        *numberEnd = position;
    return 1;
}
/** Matches a dictionary name value and optionally returns its source offsets.
 *
 * @param data PDF bytes containing the dictionary.
 * @param size Number of bytes in data.
 * @param dictionaryStart Opening-`<<` offset.
 * @param dictionaryEnd Byte after the matching `>>`.
 * @param key Key name without its leading slash.
 * @param expected Name value without its leading slash.
 * @param keyStart Optional output for the key's slash offset.
 * @param nameEnd Optional output for the byte after the value name.
 * @return One when the key has the expected name value, or zero otherwise.
 */
static int pdfEntryNameIs(const unsigned char *data, size_t size, size_t dictionaryStart,
                          size_t dictionaryEnd, const char *key, const char *expected,
                          size_t *keyStart, size_t *nameEnd) {
    size_t valueStart;
    size_t position;
    size_t nameStart;
    size_t expectedLength = strlen(expected);
    if (!findPdfDictionaryEntry(data, size, dictionaryStart, dictionaryEnd, key, keyStart,
                                &valueStart))
        return 0;
    position = skipPdfSpace(data, size, valueStart);
    if (position >= dictionaryEnd || data[position] != '/')
        return 0;
    nameStart = ++position;
    while (position < dictionaryEnd && !pdfDelimiter(data[position]))
        position++;
    if (position - nameStart != expectedLength ||
        memcmp(data + nameStart, expected, expectedLength) != 0)
        return 0;
    if (nameEnd != NULL)
        *nameEnd = position;
    return 1;
}

// TOKEN SEARCH
/** Finds a keyword outside comments, names, strings, and hexadecimal strings.
 *
 * @param data PDF bytes to search.
 * @param size Number of bytes in data.
 * @param start Byte offset to begin searching.
 * @param token NUL-terminated keyword to find.
 * @return The keyword offset, or SIZE_MAX when absent or malformed syntax is encountered.
 */
static size_t findPdfToken(const unsigned char *data, size_t size, size_t start,
                           const char *token) {
    size_t position = start;
    while (position < size) {
        if (data[position] == '%') {
            position = skipPdfSpace(data, size, position);
        } else if (data[position] == '(') {
            position = skipPdfLiteral(data, size, position);
        } else if (data[position] == '/') {
            position++;
            while (position < size && !pdfDelimiter(data[position]))
                position++;
        } else if (data[position] == '<' && position + 1 < size && data[position + 1] == '<') {
            position += 2;
        } else if (data[position] == '<') {
            position = skipPdfHexString(data, size, position);
        } else if (pdfKeywordAt(data, size, position, token)) {
            return position;
        } else {
            position++;
        }
        if (position == SIZE_MAX)
            return SIZE_MAX;
    }
    return SIZE_MAX;
}

// BYTE SEARCH
/** Finds the first exact byte sequence at or after start.
 *
 * @param data Bytes to search.
 * @param size Number of bytes in data.
 * @param start Byte offset to begin searching.
 * @param needle Sequence to find.
 * @param needleSize Number of bytes in needle.
 * @return The first match offset, or SIZE_MAX when absent.
 */
static size_t findBytes(const unsigned char *data, size_t size, size_t start,
                        const unsigned char *needle, size_t needleSize) {
    size_t position;
    if (needleSize == 0 || start > size || needleSize > size - start)
        return SIZE_MAX;
    for (position = start; position <= size - needleSize; position++) {
        if (memcmp(data + position, needle, needleSize) == 0)
            return position;
    }
    return SIZE_MAX;
}
/** Finds the last exact byte sequence in a range.
 *
 * @param data Bytes to search.
 * @param size Number of bytes in data.
 * @param needle Sequence to find.
 * @param needleSize Number of bytes in needle.
 * @return The last match offset, or SIZE_MAX when absent.
 */
static size_t findLastBytes(const unsigned char *data, size_t size, const unsigned char *needle,
                            size_t needleSize) {
    size_t position;
    if (needleSize == 0 || needleSize > size)
        return SIZE_MAX;
    position = size - needleSize;
    for (;;) {
        if (memcmp(data + position, needle, needleSize) == 0)
            return position;
        if (position == 0)
            break;
        position--;
    }
    return SIZE_MAX;
}

/*//////////////////////////////////////////////////
PDF READING AND WRITING
//////////////////////////////////////////////////*/

// DOCUMENT PARSING
/** Frees parsed object storage and resets a document for reuse or safe cleanup.
 *
 * @param document Parsed state whose allocations are owned by this module.
 */
static void pdfDocumentRelease(PdfDocument *document) {
    size_t index;
    if (document->objects != NULL) {
        for (index = 0; index < document->objectCount; index++)
            free(document->objects[index].data);
    }
    free(document->objects);
    free(document->xref);
    memset(document, 0, sizeof(*document));
}
/**
 * Copies one xref-selected object and records its dictionary and stream offsets relative to it.
 *
 * @param source Complete PDF bytes addressed by the xref table.
 * @param sourceSize Number of bytes in source.
 * @param offset Absolute byte offset of the object.
 * @param expectedNumber Object number named by the xref entry.
 * @param expectedGeneration Generation named by the xref entry.
 * @param object Receives the copied object and parsed stream metadata.
 * @return One for a supported object, or zero for malformed or unsupported syntax.
 */
static int parsePdfObject(const unsigned char *source, size_t sourceSize, size_t offset,
                          unsigned expectedNumber, unsigned expectedGeneration, PdfObject *object) {
    uint64_t number;
    uint64_t generation;
    size_t position = offset;
    size_t dictionaryStart = SIZE_MAX;
    size_t dictionaryEnd = SIZE_MAX;
    size_t objectEnd;
    size_t lengthStart = 0;
    size_t lengthEnd = 0;
    uint64_t streamLength = 0;
    size_t streamStart = 0;
    size_t markerStart = SIZE_MAX;
    int hasStream = 0;
    int isImage = 0;
    int hasFilter = 0;
    int hasRleFilter = 0;
    size_t filterEnd = 0;

    // READ OBJECT HEADER
    if (!parsePdfUnsigned(source, sourceSize, &position, &number) || number != expectedNumber ||
        !parsePdfUnsigned(source, sourceSize, &position, &generation) ||
        generation != expectedGeneration)
        return 0;
    position = skipPdfSpace(source, sourceSize, position);
    if (!pdfKeywordAt(source, sourceSize, position, "obj"))
        return 0;
    position += 3;
    position = skipPdfSpace(source, sourceSize, position);

    // INSPECT OBJECT DICTIONARY AND STREAM
    if (position + 1 < sourceSize && source[position] == '<' && source[position + 1] == '<') {
        uint64_t parsedLength;
        size_t streamKeyword;
        dictionaryStart = position;
        dictionaryEnd = findPdfDictionaryEnd(source, sourceSize, dictionaryStart);
        if (dictionaryEnd == SIZE_MAX)
            return 0;
        isImage = pdfEntryNameIs(source, sourceSize, dictionaryStart, dictionaryEnd, "Subtype",
                                 "Image", NULL, NULL);
        hasFilter = findPdfDictionaryEntry(source, sourceSize, dictionaryStart, dictionaryEnd,
                                           "Filter", NULL, NULL);
        if (hasFilter)
            hasRleFilter = pdfEntryNameIs(source, sourceSize, dictionaryStart, dictionaryEnd,
                                          "Filter", "RunLengthDecode", NULL, &filterEnd);
        markerStart = findBytes(source, dictionaryEnd, dictionaryStart,
                                (const unsigned char *)"%CRL1-IMAGE", 11);
        streamKeyword = skipPdfSpace(source, sourceSize, dictionaryEnd);
        if (pdfKeywordAt(source, sourceSize, streamKeyword, "stream")) {
            size_t afterStream = streamKeyword + 6;
            if (!pdfEntryUnsigned(source, sourceSize, dictionaryStart, dictionaryEnd, "Length",
                                  &lengthStart, &lengthEnd, &parsedLength) ||
                parsedLength > SIZE_MAX)
                return 0;
            if (afterStream < sourceSize && source[afterStream] == '\r') {
                afterStream++;
                if (afterStream < sourceSize && source[afterStream] == '\n')
                    afterStream++;
            } else if (afterStream < sourceSize && source[afterStream] == '\n') {
                afterStream++;
            } else {
                return 0;
            }
            if ((size_t)parsedLength > sourceSize - afterStream)
                return 0;
            streamStart = afterStream;
            position = streamStart + (size_t)parsedLength;
            position = skipPdfSpace(source, sourceSize, position);
            if (!pdfKeywordAt(source, sourceSize, position, "endstream"))
                return 0;
            position += 9;
            position = skipPdfSpace(source, sourceSize, position);
            if (!pdfKeywordAt(source, sourceSize, position, "endobj"))
                return 0;
            position += 6;
            objectEnd = position;
            streamLength = parsedLength;
            hasStream = 1;
        } else {
            objectEnd = findPdfToken(source, sourceSize, position, "endobj");
            if (objectEnd == SIZE_MAX)
                return 0;
            objectEnd += 6;
        }
    } else {
        objectEnd = findPdfToken(source, sourceSize, position, "endobj");
        if (objectEnd == SIZE_MAX)
            return 0;
        objectEnd += 6;
    }

    // COPY OBJECT DATA AND SAVE OFFSETS
    if (objectEnd < offset || objectEnd - offset > sourceSize - offset)
        return 0;
    object->size = objectEnd - offset;
    object->data = malloc(object->size);
    if (object->data == NULL)
        return 0;
    memcpy(object->data, source + offset, object->size);
    object->dictionaryStart = dictionaryStart == SIZE_MAX ? SIZE_MAX : dictionaryStart - offset;
    object->dictionaryEnd = dictionaryEnd == SIZE_MAX ? SIZE_MAX : dictionaryEnd - offset;
    object->lengthStart = hasStream ? lengthStart - offset : 0;
    object->lengthEnd = hasStream ? lengthEnd - offset : 0;
    object->streamStart = hasStream ? streamStart - offset : 0;
    object->streamSize = hasStream ? (size_t)streamLength : 0;
    object->filterEnd = hasRleFilter ? filterEnd - offset : 0;
    object->markerStart = markerStart == SIZE_MAX ? SIZE_MAX : markerStart - offset;
    object->hasStream = hasStream;
    object->isImage = isImage;
    object->hasFilter = hasFilter;
    object->hasRleFilter = hasRleFilter;
    return 1;
}
/**
 * Reads PDFs with one classic xref table and direct stream lengths.
 *
 * Encrypted, signed, incremental, object-stream, and xref-stream documents are rejected because
 * this module cannot rewrite those structures safely.
 *
 * @param data Complete PDF bytes.
 * @param size Number of bytes in data.
 * @param document Receives parsed objects, trailer, and xref entries on success.
 * @return One for the supported PDF subset, or zero when the structure cannot be rewritten.
 */
static int parsePdfDocument(const unsigned char *data, size_t size, PdfDocument *document) {
    static const unsigned char startxrefToken[] = "startxref";
    size_t marker;
    size_t position;
    size_t xrefOffset;
    uint64_t subsectionStart;
    uint64_t subsectionCount;
    uint64_t pdfSize;
    uint64_t value;
    size_t index;
    size_t minimumOffset = SIZE_MAX;
    memset(document, 0, sizeof(*document));
    if (data == NULL || size < sizeof(startxrefToken) - 1 || memcmp(data, "%PDF-", 5) != 0)
        return 0;

    // LOCATE THE CLASSIC CROSS-REFERENCE TABLE
    marker = findLastBytes(data, size, startxrefToken, sizeof(startxrefToken) - 1);
    if (marker == SIZE_MAX)
        return 0;
    position = marker + sizeof(startxrefToken) - 1;
    if (!parsePdfUnsigned(data, size, &position, &value) || value > SIZE_MAX)
        return 0;
    xrefOffset = (size_t)value;
    position = xrefOffset;
    if (!pdfKeywordAt(data, size, position, "xref"))
        return 0;
    position += 4;
    if (!parsePdfUnsigned(data, size, &position, &subsectionStart) ||
        !parsePdfUnsigned(data, size, &position, &subsectionCount) || subsectionStart != 0 ||
        subsectionCount < 2 || subsectionCount > size)
        return 0;

    // READ CROSS-REFERENCE ENTRIES
    document->objectCount = (size_t)subsectionCount;
    document->xref = calloc(document->objectCount, sizeof(*document->xref));
    document->objects = calloc(document->objectCount, sizeof(*document->objects));
    if (document->xref == NULL || document->objects == NULL)
        goto unsupported_pdf;
    for (index = 0; index < document->objectCount; index++) {
        uint64_t objectOffset;
        uint64_t generation;
        if (!parsePdfUnsigned(data, size, &position, &objectOffset) ||
            !parsePdfUnsigned(data, size, &position, &generation) || generation > UINT_MAX)
            goto unsupported_pdf;
        position = skipPdfSpace(data, size, position);
        if (position >= size || (data[position] != 'n' && data[position] != 'f'))
            goto unsupported_pdf;
        document->xref[index].offset = objectOffset;
        document->xref[index].generation = (unsigned)generation;
        document->xref[index].inUse = data[position] == 'n';
        position++;
        while (position < size && data[position] != '\n' && data[position] != '\r')
            position++;
        if (position < size && data[position] == '\r')
            position++;
        if (position < size && data[position] == '\n')
            position++;
    }

    // VALIDATE THE TRAILER
    position = skipPdfSpace(data, size, position);
    if (!pdfKeywordAt(data, size, position, "trailer"))
        goto unsupported_pdf;
    position = skipPdfSpace(data, size, position + 7);
    document->trailerStart = position;
    document->trailerSize = findPdfDictionaryEnd(data, size, position);
    if (document->trailerSize == SIZE_MAX)
        goto unsupported_pdf;
    document->trailerSize -= position;
    /* Keep trailer forms the serializer can preserve without dropping active document state. */
    if (!pdfEntryUnsigned(data, size, position, position + document->trailerSize, "Size", NULL,
                          NULL, &pdfSize) ||
        pdfSize != document->objectCount ||
        !findPdfDictionaryEntry(data, size, position, position + document->trailerSize, "Root",
                                NULL, NULL) ||
        findPdfDictionaryEntry(data, size, position, position + document->trailerSize, "Prev", NULL,
                               NULL) ||
        findPdfDictionaryEntry(data, size, position, position + document->trailerSize, "XRefStm",
                               NULL, NULL) ||
        findPdfDictionaryEntry(data, size, position, position + document->trailerSize, "Encrypt",
                               NULL, NULL) ||
        findBytes(data, size, 0, (const unsigned char *)"/ByteRange", sizeof("/ByteRange") - 1) !=
            SIZE_MAX ||
        findBytes(data, size, 0, (const unsigned char *)"/Linearized", sizeof("/Linearized") - 1) !=
            SIZE_MAX)
        goto unsupported_pdf;

    // LOAD ACTIVE OBJECTS
    // Object parsing rejects indirect stream lengths.
    if (document->xref[0].inUse)
        goto unsupported_pdf;
    for (index = 1; index < document->objectCount; index++) {
        PdfXrefEntry *entry = &document->xref[index];
        if (!entry->inUse)
            continue;
        if (entry->offset >= size)
            goto unsupported_pdf;
        if ((size_t)entry->offset < minimumOffset)
            minimumOffset = (size_t)entry->offset;
        if (!parsePdfObject(data, size, (size_t)entry->offset, (unsigned)index, entry->generation,
                            &document->objects[index]))
            goto unsupported_pdf;
        if (document->objects[index].hasStream &&
            pdfEntryNameIs(document->objects[index].data, document->objects[index].size,
                           document->objects[index].dictionaryStart,
                           document->objects[index].dictionaryEnd, "Type", "ObjStm", NULL, NULL))
            goto unsupported_pdf;
    }
    if (minimumOffset == SIZE_MAX)
        goto unsupported_pdf;
    document->firstObjectOffset = minimumOffset;
    return 1;
unsupported_pdf:
    pdfDocumentRelease(document);
    return 0;
}

// CROSS-REFERENCE SERIALIZATION
/** Appends a decimal integer followed by a line feed.
 *
 * @param buffer Destination PDF buffer.
 * @param value Unsigned value to serialize.
 * @return One when the complete line is appended, or zero on formatting or allocation failure.
 */
static int appendUnsignedLine(ByteBuffer *buffer, uint64_t value) {
    char line[32];
    int length = snprintf(line, sizeof(line), "%" PRIu64 "\n", value);
    if (length < 0 || (size_t)length >= sizeof(line))
        return 0;
    return bufferAppend(buffer, line, (size_t)length);
}
/**
 * Serializes parsed objects and rebuilds the classic xref table with their new offsets.
 *
 * @param source Original bytes used to preserve the header and trailer dictionary.
 * @param document Parsed objects and their xref metadata.
 * @param output Receives the serialized PDF bytes.
 * @return One when the complete document is written, or zero on allocation or size failure.
 */
static int serializePdfDocument(const unsigned char *source, const PdfDocument *document,
                                ByteBuffer *output) {
    uint64_t *offsets;
    size_t index;
    size_t xrefPosition;
    int success = 0;
    offsets = calloc(document->objectCount, sizeof(*offsets));
    if (offsets == NULL)
        return 0;

    // COPY THE HEADER AND OBJECTS
    if (!bufferAppend(output, source, document->firstObjectOffset))
        goto cleanup;
    if (output->size > 0 && output->data[output->size - 1] != '\n' &&
        output->data[output->size - 1] != '\r' && !bufferAppend(output, "\n", 1))
        goto cleanup;
    for (index = 1; index < document->objectCount; index++) {
        const PdfObject *object = &document->objects[index];
        if (!document->xref[index].inUse)
            continue;
        if (output->size > UINT64_C(9999999999))
            goto cleanup;
        offsets[index] = (uint64_t)output->size;
        if (!bufferAppend(output, object->data, object->size) ||
            ((object->size == 0 || object->data[object->size - 1] != '\n') &&
             !bufferAppend(output, "\n", 1)))
            goto cleanup;
    }

    // REBUILD THE CROSS-REFERENCE TABLE
    xrefPosition = output->size;
    if (!bufferAppend(output, "xref\n0 ", 7) ||
        !appendUnsignedLine(output, (uint64_t)document->objectCount))
        goto cleanup;
    for (index = 0; index < document->objectCount; index++) {
        uint64_t offset = index == 0 || !document->xref[index].inUse ? document->xref[index].offset
                                                                     : offsets[index];
        unsigned generation = document->xref[index].generation;
        char line[64];
        int length;
        if (generation > 99999 || offset > UINT64_C(9999999999))
            goto cleanup;
        length = snprintf(line, sizeof(line), "%010llu %05u %c\n", (unsigned long long)offset,
                          generation, document->xref[index].inUse ? 'n' : 'f');
        if (length < 0 || (size_t)length >= sizeof(line) ||
            !bufferAppend(output, line, (size_t)length))
            goto cleanup;
    }

    // APPEND THE TRAILER
    if (!bufferAppend(output, "trailer\n", 8) ||
        !bufferAppend(output, source + document->trailerStart, document->trailerSize) ||
        !bufferAppend(output, "\nstartxref\n", 11) ||
        !appendUnsignedLine(output, (uint64_t)xrefPosition) || !bufferAppend(output, "%%EOF\n", 6))
        goto cleanup;
    success = 1;
cleanup:
    free(offsets);
    return success;
}

// IMAGE STREAM REWRITING
/**
 * Replaces one stream and updates its direct Length entry.
 *
 * Compression adds this program's marker and filter; restoration removes those same entries.
 * The object is changed only after the replacement buffer has been built successfully.
 *
 * @param object Image XObject and parsed stream offsets to update.
 * @param replacement Encoded or restored stream bytes.
 * @param replacementSize Number of bytes in replacement.
 * @param compressing Nonzero to add the RLE marker and filter; zero to remove them.
 * @return One when the object is updated, or zero if the marker or allocation is invalid.
 */
static int replacePdfImageStream(PdfObject *object, const unsigned char *replacement,
                                 size_t replacementSize, int compressing) {
    ByteBuffer changed = {0};
    char lengthText[32];
    int lengthTextSize;
    size_t dictionaryClose = object->dictionaryEnd - 2;
    size_t oldStreamEnd = object->streamStart + object->streamSize;

    // BUILD THE UPDATED DICTIONARY
    lengthTextSize = snprintf(lengthText, sizeof(lengthText), "%zu", replacementSize);
    if (lengthTextSize < 0 || (size_t)lengthTextSize >= sizeof(lengthText))
        return 0;
    if (!bufferAppend(&changed, object->data, object->lengthStart) ||
        !bufferAppend(&changed, lengthText, (size_t)lengthTextSize))
        goto failure;
    if (compressing) {
        /* The marker lets restoration identify a filter this program added. */
        static const char markerAndFilter[] = "\n%CRL1-IMAGE\n/Filter /RunLengthDecode\n";
        if (!bufferAppend(&changed, object->data + object->lengthEnd,
                          dictionaryClose - object->lengthEnd) ||
            !bufferAppend(&changed, markerAndFilter, sizeof(markerAndFilter) - 1) ||
            !bufferAppend(&changed, object->data + dictionaryClose, 2))
            goto failure;
    } else {
        size_t markerStart = object->markerStart;
        size_t filterEnd = object->filterEnd;
        size_t removeStart;
        size_t removeEnd;
        if (markerStart == SIZE_MAX || filterEnd <= markerStart || markerStart < object->lengthEnd)
            goto failure;
        removeStart = markerStart;
        if (removeStart > 0 && object->data[removeStart - 1] == '\n') {
            removeStart--;
            if (removeStart > 0 && object->data[removeStart - 1] == '\r')
                removeStart--;
        }
        removeEnd = filterEnd;
        if (removeEnd < dictionaryClose && object->data[removeEnd] == '\r') {
            removeEnd++;
            if (removeEnd < dictionaryClose && object->data[removeEnd] == '\n')
                removeEnd++;
        } else if (removeEnd < dictionaryClose && object->data[removeEnd] == '\n') {
            removeEnd++;
        }
        if (removeStart < object->lengthEnd || removeEnd > dictionaryClose ||
            !bufferAppend(&changed, object->data + object->lengthEnd,
                          removeStart - object->lengthEnd) ||
            !bufferAppend(&changed, object->data + removeEnd, dictionaryClose - removeEnd) ||
            !bufferAppend(&changed, object->data + dictionaryClose, 2))
            goto failure;
    }

    // APPEND THE REPLACEMENT STREAM
    if (!bufferAppend(&changed, object->data + object->dictionaryEnd,
                      object->streamStart - object->dictionaryEnd) ||
        !bufferAppend(&changed, replacement, replacementSize) ||
        !bufferAppend(&changed, object->data + oldStreamEnd, object->size - oldStreamEnd))
        goto failure;

    // COMMIT THE UPDATED OBJECT
    free(object->data);
    object->data = changed.data;
    object->size = changed.size;
    return 1;
failure:
    bufferRelease(&changed);
    return 0;
}
/**
 * Rewrites supported PDFs by compressing image streams or restoring streams marked by this code.
 *
 * Compression returns a serialized document only when the whole file becomes smaller. When
 * parsing succeeds but nothing changes, output is NULL, outputSize is zero, and changedPdf is zero.
 *
 * @param input Complete PDF bytes.
 * @param inputSize Number of input bytes.
 * @param decompressing Nonzero to restore marked image streams.
 * @param output Receives owned serialized bytes when the document changes.
 * @param outputSize Receives the serialized length, or zero when unchanged.
 * @param changedPdf Receives one when output contains a transformed PDF.
 * @return One when parsing and transformation complete, or zero for unsupported syntax or failure.
 */
static int transformPdfDocument(const unsigned char *input, size_t inputSize, int decompressing,
                                unsigned char **output, size_t *outputSize, int *changedPdf) {
    PdfDocument document = {0};
    ByteBuffer serialized = {0};
    size_t index;
    int changed = 0;

    // PARSE THE DOCUMENT
    if (output == NULL || outputSize == NULL || changedPdf == NULL ||
        !parsePdfDocument(input, inputSize, &document))
        return 0;

    // TRANSFORM ELIGIBLE IMAGE STREAMS
    for (index = 1; index < document.objectCount; index++) {
        PdfObject *object = &document.objects[index];
        unsigned char *replacement = NULL;
        size_t replacementSize = 0;
        int shouldReplace = 0;
        if (!object->hasStream || !object->isImage)
            continue;
        if (decompressing) {
            if (object->markerStart == SIZE_MAX || !object->hasRleFilter)
                continue;
            if (!runLengthDecode(object->data + object->streamStart, object->streamSize, SIZE_MAX,
                                 &replacement, &replacementSize))
                goto failure;
            shouldReplace = 1;
        } else {
            ByteBuffer encoded = {0};
            if (object->hasFilter)
                continue;
            if (!runLengthEncode(object->data + object->streamStart, object->streamSize,
                                 &encoded)) {
                bufferRelease(&encoded);
                goto failure;
            }
            if (encoded.size < object->streamSize) {
                replacement = encoded.data;
                replacementSize = encoded.size;
                shouldReplace = 1;
            } else {
                bufferRelease(&encoded);
            }
        }
        if (shouldReplace) {
            if (!replacePdfImageStream(object, replacement, replacementSize, !decompressing)) {
                free(replacement);
                goto failure;
            }
            free(replacement);
            changed = 1;
        }
    }

    // RETURN WHEN NO STREAM CHANGED
    if (!changed) {
        *output = NULL;
        *outputSize = 0;
        *changedPdf = 0;
        pdfDocumentRelease(&document);
        return 1;
    }

    // SERIALIZE AND CHECK THE FINAL SIZE
    if (!serializePdfDocument(input, &document, &serialized))
        goto failure;
    if (!decompressing && serialized.size >= inputSize) {
        bufferRelease(&serialized);
        *output = NULL;
        *outputSize = 0;
        *changedPdf = 0;
        pdfDocumentRelease(&document);
        return 1;
    }
    *output = serialized.data;
    *outputSize = serialized.size;
    *changedPdf = 1;
    pdfDocumentRelease(&document);
    return 1;
failure:
    bufferRelease(&serialized);
    pdfDocumentRelease(&document);
    return 0;
}

/*//////////////////////////////////////////////////
FILE FLOWS
//////////////////////////////////////////////////*/

// FILE TRANSFORMS
int compressFile(const char *inputPath, const char *outputPath) {
    file_t inputFile = {0};
    unsigned char *output = NULL;
    size_t outputSize = 0;
    int changedPdf = 0;
    int success;

    // VALIDATE AND READ INPUT
    if (inputPath == NULL || outputPath == NULL || strcmp(inputPath, outputPath) == 0)
        return 0;
    if (!readInputFile(inputPath, &inputFile))
        return 0;

    // COMPRESS THE PDF OR CONTAINER
    if (inputFile.type == 'p') {
        if (!transformPdfDocument(inputFile.data, inputFile.dataSize, 0, &output, &outputSize,
                                  &changedPdf)) {
            if (inputFile.dataSize < 5 || memcmp(inputFile.data, "%PDF-", 5) != 0) {
                free(inputFile.data);
                fprintf(stderr, "The input is not a readable PDF: %s\n", inputPath);
                return 0;
            }
            fprintf(stderr, "This PDF uses an unsupported structure; copying it unchanged.\n");
            output = inputFile.data;
            outputSize = inputFile.dataSize;
        } else if (!changedPdf) {
            fprintf(stderr, "No smaller eligible PDF image stream was found; copying unchanged.\n");
            output = inputFile.data;
            outputSize = inputFile.dataSize;
        }
    } else if (!encodeContainer(inputFile.data, inputFile.dataSize, &output, &outputSize)) {
        free(inputFile.data);
        fprintf(stderr, "Could not encode the input file.\n");
        return 0;
    }

    // WRITE OUTPUT ONCE
    success = writeOutputFile(outputPath, output, outputSize);
    if (output != inputFile.data)
        free(output);
    free(inputFile.data);
    return success;
}

int decompressFile(const char *inputPath, const char *outputPath) {
    file_t inputFile = {0};
    unsigned char *output = NULL;
    size_t outputSize = 0;
    int changedPdf = 0;
    int success;

    // VALIDATE AND READ INPUT
    if (inputPath == NULL || outputPath == NULL || strcmp(inputPath, outputPath) == 0)
        return 0;
    if (!readInputFile(inputPath, &inputFile))
        return 0;

    // RESTORE THE PDF OR DECODE THE CONTAINER
    if (inputFile.type == 'p') {
        if (!transformPdfDocument(inputFile.data, inputFile.dataSize, 1, &output, &outputSize,
                                  &changedPdf) ||
            !changedPdf) {
            free(inputFile.data);
            fprintf(stderr, "The PDF has no supported RLE image streams to restore.\n");
            return 0;
        }
    } else if (!decodeContainer(inputFile.data, inputFile.dataSize, &output, &outputSize)) {
        free(inputFile.data);
        fprintf(stderr, "The input is not a valid CRL1 container.\n");
        return 0;
    }

    // WRITE OUTPUT ONCE
    success = writeOutputFile(outputPath, output, outputSize);
    free(output);
    free(inputFile.data);
    return success;
}
