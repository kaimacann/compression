/**
 * @file codec.c Original PNG, zlib, and DEFLATE codecs.
 * @author kai
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* SHARED CONTRACT*/
#include "codec.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

enum {
    BITS_PER_BYTE = 8,
    ADAM7_INITIAL_STEP = 8,
    PNG_SIGNATURE_LENGTH = 8,
    DEFLATE_MAX_CODE_BITS = 15,
    DEFLATE_CODE_LENGTH_SLOTS = DEFLATE_MAX_CODE_BITS + 1,
    DEFLATE_CODE_LENGTHS_CAPACITY = 320,
    DEFLATE_FIXED_LITERAL_SYMBOLS = 288,
    DEFLATE_FIXED_DISTANCE_SYMBOLS = 32,
    DEFLATE_MAX_LITERAL_SYMBOLS = 286,
    DEFLATE_MAX_DISTANCE_SYMBOLS = 32,
    DEFLATE_USABLE_DISTANCE_SYMBOLS = 30,
    DEFLATE_CODE_LENGTH_SYMBOLS = 19,
    DEFLATE_STORED_LENGTH_BITS = 16,
    DEFLATE_STORED_LENGTH_MAX = 65535,
    DEFLATE_REPEAT_PREVIOUS_SYMBOL = 16,
    DEFLATE_REPEAT_SHORT_ZERO_SYMBOL = 17,
    DEFLATE_REPEAT_LONG_ZERO_SYMBOL = 18,
    DEFLATE_RESERVED_DISTANCE_SYMBOLS = 2,
    ADLER_COMPONENT_BITS = 16,
    DEFLATE_LITERAL_SYMBOL_LIMIT = 256,
    DEFLATE_END_OF_BLOCK_SYMBOL = 256,
    DEFLATE_FIRST_LENGTH_SYMBOL = 257,
    DEFLATE_MAX_MATCH_LENGTH = 258,
    DEFLATE_WINDOW_SIZE = 32768,
    DEFLATE_HASH_TABLE_SIZE = 32768,
    DEFLATE_HASH_MASK = 32767,
    DEFLATE_MAX_CHAIN_ATTEMPTS = 64,
    DEFLATE_HASH_MULTIPLIER_FIRST = 251,
    DEFLATE_HASH_MULTIPLIER_SECOND = 31,
    DEFLATE_MIN_MATCH_LENGTH = 3,
    DEFLATE_LITERAL_END = 256,
    DEFLATE_MAX_LENGTH_SYMBOL = 285,
    DEFLATE_LAST_LENGTH_BASE_INDEX = 28,
    DEFLATE_LAST_DISTANCE_BASE_INDEX = 29,
    DEFLATE_FIXED_LITERAL_FIRST_BOUNDARY = 144,
    DEFLATE_FIXED_LITERAL_SECOND_BOUNDARY = 256,
    DEFLATE_FIXED_LENGTH_BOUNDARY = 280,
    DEFLATE_FIXED_LITERAL_FIRST_CODE = 48,
    DEFLATE_FIXED_LITERAL_SECOND_CODE = 400,
    DEFLATE_FIXED_LENGTH_CODE = 192,
    DEFLATE_FIXED_LONG_CODE_BITS = 9,
    DEFLATE_FIXED_SHORT_CODE_BITS = 7,
    DEFLATE_FIXED_DISTANCE_CODE_BITS = 5,
    DEFLATE_CODE_LENGTH_SYMBOL_6 = 6,
    DEFLATE_CODE_LENGTH_SYMBOL_7 = 7,
    DEFLATE_CODE_LENGTH_SYMBOL_5 = 5,
    DEFLATE_CODE_LENGTH_SYMBOL_9 = 9,
    DEFLATE_CODE_LENGTH_SYMBOL_10 = 10,
    DEFLATE_CODE_LENGTH_SYMBOL_11 = 11,
    DEFLATE_DYNAMIC_COUNT_BITS = 5,
    DEFLATE_DYNAMIC_COUNT_OFFSET = 1,
    DEFLATE_CODE_LENGTH_COUNT_BITS = 4,
    DEFLATE_CODE_LENGTH_COUNT_OFFSET = 4,
    DEFLATE_CODE_LENGTH_VALUE_BITS = 3,
    DEFLATE_CODE_LENGTH_DIRECT_LIMIT = 16,
    DEFLATE_REPEAT_PREVIOUS_BITS = 2,
    DEFLATE_REPEAT_PREVIOUS_OFFSET = 3,
    DEFLATE_REPEAT_SHORT_ZERO_BITS = 3,
    DEFLATE_REPEAT_SHORT_ZERO_OFFSET = 3,
    DEFLATE_REPEAT_LONG_ZERO_BITS = 7,
    DEFLATE_REPEAT_LONG_ZERO_OFFSET = 11,
    ZLIB_TRAILER_LENGTH = 4,
    RC_INITIAL_BUFFER_CAPACITY = 256,
    PNG_PALETTE_ENTRY_COUNT = 256,
    PNG_RGBA_CHANNEL_COUNT = 4,
    PNG_MAX_SAMPLE_8_BIT = 255,
    PNG_OPAQUE_ALPHA = 255,
    PNG_SAMPLE_DEPTH_16 = 16,
    PNG_SAMPLE_DEPTH_1 = 1,
    PNG_SAMPLE_DEPTH_2 = 2,
    PNG_SAMPLE_DEPTH_4 = 4,
    PNG_ALPHA_CHANNEL_INDEX = 3,
    PNG_HEADER_COLOR_TYPE_OFFSET = 9,
    PNG_HEADER_INTERLACE_OFFSET = 12,
    PNG_TRANSPARENCY_GRAY_LENGTH = 2,
    PNG_SAMPLE_RANGE_16 = 65536,
    PNG_MAX_PALETTE_BYTES = 768,
    PNG_CHUNK_OVERHEAD = 12,
    PNG_HEADER_LENGTH = 13,
    PNG_IHDR_RESERVED_FIRST_OFFSET = 10,
    PNG_IHDR_RESERVED_SECOND_OFFSET = 11,
    PNG_MAX_COLOR_TYPE = 6,
    PNG_ADAM7_PASS_COUNT = 7,
    PNG_RGB_CHANNEL_COUNT = 3,
    PNG_COLOR_CHANNEL_TABLE_SIZE = 7,
    PNG_ANCILLARY_CHUNK_FLAG = 32,
    PNG_TRANS_RGB_LENGTH = 6,
    PNG_COLOR_TYPE_RGB = 2,
    PNG_COLOR_TYPE_GRAY = 0,
    PNG_COLOR_TYPE_GRAY_ALPHA = 4,
    PNG_COLOR_TYPE_RGBA = 6,
    PNG_COLOR_TYPE_INDEXED = 3,
    ZLIB_MINIMUM_STREAM_LENGTH = 6,
    ZLIB_MAX_WINDOW_EXPONENT = 7,
    ZLIB_HEADER_DIVISOR = 31,
    ZLIB_CMF_METHOD_MASK = 15,
    ZLIB_DICTIONARY_FLAG = 32,
    ZLIB_CHECKSUM_FIRST_SHIFT = 24,
    ZLIB_HEADER_FIRST_BYTE = 0x78,
    JPEG_START_OF_IMAGE_END = 216,
};

#define PNG_CRC_INITIAL_VALUE 0xffffffffUL
#define PNG_CRC_POLYNOMIAL 0xedb88320UL
#define ZLIB_ADLER_MODULUS 65521UL

/* BIT STREAMS*/
typedef struct {
    const unsigned char *p;
    size_t n, at;
    unsigned long bits;
    unsigned count;
    int bad;
} Bits;
typedef struct {
    unsigned count[DEFLATE_CODE_LENGTH_SLOTS], symbols[DEFLATE_CODE_LENGTHS_CAPACITY];
} Huff;
typedef struct {
    const Huff *literals;
    const Huff *distances;
} HuffTables;
typedef struct {
    RcBuffer *out;
    unsigned long bits;
    unsigned count;
    int bad;
} Writer;
typedef struct {
    unsigned width;
    unsigned height;
    unsigned depth;
    unsigned type;
    unsigned interlace;
    unsigned channels;
    unsigned paletteCount;
    unsigned char palette[PNG_PALETTE_ENTRY_COUNT][PNG_RGBA_CHANNEL_COUNT];
    unsigned transparency[PNG_RGB_CHANNEL_COUNT];
} PngInfo;
typedef struct {
    int header;
    int ended;
    int haveData;
    int closedData;
} PngChunkState;
typedef struct {
    const unsigned char *name;
    const unsigned char *body;
    unsigned long length;
} PngChunk;
typedef struct {
    unsigned value;
    unsigned bitCount;
} BitPattern;

/* CANONICAL TABLES*/
static const unsigned lbase[29] = {
    3,  4,  5,  6,  7,  BITS_PER_BYTE, 9,   10,  11,  13,  15,  17, 19, 23, 27, 31, 35, 43,
    51, 59, 67, 83, 99, 115,           131, 163, 195, 227, 258,
};
static const unsigned char lextra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0,
};
static const unsigned dbase[30] = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577,
};
static const unsigned char dextra[30] = {
    0, 0, 0,  0,  1,  1,  2,  2,  3,  3,  4, 4, 5, 5, 6, 6, 7, 7, BITS_PER_BYTE, BITS_PER_BYTE,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13,
};

/*//////////////////////////////////////////////////
BUFFERS
//////////////////////////////////////////////////*/

/* STORAGE*/
/**
 * Copy a bounded byte range without relying on libc's unchecked buffer helpers.
 * @param source Readable memory containing length bytes.
 * @param length Number of bytes to copy.
 * @param destination Writable memory with room for length bytes.
 * @author kai
 */
static void copyBytes(const void *source, size_t length, void *destination) {
    unsigned char *destinationBytes = destination;
    const unsigned char *sourceBytes = source;
    size_t index;
    for (index = 0; index < length; index++) {
        destinationBytes[index] = sourceBytes[index];
    }
}

typedef struct {
    unsigned char value;
    size_t length;
} FillBytesOptions;

/**
 * Fill a bounded byte range with one byte value.
 * @param destination Writable memory with room for fillOptions.length bytes.
 * @param fillOptions Byte value and number of bytes to fill.
 * @author kai
 */
static void fillBytes(void *destination, FillBytesOptions fillOptions) {
    unsigned char *destinationBytes = destination;
    size_t index;
    for (index = 0; index < fillOptions.length; index++) {
        destinationBytes[index] = fillOptions.value;
    }
}

/**
 * Append bytes to a buffer, growing it up to the shared size limit.
 * @param buffer Buffer that owns the appended bytes.
 * @param source Bytes to copy into the buffer.
 * @param length Number of bytes to append.
 * @return Nonzero when the buffer owns the appended bytes; zero on limit or allocation failure.
 * @author kai
 */
int rcAppend(RcBuffer *buffer, const void *source, size_t length) {
    size_t cap;
    unsigned char *grownData;
    if (length > RC_LIMIT || buffer->size > RC_LIMIT - length) {
        return 0;
    }
    if (buffer->size + length > buffer->capacity) {
        cap = buffer->capacity ? buffer->capacity : RC_INITIAL_BUFFER_CAPACITY;
        while (cap < buffer->size + length) {
            cap = cap > RC_LIMIT / 2 ? RC_LIMIT : cap * 2;
        }
        grownData = realloc(buffer->data, cap);
        if (!grownData) {
            return 0;
        }
        buffer->data = grownData;
        buffer->capacity = cap;
    }
    if (length) {
        copyBytes(source, length, buffer->data + buffer->size);
    }
    buffer->size += length;
    return 1;
}

/* CHECKSUMS*/
/**
 * Compute the PNG CRC-32 used to validate chunks.
 * @param data PNG chunk bytes covered by the checksum.
 * @param length Number of bytes in the chunk data.
 * @return CRC-32 value used by the PNG chunk verifier.
 * @author kai
 */
unsigned long rcCrc(const unsigned char *data, size_t length) {
    unsigned long checksum = PNG_CRC_INITIAL_VALUE;
    size_t byteIndex;
    int bitIndex;
    for (byteIndex = 0; byteIndex < length; byteIndex++) {
        checksum ^= data[byteIndex];
        for (bitIndex = 0; bitIndex < BITS_PER_BYTE; bitIndex++) {
            checksum = (checksum >> 1U) ^ ((checksum & 1UL) ? PNG_CRC_POLYNOMIAL : 0UL);
        }
    }
    return checksum ^ PNG_CRC_INITIAL_VALUE;
}
/**
 * Compute the Adler-32 checksum stored at the end of a zlib stream.
 * @param data Uncompressed zlib payload.
 * @param length Payload length in bytes.
 * @return Adler-32 checksum of the uncompressed payload.
 * @author kai
 */
static unsigned long adler(const unsigned char *data, size_t length) {
    unsigned long sumA = 1;
    unsigned long sumB = 0;
    size_t byteIndex;
    for (byteIndex = 0; byteIndex < length; byteIndex++) {
        sumA = (sumA + data[byteIndex]) % ZLIB_ADLER_MODULUS;
        sumB = (sumB + sumA) % ZLIB_ADLER_MODULUS;
    }
    return (sumB << (unsigned)ADLER_COMPONENT_BITS) | sumA;
}
/**
 * Read one big-endian 32-bit integer.
 * @param data Four bytes in network byte order.
 * @return The unsigned 32-bit value represented by the four bytes.
 * @author kai
 */
static unsigned long be32(const unsigned char *data) {
    return ((unsigned long)data[0] << (unsigned)(BITS_PER_BYTE * 3)) |
           ((unsigned long)data[1] << (unsigned)(BITS_PER_BYTE * 2)) |
           ((unsigned long)data[2] << (unsigned)BITS_PER_BYTE) | (unsigned long)data[3];
}

/*//////////////////////////////////////////////////
DEFLATE
//////////////////////////////////////////////////*/

/* DECODING*/
/**
 * Read low-order bits from the DEFLATE bit stream.
 * @param reader DEFLATE bit reader whose cursor advances.
 * @param bitCount Number of low-order bits to consume.
 * @return The requested bits, or zero while marking the reader invalid on truncation.
 * @author kai
 */
static unsigned readBits(Bits *reader, unsigned bitCount) {
    unsigned value;
    while (reader->count < bitCount) {
        if (reader->at == reader->n) {
            reader->bad = 1;
            return 0;
        }
        reader->bits |= (unsigned long)reader->p[reader->at++] << reader->count;
        reader->count += BITS_PER_BYTE;
    }
    value = (unsigned)(reader->bits & ((1UL << bitCount) - 1));
    reader->bits >>= bitCount;
    reader->count -= bitCount;
    return value;
}
/**
 * Build a canonical Huffman lookup table from code lengths; reject oversubscribed tables.
 * @param table Table to initialize with canonical code counts and symbols.
 * @param lengths Code length for each symbol.
 * @param lengthCount Number of symbol lengths supplied.
 * @return Nonzero for a valid canonical table; zero for invalid code lengths.
 * @author kai
 */
static int makeHuff(Huff *table, const unsigned char *lengths, unsigned lengthCount) {
    unsigned offsets[DEFLATE_CODE_LENGTH_SLOTS] = {0};
    unsigned symbolIndex;
    unsigned codeLength;
    int left = 1;
    *table = (Huff){0};
    for (symbolIndex = 0; symbolIndex < lengthCount; symbolIndex++) {
        if (lengths[symbolIndex] > DEFLATE_MAX_CODE_BITS) {
            return 0;
        }
        table->count[lengths[symbolIndex]]++;
    }
    for (codeLength = 1; codeLength <= DEFLATE_MAX_CODE_BITS; codeLength++) {
        left = (left * 2) - (int)table->count[codeLength];
        if (left < 0) {
            return 0;
        }
    }
    if (table->count[0] == lengthCount) {
        return 0;
    }
    for (codeLength = 1; codeLength < DEFLATE_MAX_CODE_BITS; codeLength++) {
        offsets[codeLength + 1] = offsets[codeLength] + table->count[codeLength];
    }
    for (symbolIndex = 0; symbolIndex < lengthCount; symbolIndex++) {
        if (lengths[symbolIndex]) {
            table->symbols[offsets[lengths[symbolIndex]]++] = symbolIndex;
        }
    }
    return 1;
}
/**
 * Decode one symbol from a canonical DEFLATE Huffman table.
 * @param reader Bit reader positioned at the next Huffman code.
 * @param table Canonical table used to map the code to a symbol.
 * @return Decoded symbol, or -1 if the bit stream cannot form a table code.
 * @author kai
 */
static int decodeHuff(Bits *reader, const Huff *table) {
    unsigned code = 0;
    unsigned first = 0;
    unsigned index = 0;
    unsigned codeBits;
    for (codeBits = 1; codeBits <= DEFLATE_MAX_CODE_BITS; codeBits++) {
        code |= readBits(reader, 1);
        if (reader->bad) {
            return -1;
        }
        if (code >= first && code - first < table->count[codeBits]) {
            return (int)table->symbols[index + code - first];
        }
        index += table->count[codeBits];
        first = (first + table->count[codeBits]) << 1U;
        code <<= 1U;
    }
    reader->bad = 1;
    return -1;
}

/**
 * Expand one DEFLATE code-length symbol into the declared length list.
 * @param reader Current DEFLATE bit reader.
 * @param table Huffman code-length table.
 * @param lengths Receives expanded literal and distance lengths.
 * @param index Current output index, advanced by the decoded run.
 * @param limit Maximum output index.
 * @return Nonzero when the run is valid and fits the declared alphabets.
 * @author kai
 */
static int readCodeLengthRun(Bits *reader, const Huff *table, unsigned char lengths[],
                             unsigned *index, unsigned limit) {
    int symbol = decodeHuff(reader, table);
    unsigned run;
    unsigned value;
    if (symbol < 0) {
        return 0;
    }
    if (symbol < DEFLATE_CODE_LENGTH_DIRECT_LIMIT) {
        if (*index >= limit) {
            return 0;
        }
        lengths[(*index)++] = (unsigned char)symbol;
        return 1;
    }
    if (symbol == DEFLATE_REPEAT_PREVIOUS_SYMBOL) {
        if (!*index) {
            return 0;
        }
        run = readBits(reader, DEFLATE_REPEAT_PREVIOUS_BITS) + DEFLATE_REPEAT_PREVIOUS_OFFSET;
        value = lengths[*index - 1];
    } else if (symbol == DEFLATE_REPEAT_SHORT_ZERO_SYMBOL) {
        run = readBits(reader, DEFLATE_REPEAT_SHORT_ZERO_BITS) + DEFLATE_REPEAT_SHORT_ZERO_OFFSET;
        value = 0;
    } else if (symbol == DEFLATE_REPEAT_LONG_ZERO_SYMBOL) {
        run = readBits(reader, DEFLATE_REPEAT_LONG_ZERO_BITS) + DEFLATE_REPEAT_LONG_ZERO_OFFSET;
        value = 0;
    } else {
        return 0;
    }
    if (run > limit - *index) {
        return 0;
    }
    while (run--) {
        lengths[(*index)++] = (unsigned char)value;
    }
    return !reader->bad;
}

/**
 * Read dynamic DEFLATE alphabets from the current block header.
 * @param reader Current DEFLATE bit reader.
 * @param literals Receives the literal/length Huffman tree.
 * @param distances Receives the distance Huffman tree.
 * @return Nonzero when both alphabets are valid.
 * @author kai
 */
static int readDynamicTables(Bits *reader, Huff *literals, Huff *distances) {
    static const unsigned char order[DEFLATE_CODE_LENGTH_SYMBOLS] = {
        DEFLATE_REPEAT_PREVIOUS_SYMBOL,
        DEFLATE_REPEAT_SHORT_ZERO_SYMBOL,
        DEFLATE_REPEAT_LONG_ZERO_SYMBOL,
        0,
        BITS_PER_BYTE,
        DEFLATE_CODE_LENGTH_SYMBOL_7,
        DEFLATE_CODE_LENGTH_SYMBOL_9,
        DEFLATE_CODE_LENGTH_SYMBOL_6,
        DEFLATE_CODE_LENGTH_SYMBOL_10,
        DEFLATE_CODE_LENGTH_SYMBOL_5,
        DEFLATE_CODE_LENGTH_SYMBOL_11,
        4,
        12,
        3,
        13,
        2,
        14,
        1,
        DEFLATE_MAX_CODE_BITS,
    };
    unsigned literalCount =
        readBits(reader, DEFLATE_DYNAMIC_COUNT_BITS) + DEFLATE_FIRST_LENGTH_SYMBOL;
    unsigned distanceCount =
        readBits(reader, DEFLATE_DYNAMIC_COUNT_BITS) + DEFLATE_DYNAMIC_COUNT_OFFSET;
    unsigned codeCount =
        readBits(reader, DEFLATE_CODE_LENGTH_COUNT_BITS) + DEFLATE_CODE_LENGTH_COUNT_OFFSET;
    unsigned index = 0;
    unsigned orderIndex;
    unsigned char lengths[DEFLATE_CODE_LENGTHS_CAPACITY] = {0};
    unsigned char codeLengths[DEFLATE_CODE_LENGTH_SYMBOLS] = {0};
    unsigned char distanceLengths[DEFLATE_MAX_DISTANCE_SYMBOLS] = {0};
    Huff codeTable;
    unsigned totalCount = literalCount + distanceCount;
    if (literalCount > DEFLATE_MAX_LITERAL_SYMBOLS ||
        distanceCount > DEFLATE_MAX_DISTANCE_SYMBOLS) {
        return 0;
    }
    for (orderIndex = 0; orderIndex < codeCount; orderIndex++) {
        codeLengths[order[orderIndex]] =
            (unsigned char)readBits(reader, DEFLATE_CODE_LENGTH_VALUE_BITS);
    }
    if (reader->bad || !makeHuff(&codeTable, codeLengths, DEFLATE_CODE_LENGTH_SYMBOLS)) {
        return 0;
    }
    while (index < totalCount) {
        if (!readCodeLengthRun(reader, &codeTable, lengths, &index, totalCount)) {
            return 0;
        }
    }
    if (!lengths[DEFLATE_END_OF_BLOCK_SYMBOL] || !makeHuff(literals, lengths, literalCount)) {
        return 0;
    }
    copyBytes(lengths + literalCount, distanceCount, distanceLengths);
    if (!makeHuff(distances, distanceLengths, distanceCount)) {
        *distances = (Huff){0};
    }
    return 1;
}

/**
 * Copy a stored DEFLATE block after validating its length complement.
 * @param reader Current DEFLATE bit reader.
 * @param out Receives stored payload bytes.
 * @return Nonzero when the complete block fits and is appended.
 * @author kai
 */
static int readStoredBlock(Bits *reader, RcBuffer *out) {
    unsigned length;
    unsigned inverseLength;
    reader->bits = 0;
    reader->count = 0;
    length = readBits(reader, DEFLATE_STORED_LENGTH_BITS);
    inverseLength = readBits(reader, DEFLATE_STORED_LENGTH_BITS);
    if (reader->bad || (length ^ inverseLength) != DEFLATE_STORED_LENGTH_MAX ||
        length > reader->n - reader->at || !rcAppend(out, reader->p + reader->at, length)) {
        return 0;
    }
    reader->at += length;
    return 1;
}

/**
 * Build fixed Huffman tables for a DEFLATE block.
 * @param literals Receives the fixed literal/length tree.
 * @param distances Receives the fixed distance tree.
 * @return Nonzero when both fixed trees are valid.
 * @author kai
 */
static int buildFixedTables(Huff *literals, Huff *distances) {
    unsigned char lengths[DEFLATE_FIXED_LITERAL_SYMBOLS] = {0};
    unsigned char distanceLengths[DEFLATE_FIXED_DISTANCE_SYMBOLS];
    unsigned index;
    for (index = 0; index < DEFLATE_FIXED_LITERAL_SYMBOLS; index++) {
        if (index < DEFLATE_FIXED_LITERAL_FIRST_BOUNDARY ||
            index >= DEFLATE_FIXED_LENGTH_BOUNDARY) {
            lengths[index] = BITS_PER_BYTE;
        } else if (index < DEFLATE_FIXED_LITERAL_SECOND_BOUNDARY) {
            lengths[index] = DEFLATE_FIXED_LONG_CODE_BITS;
        } else {
            lengths[index] = DEFLATE_FIXED_SHORT_CODE_BITS;
        }
    }
    for (index = 0; index < DEFLATE_FIXED_DISTANCE_SYMBOLS; index++) {
        distanceLengths[index] = DEFLATE_FIXED_DISTANCE_CODE_BITS;
    }
    return makeHuff(literals, lengths, DEFLATE_FIXED_LITERAL_SYMBOLS) &&
           makeHuff(distances, distanceLengths, DEFLATE_FIXED_DISTANCE_SYMBOLS);
}

/**
 * Copy one bounded length/distance match from previously decoded output.
 * @param reader Current DEFLATE bit reader.
 * @param out Existing decoded output that supplies the match history.
 * @param distances Distance Huffman table.
 * @param symbol Literal/length symbol to process.
 * @return Nonzero when a valid match is copied.
 * @author kai
 */
static int copyInflateMatch(Bits *reader, RcBuffer *out, const Huff *distances, int symbol) {
    unsigned lengthIndex = (unsigned)symbol - DEFLATE_FIRST_LENGTH_SYMBOL;
    unsigned length = lbase[lengthIndex] + readBits(reader, lextra[lengthIndex]);
    int distanceSymbol = decodeHuff(reader, distances);
    unsigned distance;
    unsigned char byte;
    if (distanceSymbol < 0 ||
        distanceSymbol >= DEFLATE_MAX_DISTANCE_SYMBOLS - DEFLATE_RESERVED_DISTANCE_SYMBOLS) {
        return 0;
    }
    distance = dbase[distanceSymbol] + readBits(reader, dextra[distanceSymbol]);
    if (reader->bad || distance > out->size || length > RC_LIMIT - out->size) {
        return 0;
    }
    while (length--) {
        byte = out->data[out->size - distance];
        if (!rcAppend(out, &byte, 1)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Decode literals and matches from one Huffman-coded DEFLATE block.
 * @param reader Current DEFLATE bit reader.
 * @param out Receives decoded bytes.
 * @param tables Literal/length and distance Huffman tables.
 * @return Nonzero when the end-of-block symbol is reached.
 * @author kai
 */
static int decodeHuffmanBlock(Bits *reader, RcBuffer *out, HuffTables tables) {
    for (;;) {
        int symbol = decodeHuff(reader, tables.literals);
        if (symbol < 0) {
            return 0;
        }
        if (symbol == DEFLATE_END_OF_BLOCK_SYMBOL) {
            return 1;
        }
        if (symbol < DEFLATE_LITERAL_SYMBOL_LIMIT) {
            unsigned char byte = (unsigned char)symbol;
            if (!rcAppend(out, &byte, 1)) {
                return 0;
            }
        } else if (symbol > DEFLATE_MAX_LITERAL_SYMBOLS - 1 ||
                   !copyInflateMatch(reader, out, tables.distances, symbol)) {
            return 0;
        }
    }
}
/**
 * Decode and validate one zlib stream.
 * @param inputData Complete zlib stream, including header and Adler checksum.
 * @param inputLength Encoded stream length in bytes.
 * @param out Receives decoded bytes; release its storage even after failure.
 * @return 1 only when payload and Adler checksum agree.
 * @author kai
 */
int rcInflate(const unsigned char *inputData, size_t inputLength, RcBuffer *out) {
    Bits bitReader = {0};
    int finalBlock = 0;

    /* VALIDATE THE ZLIB ENVELOPE*/
    /* Reject unsupported windows and preset dictionaries before reading entropy bits.*/
    if (inputLength < ZLIB_MINIMUM_STREAM_LENGTH ||
        ((unsigned)inputData[0] & (unsigned)ZLIB_CMF_METHOD_MASK) != BITS_PER_BYTE ||
        ((unsigned)inputData[0] >> 4U) > ZLIB_MAX_WINDOW_EXPONENT ||
        (((unsigned)inputData[0] << (unsigned)BITS_PER_BYTE) + inputData[1]) %
            ZLIB_HEADER_DIVISOR ||
        ((unsigned)inputData[1] & (unsigned)ZLIB_DICTIONARY_FLAG)) {
        return 0;
    }
    bitReader.p = inputData + 2;
    bitReader.n = inputLength - 2 - ZLIB_TRAILER_LENGTH;

    /* Decode each block using dedicated stored, fixed, and dynamic helpers.*/
    while (!finalBlock && !bitReader.bad) {
        unsigned blockType;
        Huff literals;
        Huff distances;
        finalBlock = (int)readBits(&bitReader, 1);
        blockType = readBits(&bitReader, 2);
        if (bitReader.bad) {
            return 0;
        }
        if (blockType == 0) {
            if (!readStoredBlock(&bitReader, out)) {
                return 0;
            }
            continue;
        }
        if (blockType == 1) {
            if (!buildFixedTables(&literals, &distances)) {
                return 0;
            }
        } else if (blockType == 2) {
            if (!readDynamicTables(&bitReader, &literals, &distances)) {
                return 0;
            }
        } else {
            return 0;
        }
        if (!decodeHuffmanBlock(&bitReader, out, (HuffTables){&literals, &distances})) {
            return 0;
        }
    }

    /* VERIFY THE COMPLETE STREAM*/
    /* Check input consumption and the Adler checksum before reporting success.*/
    return !bitReader.bad && bitReader.at == bitReader.n &&
           adler(out->data, out->size) == be32(inputData + inputLength - 4);
}

/* ENCODING*/
/**
 * Append a value to the output bit stream, least-significant bit first.
 * @param writer Bit writer that receives the encoded value.
 * @param pattern Value and number of low-order bits to emit.
 * @author kai
 */
static void putBits(Writer *writer, BitPattern pattern) {
    writer->bits |= (unsigned long)pattern.value << writer->count;
    writer->count += pattern.bitCount;
    while (writer->count >= BITS_PER_BYTE) {
        unsigned char outputByte = (unsigned char)writer->bits;
        if (!rcAppend(writer->out, &outputByte, 1)) {
            writer->bad = 1;
        }
        writer->bits >>= (unsigned)BITS_PER_BYTE;
        writer->count -= BITS_PER_BYTE;
    }
}
/**
 * Reverse the low n bits for DEFLATE canonical codes.
 * @param pattern Code and number of bits to reverse.
 * @return The low-order n bits of v in reversed order.
 * @author kai
 */
static unsigned reverseBits(BitPattern pattern) {
    unsigned value = pattern.value;
    unsigned bitCount = pattern.bitCount;
    unsigned reversed = 0;
    while (bitCount--) {
        reversed = (reversed << 1U) | (value & 1U);
        value >>= 1U;
    }
    return reversed;
}
/**
 * Write one symbol using the fixed DEFLATE literal/length table.
 * @param writer Destination DEFLATE bit writer.
 * @param symbol Literal or length symbol from the fixed alphabet.
 * @author kai
 */
static void fixedSymbol(Writer *writer, unsigned symbol) {
    unsigned code;
    unsigned codeLength;
    if (symbol < DEFLATE_FIXED_LITERAL_FIRST_BOUNDARY) {
        code = DEFLATE_FIXED_LITERAL_FIRST_CODE + symbol;
        codeLength = BITS_PER_BYTE;
    } else if (symbol < DEFLATE_FIXED_LITERAL_SECOND_BOUNDARY) {
        code = DEFLATE_FIXED_LITERAL_SECOND_CODE + symbol - DEFLATE_FIXED_LITERAL_FIRST_BOUNDARY;
        codeLength = DEFLATE_FIXED_LONG_CODE_BITS;
    } else if (symbol < DEFLATE_FIXED_LENGTH_BOUNDARY) {
        code = symbol - DEFLATE_FIXED_LITERAL_SECOND_BOUNDARY;
        codeLength = DEFLATE_FIXED_SHORT_CODE_BITS;
    } else {
        code = DEFLATE_FIXED_LENGTH_CODE + symbol - DEFLATE_FIXED_LENGTH_BOUNDARY;
        codeLength = BITS_PER_BYTE;
    }
    putBits(writer, (BitPattern){reverseBits((BitPattern){code, codeLength}), codeLength});
}

typedef struct {
    unsigned length;
    unsigned distance;
} DeflateMatch;
typedef struct {
    size_t *head;
    size_t *previous;
} DeflateHistory;
typedef struct {
    size_t offset;
    unsigned count;
} DeflateRange;

/**
 * Find the longest prior match in the bounded LZ77 history chain.
 * @param inputData Source bytes.
 * @param inputLength Source length.
 * @param inputOffset Current source offset.
 * @param history Hash-chain heads and links.
 * @return Best match length and distance, or zero length when no match exists.
 * @author kai
 */
static DeflateMatch findDeflateMatch(const unsigned char *inputData, size_t inputLength,
                                     size_t inputOffset, const DeflateHistory *history) {
    DeflateMatch best = {0, 0};
    unsigned hash;
    unsigned attempts = 0;
    size_t candidate = (size_t)-1;
    if (inputOffset + 2 < inputLength) {
        hash = (((unsigned)inputData[inputOffset] * DEFLATE_HASH_MULTIPLIER_FIRST) +
                ((unsigned)inputData[inputOffset + 1] * DEFLATE_HASH_MULTIPLIER_SECOND) +
                inputData[inputOffset + 2]) &
               (unsigned)DEFLATE_HASH_MASK;
        candidate = history->head[hash];
    }
    while (candidate != (size_t)-1 && candidate < inputOffset &&
           inputOffset - candidate <= DEFLATE_WINDOW_SIZE &&
           attempts++ < DEFLATE_MAX_CHAIN_ATTEMPTS) {
        unsigned matchLength = 0;
        while (matchLength < DEFLATE_MAX_MATCH_LENGTH && inputOffset + matchLength < inputLength &&
               inputData[candidate + matchLength] == inputData[inputOffset + matchLength]) {
            matchLength++;
        }
        if (matchLength >= DEFLATE_MIN_MATCH_LENGTH && matchLength > best.length) {
            best.length = matchLength;
            best.distance = (unsigned)(inputOffset - candidate);
            if (matchLength == DEFLATE_MAX_MATCH_LENGTH) {
                break;
            }
        }
        {
            size_t next = history->previous[candidate & (size_t)DEFLATE_HASH_MASK];
            if (next >= candidate) {
                break;
            }
            candidate = next;
        }
    }
    return best;
}

/**
 * Add consumed source bytes to the LZ77 history hash chains.
 * @param inputData Source bytes.
 * @param inputLength Source length.
 * @param range Start offset and number of consumed bytes.
 * @param history Hash-chain heads and links.
 * @author kai
 */
static void indexDeflatePositions(const unsigned char *inputData, size_t inputLength,
                                  DeflateRange range, DeflateHistory *history) {
    unsigned matchOffset;
    for (matchOffset = 0; matchOffset < range.count; matchOffset++) {
        size_t position = range.offset + matchOffset;
        if (position + 2 < inputLength) {
            unsigned hash = (((unsigned)inputData[position] * DEFLATE_HASH_MULTIPLIER_FIRST) +
                             ((unsigned)inputData[position + 1] * DEFLATE_HASH_MULTIPLIER_SECOND) +
                             inputData[position + 2]) &
                            (unsigned)DEFLATE_HASH_MASK;
            history->previous[position & (size_t)DEFLATE_HASH_MASK] = history->head[hash];
            history->head[hash] = position;
        }
    }
}
/**
 * Encode bytes as a zlib stream using stored blocks when compression is not useful.
 * @param inputData Uncompressed bytes to encode.
 * @param inputLength Input length in bytes.
 * @param out Receives the complete zlib stream.
 * @return Nonzero when a complete stream is produced; zero on allocation or size failure.
 * @author kai
 */
int rcDeflate(const unsigned char *inputData, size_t inputLength, RcBuffer *out) {
    size_t *head = malloc(((size_t)DEFLATE_HASH_TABLE_SIZE * 2U) * sizeof(*head));
    DeflateHistory history;
    size_t inputOffset = 0;
    size_t byteIndex;
    Writer writer = {out, 0, 0, 0};
    unsigned char header[2] = {ZLIB_HEADER_FIRST_BYTE, 0x01};
    unsigned char tail[4];
    unsigned long sum;

    /* PREPARE THE HISTORY WINDOW*/
    /* Hash heads and history links share one allocation and one lifetime.*/
    if (!head) {
        return 0;
    }
    history.head = head;
    history.previous = head + DEFLATE_HASH_TABLE_SIZE;
    for (byteIndex = 0; byteIndex < DEFLATE_HASH_TABLE_SIZE; byteIndex++) {
        history.head[byteIndex] = history.previous[byteIndex] = (size_t)-1;
    }
    if (!rcAppend(out, header, 2)) {
        free(head);
        return 0;
    }
    putBits(&writer, (BitPattern){3, 3});

    /* Bound hash-chain searches so repetitive inputs cannot cause unbounded work.*/
    while (inputOffset < inputLength && !writer.bad) {
        DeflateMatch match = findDeflateMatch(inputData, inputLength, inputOffset, &history);
        unsigned step = 1;

        /* EMIT THE SELECTED REPRESENTATION*/
        /* A match emits length/distance codes; otherwise emit one literal byte.*/
        if (match.length >= DEFLATE_MIN_MATCH_LENGTH) {
            unsigned codeIndex = 0;
            while (codeIndex < DEFLATE_LAST_LENGTH_BASE_INDEX &&
                   match.length >= lbase[codeIndex + 1]) {
                codeIndex++;
            }
            fixedSymbol(&writer, codeIndex + DEFLATE_FIRST_LENGTH_SYMBOL);
            putBits(&writer, (BitPattern){match.length - lbase[codeIndex], lextra[codeIndex]});
            codeIndex = 0;
            while (codeIndex < DEFLATE_LAST_DISTANCE_BASE_INDEX &&
                   match.distance >= dbase[codeIndex + 1]) {
                codeIndex++;
            }
            putBits(&writer,
                    (BitPattern){
                        reverseBits((BitPattern){codeIndex, DEFLATE_FIXED_DISTANCE_CODE_BITS}),
                        DEFLATE_FIXED_DISTANCE_CODE_BITS,
                    });
            putBits(&writer, (BitPattern){match.distance - dbase[codeIndex], dextra[codeIndex]});
            step = match.length;
        } else {
            fixedSymbol(&writer, inputData[inputOffset]);
        }

        /* INDEX THE CONSUMED POSITIONS*/
        indexDeflatePositions(inputData, inputLength, (DeflateRange){inputOffset, step}, &history);
        inputOffset += step;
    }

    /* CLOSE THE ZLIB STREAM*/
    /* Finish the block, pad its last byte, and append the checksum of the original bytes.*/
    fixedSymbol(&writer, DEFLATE_END_OF_BLOCK_SYMBOL);
    if (writer.count) {
        putBits(&writer, (BitPattern){0, BITS_PER_BYTE - writer.count});
    }
    sum = adler(inputData, inputLength);
    for (byteIndex = 0; byteIndex < 4; byteIndex++) {
        tail[byteIndex] =
            (unsigned char)(sum >> (ZLIB_CHECKSUM_FIRST_SHIFT - (byteIndex * BITS_PER_BYTE)));
    }
    free(head);
    return !writer.bad && rcAppend(out, tail, 4);
}

/*//////////////////////////////////////////////////
PNG
//////////////////////////////////////////////////*/

/* FILTERS*/
/**
 * Select the PNG Paeth predictor nearest to the three neighbouring bytes.
 * @param left Left neighbouring sample.
 * @param above Sample above the current position.
 * @param upperLeft Upper-left neighbouring sample.
 * @return The neighbouring sample nearest the Paeth estimate.
 * @author kai
 */
static int paeth(int left, int above, int upperLeft) {
    int estimate = left + above - upperLeft;
    int leftDistance = abs(estimate - left);
    int aboveDistance = abs(estimate - above);
    int upperLeftDistance = abs(estimate - upperLeft);
    if (leftDistance <= aboveDistance && leftDistance <= upperLeftDistance) {
        return left;
    }
    if (aboveDistance <= upperLeftDistance) {
        return above;
    }
    return upperLeft;
}
/**
 * Read one packed PNG sample at the requested bit depth.
 * @param row Packed row to read or update.
 * @param sample Zero-based sample index in the packed row.
 * @param depth PNG sample bit depth: 1, 2, 4, 8, or 16.
 * @return The unpacked sample value at the requested index.
 * @author kai
 */
unsigned rcSample(const unsigned char *row, size_t sample, unsigned depth) {
    if (depth == PNG_SAMPLE_DEPTH_16) {
        return ((unsigned)row[sample * 2] << (unsigned)BITS_PER_BYTE) | row[(sample * 2) + 1];
    }
    if (depth == BITS_PER_BYTE) {
        return row[sample];
    }
    return ((unsigned)row[sample * depth / BITS_PER_BYTE] >>
            (unsigned)(BITS_PER_BYTE - depth - (sample * depth % BITS_PER_BYTE))) &
           ((1U << depth) - 1);
}

/**
 * Reverse one PNG scanline filter using the prior decoded row when present.
 * @param row Packed row to read or update.
 * @param prior Previous decoded row, or NULL.
 * @param options Row length, bytes per pixel, and filter selector.
 * @return 0 for an unknown filter; row and prior must each cover length bytes.
 * @author kai
 */
int rcUnfilter(unsigned char *row, const unsigned char *prior, RcUnfilterOptions options) {
    size_t byteIndex;
    if (options.filter > 4) {
        return 0;
    }
    /* RECONSTRUCT LEFT TO RIGHT*/
    /* Left neighbours already contain decoded bytes, including for packed colour samples.*/
    for (byteIndex = 0; byteIndex < options.length; byteIndex++) {
        int left = byteIndex >= options.bytesPerPixel ? row[byteIndex - options.bytesPerPixel] : 0;
        int above = prior ? prior[byteIndex] : 0;
        int upperLeft = prior && byteIndex >= options.bytesPerPixel
                            ? prior[byteIndex - options.bytesPerPixel]
                            : 0;
        int prediction;
        if (options.filter == 0) {
            prediction = 0;
        } else if (options.filter == 1) {
            prediction = left;
        } else if (options.filter == 2) {
            prediction = above;
        } else if (options.filter == 3) {
            prediction = (left + above) / 2;
        } else {
            prediction = paeth(left, above, upperLeft);
        }
        row[byteIndex] = (unsigned char)(row[byteIndex] + prediction);
    }
    return 1;
}

/**
 * Validate and store PNG image geometry from its IHDR payload.
 * @param info Receives image geometry and channel count.
 * @param body IHDR payload bytes.
 * @param length IHDR payload length.
 * @return Nonzero when the image header is supported.
 * @author kai
 */
static int readPngHeader(PngInfo *info, const unsigned char *body, unsigned long length) {
    static const unsigned char channelCounts[PNG_COLOR_CHANNEL_TABLE_SIZE] = {1, 0, 3, 1, 2, 0, 4};
    if (length != PNG_HEADER_LENGTH) {
        return 0;
    }
    info->width = (unsigned)be32(body);
    info->height = (unsigned)be32(body + 4);
    info->depth = body[BITS_PER_BYTE];
    info->type = body[PNG_HEADER_COLOR_TYPE_OFFSET];
    info->interlace = body[PNG_HEADER_INTERLACE_OFFSET];
    if (!info->width || !info->height || (size_t)info->width > RC_PIXELS / info->height ||
        body[PNG_IHDR_RESERVED_FIRST_OFFSET] || body[PNG_IHDR_RESERVED_SECOND_OFFSET] ||
        info->interlace > 1 || info->type > PNG_MAX_COLOR_TYPE) {
        return 0;
    }
    info->channels = channelCounts[info->type];
    return info->channels &&
           (info->depth == BITS_PER_BYTE ||
            (info->depth == PNG_SAMPLE_DEPTH_16 && info->type != PNG_COLOR_TYPE_INDEXED) ||
            ((info->type == PNG_COLOR_TYPE_GRAY || info->type == PNG_COLOR_TYPE_INDEXED) &&
             (info->depth == PNG_SAMPLE_DEPTH_1 || info->depth == PNG_SAMPLE_DEPTH_2 ||
              info->depth == PNG_SAMPLE_DEPTH_4)));
}

/**
 * Load an RGB palette from a validated PNG PLTE payload.
 * @param info Palette state.
 * @param body PLTE payload bytes.
 * @param length PLTE payload length.
 * @return Nonzero when the palette payload is supported.
 * @author kai
 */
static int readPngPalette(PngInfo *info, const unsigned char *body, unsigned long length) {
    unsigned entryIndex;
    if (!length || length % PNG_RGB_CHANNEL_COUNT || length > PNG_MAX_PALETTE_BYTES) {
        return 0;
    }
    info->paletteCount = (unsigned)length / PNG_RGB_CHANNEL_COUNT;
    for (entryIndex = 0; entryIndex < info->paletteCount; entryIndex++) {
        copyBytes(body + ((size_t)entryIndex * PNG_RGB_CHANNEL_COUNT), PNG_RGB_CHANNEL_COUNT,
                  info->palette[entryIndex]);
    }
    return 1;
}

/**
 * Apply a PNG tRNS payload to palette alpha or transparency keys.
 * @param info Image colour and palette state.
 * @param body tRNS payload bytes.
 * @param length tRNS payload length.
 * @return Nonzero when the transparency payload matches the colour type.
 * @author kai
 */
static int readPngTransparency(PngInfo *info, const unsigned char *body, unsigned long length) {
    unsigned entryIndex;
    if (info->type == PNG_COLOR_TYPE_INDEXED) {
        if (!info->paletteCount || length > info->paletteCount) {
            return 0;
        }
        for (entryIndex = 0; entryIndex < length; entryIndex++) {
            info->palette[entryIndex][PNG_ALPHA_CHANNEL_INDEX] = body[entryIndex];
        }
        return 1;
    }
    if (info->type == PNG_COLOR_TYPE_GRAY && length == PNG_TRANSPARENCY_GRAY_LENGTH) {
        info->transparency[0] = ((unsigned)body[0] << (unsigned)BITS_PER_BYTE) | body[1];
        return 1;
    }
    if (info->type == PNG_COLOR_TYPE_RGB && length == PNG_TRANS_RGB_LENGTH) {
        for (entryIndex = 0; entryIndex < PNG_RGB_CHANNEL_COUNT; entryIndex++) {
            info->transparency[entryIndex] =
                ((unsigned)body[(size_t)entryIndex * 2] << (unsigned)BITS_PER_BYTE) |
                body[((size_t)entryIndex * 2) + 1];
        }
        return 1;
    }
    return 0;
}

/**
 * Accept an ancillary PNG chunk and close a preceding contiguous IDAT sequence.
 * @param state Current PNG chunk-order state.
 * @param name Four-byte chunk type.
 * @return Nonzero for an ancillary chunk.
 * @author kai
 */
static int handlePngAncillary(PngChunkState *state, const unsigned char *name) {
    if (state->haveData) {
        state->closedData = 1;
    }
    return !!((unsigned)name[0] & (unsigned)PNG_ANCILLARY_CHUNK_FLAG);
}

/**
 * Process one CRC-validated PNG chunk and update parser state.
 * @param info Image metadata and palette.
 * @param state Chunk-order state.
 * @param packed Receives contiguous IDAT data.
 * @param chunk PNG chunk type, payload, and length.
 * @return 1 to continue, 2 for IEND, or 0 for unsupported or invalid order.
 * @author kai
 */
static int handlePngChunk(PngInfo *info, PngChunkState *state, RcBuffer *packed, PngChunk chunk) {
    const unsigned char *name = chunk.name;
    const unsigned char *body = chunk.body;
    unsigned long length = chunk.length;
    if (memcmp(name, "IHDR", 4) == 0) {
        if (state->header || !readPngHeader(info, body, length)) {
            return 0;
        }
        state->header = 1;
    } else if (memcmp(name, "PLTE", 4) == 0) {
        return state->haveData ? 0 : readPngPalette(info, body, length);
    } else if (memcmp(name, "tRNS", 4) == 0) {
        return state->haveData ? 0 : readPngTransparency(info, body, length);
    } else if (memcmp(name, "IDAT", 4) == 0) {
        if (state->closedData || !rcAppend(packed, body, length)) {
            return 0;
        }
        state->haveData = 1;
    } else if (memcmp(name, "acTL", 4) == 0) {
        return 0;
    } else if (memcmp(name, "IEND", 4) == 0) {
        if (length || !state->haveData) {
            return 0;
        }
        state->ended = 1;
        return 2;
    } else {
        if (!handlePngAncillary(state, name)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Validate PNG chunk CRCs and collect image metadata and IDAT payload.
 * @param pngData Complete PNG bytes.
 * @param pngLength Number of available bytes.
 * @param info Receives image metadata.
 * @param packed Receives compressed image data.
 * @return Nonzero when the full chunk sequence ends with a valid IEND.
 * @author kai
 */
static int parsePngChunks(const unsigned char *pngData, size_t pngLength, PngInfo *info,
                          RcBuffer *packed) {
    PngChunkState state = {0};
    size_t inputOffset = PNG_SIGNATURE_LENGTH;
    while (inputOffset + PNG_CHUNK_OVERHEAD <= pngLength) {
        unsigned long length = be32(pngData + inputOffset);
        const unsigned char *name = pngData + inputOffset + 4;
        const unsigned char *body = pngData + inputOffset + BITS_PER_BYTE;
        int result;
        if (length > pngLength - inputOffset - PNG_CHUNK_OVERHEAD ||
            rcCrc(name, (size_t)length + 4) != be32(body + length) ||
            (!state.header && memcmp(name, "IHDR", 4) != 0)) {
            return 0;
        }
        result = handlePngChunk(info, &state, packed, (PngChunk){name, body, length});
        if (!result) {
            return 0;
        }
        if (result == 2) {
            inputOffset += PNG_CHUNK_OVERHEAD;
            break;
        }
        inputOffset += (size_t)length + PNG_CHUNK_OVERHEAD;
    }
    return state.header && state.ended && inputOffset == pngLength &&
           (info->type != PNG_COLOR_TYPE_INDEXED || info->paletteCount != 0);
}

typedef struct {
    unsigned originX;
    unsigned originY;
    unsigned strideX;
    unsigned strideY;
    unsigned width;
    unsigned height;
} PngPass;

/**
 * Convert one PNG sample pixel into the final RGBA raster.
 * @param info PNG colour, palette, and transparency metadata.
 * @param pass Adam7 pass geometry.
 * @param row Unfiltered packed sample row.
 * @param columnIndex Pixel column in the current pass.
 * @param image Receives the decoded pixel.
 * @param rowIndex Row index in the current pass.
 * @return Nonzero when the sample values are valid.
 * @author kai
 */
static int decodePngPixel(const PngInfo *info, PngPass pass, const unsigned char *row,
                          unsigned columnIndex, RcImage *image, unsigned rowIndex) {
    unsigned samples[PNG_RGBA_CHANNEL_COUNT] = {0, 0, 0, PNG_OPAQUE_ALPHA};
    unsigned channelIndex;
    unsigned maxSample = (1U << info->depth) - 1;
    unsigned pixelX = pass.originX + (columnIndex * pass.strideX);
    unsigned pixelY = pass.originY + (rowIndex * pass.strideY);
    unsigned char *pixel =
        image->rgba + ((((size_t)pixelY * image->width) + pixelX) * PNG_RGBA_CHANNEL_COUNT);
    for (channelIndex = 0; channelIndex < info->channels; channelIndex++) {
        samples[channelIndex] =
            rcSample(row, ((size_t)columnIndex * info->channels) + channelIndex, info->depth);
    }
    if (info->type == PNG_COLOR_TYPE_INDEXED) {
        if (samples[0] >= info->paletteCount) {
            return 0;
        }
        copyBytes(info->palette[samples[0]], PNG_RGBA_CHANNEL_COUNT, pixel);
        return 1;
    }
    pixel[PNG_ALPHA_CHANNEL_INDEX] = PNG_OPAQUE_ALPHA;
    if (info->type == PNG_COLOR_TYPE_GRAY || info->type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        pixel[0] = pixel[1] = pixel[2] =
            (unsigned char)(samples[0] * PNG_MAX_SAMPLE_8_BIT / maxSample);
        if (info->type == PNG_COLOR_TYPE_GRAY_ALPHA) {
            pixel[PNG_ALPHA_CHANNEL_INDEX] =
                (unsigned char)(samples[1] * PNG_MAX_SAMPLE_8_BIT / maxSample);
        } else if (samples[0] == info->transparency[0]) {
            pixel[PNG_ALPHA_CHANNEL_INDEX] = 0;
        }
    } else {
        for (channelIndex = 0; channelIndex < PNG_RGB_CHANNEL_COUNT; channelIndex++) {
            pixel[channelIndex] =
                (unsigned char)(samples[channelIndex] * PNG_MAX_SAMPLE_8_BIT / maxSample);
        }
        if (info->type == PNG_COLOR_TYPE_RGBA) {
            pixel[PNG_ALPHA_CHANNEL_INDEX] = (unsigned char)(samples[PNG_ALPHA_CHANNEL_INDEX] *
                                                             PNG_MAX_SAMPLE_8_BIT / maxSample);
        } else if (samples[0] == info->transparency[0] && samples[1] == info->transparency[1] &&
                   samples[2] == info->transparency[2]) {
            pixel[PNG_ALPHA_CHANNEL_INDEX] = 0;
        }
    }
    return 1;
}

/**
 * Decode one filtered PNG scanline into the final raster.
 * @param info PNG colour metadata.
 * @param pass Adam7 pass geometry.
 * @param row Unfiltered packed sample row.
 * @param rowIndex Row index within the pass.
 * @param image Receives RGBA pixels.
 * @return Nonzero when all pixels in the row are valid.
 * @author kai
 */
static int decodePngRow(const PngInfo *info, PngPass pass, const unsigned char *row,
                        unsigned rowIndex, RcImage *image) {
    unsigned columnIndex;
    for (columnIndex = 0; columnIndex < pass.width; columnIndex++) {
        if (!decodePngPixel(info, pass, row, columnIndex, image, rowIndex)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Decode and unfilter all rows in one PNG Adam7 pass.
 * @param info PNG colour and image metadata.
 * @param pass Pass origin, strides, and dimensions.
 * @param raw Inflated image rows.
 * @param cursor Current offset within the inflated rows.
 * @param image Receives final RGBA pixels.
 * @return Nonzero when all pass rows are decoded.
 * @author kai
 */
static int decodePngPass(const PngInfo *info, PngPass pass, RcBuffer *raw, size_t *cursor,
                         RcImage *image) {
    size_t rowBytes =
        (((size_t)pass.width * info->channels * info->depth) + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE;
    size_t bytesPerPixel = ((info->channels * info->depth) + (BITS_PER_BYTE - 1)) / BITS_PER_BYTE;
    size_t rowInputBytes = rowBytes + 1;
    unsigned rowIndex;
    unsigned char *prior = NULL;
    if (!pass.width || !pass.height) {
        return 1;
    }
    if (!rowInputBytes || *cursor > raw->size || rowInputBytes > raw->size - *cursor ||
        pass.height > (raw->size - *cursor) / rowInputBytes) {
        return 0;
    }
    for (rowIndex = 0; rowIndex < pass.height; rowIndex++) {
        unsigned filter = raw->data[(*cursor)++];
        unsigned char *row = raw->data + *cursor;
        *cursor += rowBytes;
        if (!rcUnfilter(row, prior, (RcUnfilterOptions){rowBytes, bytesPerPixel, filter}) ||
            !decodePngRow(info, pass, row, rowIndex, image)) {
            return 0;
        }
        prior = row;
    }
    return 1;
}

/**
 * Inflate PNG image bytes and decode all non-interlaced or Adam7 passes.
 * @param info Validated PNG metadata.
 * @param packed Compressed IDAT payload.
 * @param raw Receives inflated row bytes.
 * @param image Receives final RGBA raster.
 * @return Nonzero when all inflated bytes map to valid pixels.
 * @author kai
 */
static int decodePngRaster(const PngInfo *info, RcBuffer *packed, RcBuffer *raw, RcImage *image) {
    static const unsigned passStartX[PNG_ADAM7_PASS_COUNT] = {0, 4, 0, 2, 0, 1, 0};
    static const unsigned passStartY[PNG_ADAM7_PASS_COUNT] = {0, 0, 4, 0, 2, 0, 1};
    static const unsigned passStepX[PNG_ADAM7_PASS_COUNT] = {
        ADAM7_INITIAL_STEP, ADAM7_INITIAL_STEP, 4, 4, 2, 2, 1,
    };
    static const unsigned passStepY[PNG_ADAM7_PASS_COUNT] = {
        ADAM7_INITIAL_STEP, ADAM7_INITIAL_STEP, ADAM7_INITIAL_STEP, 4, 4, 2, 2,
    };
    size_t cursor = 0;
    size_t pixelCount = (size_t)info->width * info->height;
    unsigned passIndex;
    unsigned passes = info->interlace ? PNG_ADAM7_PASS_COUNT : 1;
    if (!pixelCount || !rcInflate(packed->data, packed->size, raw)) {
        return 0;
    }
    image->rgba = calloc(pixelCount, sizeof(*image->rgba));
    if (!image->rgba) {
        return 0;
    }
    image->width = info->width;
    image->height = info->height;
    for (passIndex = 0; passIndex < passes; passIndex++) {
        PngPass pass = {0, 0, 1, 1, info->width, info->height};
        if (info->interlace) {
            pass.originX = passStartX[passIndex];
            pass.originY = passStartY[passIndex];
            pass.strideX = passStepX[passIndex];
            pass.strideY = passStepY[passIndex];
            pass.width = info->width > pass.originX
                             ? (info->width - pass.originX + pass.strideX - 1) / pass.strideX
                             : 0;
            pass.height = info->height > pass.originY
                              ? (info->height - pass.originY + pass.strideY - 1) / pass.strideY
                              : 0;
        }
        if (!decodePngPass(info, pass, raw, &cursor, image)) {
            return 0;
        }
    }
    return cursor == raw->size;
}

/* IMAGE DECODING*/
/**
 * Decode supported PNG colour types, transparency, and Adam7 passes into RGBA8.
 * @param pngData Complete PNG file bytes.
 * @param pngLength PNG length in bytes.
 * @param image Receives decoded RGBA8 pixels on success.
 * @return Nonzero for a fully validated image; zero for malformed or unsupported data.
 * @author kai
 */
static int pngDecode(const unsigned char *pngData, size_t pngLength, RcImage *image) {
    PngInfo info = {0};
    RcBuffer packed = {0};
    RcBuffer raw = {0};
    int success = 0;
    info.transparency[0] = PNG_SAMPLE_RANGE_16;
    info.transparency[1] = PNG_SAMPLE_RANGE_16;
    info.transparency[2] = PNG_SAMPLE_RANGE_16;
    fillBytes(info.palette, (FillBytesOptions){PNG_OPAQUE_ALPHA, sizeof(info.palette)});
    if (parsePngChunks(pngData, pngLength, &info, &packed) &&
        decodePngRaster(&info, &packed, &raw, image)) {
        success = 1;
    }
    free(packed.data);
    free(raw.data);
    if (!success) {
        free(image->rgba);
        *image = (RcImage){0};
    }
    return success;
}

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* FORMAT DISPATCH*/
/**
 * Select a decoder from the file signature and return owned RGBA8 pixels.
 * @param pngData PNG or JPEG file bytes, identified by signature.
 * @param pngLength Encoded file length in bytes.
 * @param image Receives owned RGBA pixels on success.
 * @return 0 for malformed, unsupported, or over-limit images.
 * @author kai
 */
int rcDecode(const unsigned char *pngData, size_t pngLength, RcImage *image) {
    *image = (RcImage){0};
    if (pngLength >= PNG_SIGNATURE_LENGTH &&
        !memcmp(pngData, "\211PNG\r\n\032\n", PNG_SIGNATURE_LENGTH)) {
        return pngDecode(pngData, pngLength, image);
    }
    if (pngLength >= 2 && pngData[0] == PNG_MAX_SAMPLE_8_BIT &&
        pngData[1] == JPEG_START_OF_IMAGE_END) {
        return rcJpeg(pngData, pngLength, image, 1);
    }
    return 0;
}
