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
    DEFLATE_CODE_LENGTH_SYMBOL_9 = 9,
    DEFLATE_CODE_LENGTH_SYMBOL_10 = 10,
    DEFLATE_CODE_LENGTH_SYMBOL_11 = 11,
    RC_INITIAL_BUFFER_CAPACITY = 256,
    PNG_PALETTE_ENTRY_COUNT = 256,
    PNG_RGBA_CHANNEL_COUNT = 4,
    PNG_MAX_SAMPLE_8_BIT = 255,
    PNG_OPAQUE_ALPHA = 255,
    PNG_SAMPLE_DEPTH_16 = 16,
    PNG_SAMPLE_RANGE_16 = 65536,
    PNG_MAX_PALETTE_BYTES = 768,
    PNG_CHUNK_OVERHEAD = 12,
    PNG_HEADER_LENGTH = 13,
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
    RcBuffer *out;
    unsigned long bits;
    unsigned count;
    int bad;
} Writer;

/* CANONICAL TABLES*/
static const unsigned lbase[29] = {
    3,  4,  5,  6,  7,  BITS_PER_BYTE, 9,   10,  11,  13,  15, 17, 19, 23, 27, 31, 35, 43,
    51, 59, 67, 83, 99, 115,           131, 163, 195, 227, 258};
static const unsigned char lextra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                         2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned dbase[30] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                   33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                   1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const unsigned char dextra[30] = {
    0, 0, 0,  0,  1,  1,  2,  2,  3,  3, 4, 4, 5, 5, 6, 6, 7, 7, BITS_PER_BYTE, BITS_PER_BYTE,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/*//////////////////////////////////////////////////
BUFFERS
//////////////////////////////////////////////////*/

/* STORAGE*/
/**
 * Copy a bounded byte range without relying on libc's unchecked buffer helpers.
 * @param destination Writable memory with room for length bytes.
 * @param source Readable memory containing length bytes.
 * @param length Number of bytes to copy.
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

/**
 * Fill a bounded byte range with one byte value.
 * @param destination Writable memory with room for length bytes.
 * @param value Byte value to repeat.
 * @param length Number of bytes to fill.
 * @author kai
 */
static void fillBytes(unsigned char value, size_t length, void *destination) {
    unsigned char *destinationBytes = destination;
    size_t index;
    for (index = 0; index < length; index++) {
        destinationBytes[index] = value;
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
            checksum = (checksum >> 1) ^ ((checksum & 1) ? PNG_CRC_POLYNOMIAL : 0);
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
    return (sumB << ADLER_COMPONENT_BITS) | sumA;
}
/**
 * Read one big-endian 32-bit integer.
 * @param data Four bytes in network byte order.
 * @return The unsigned 32-bit value represented by the four bytes.
 * @author kai
 */
static unsigned long be32(const unsigned char *data) {
    return ((unsigned long)data[0] << (BITS_PER_BYTE * 3)) |
           ((unsigned long)data[1] << (BITS_PER_BYTE * 2)) |
           ((unsigned long)data[2] << BITS_PER_BYTE) | data[3];
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
        if (lengths[symbolIndex] > 15) {
            return 0;
        }
        table->count[lengths[symbolIndex]]++;
    }
    for (codeLength = 1; codeLength <= DEFLATE_MAX_CODE_BITS; codeLength++) {
        left = left * 2 - (int)table->count[codeLength];
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
        first = (first + table->count[codeBits]) << 1;
        code <<= 1;
    }
    reader->bad = 1;
    return -1;
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
    if (inputLength < 6 || (inputData[0] & 15) != BITS_PER_BYTE || (inputData[0] >> 4) > 7 ||
        (((unsigned)inputData[0] << BITS_PER_BYTE) + inputData[1]) % 31 || (inputData[1] & 32)) {
        return 0;
    }
    bitReader.p = inputData + 2;
    bitReader.n = inputLength - 6;

    /* Decode block alphabets before copying literals or bounded history matches.*/
    while (!finalBlock && !bitReader.bad) {
        unsigned blockType;
        Huff lit;
        Huff dist;
        unsigned char lens[DEFLATE_CODE_LENGTHS_CAPACITY] = {0};
        unsigned char distanceLengths[DEFLATE_MAX_DISTANCE_SYMBOLS] = {0};
        unsigned index;

        /* READ THE BLOCK HEADER*/
        finalBlock = (int)readBits(&bitReader, 1);
        blockType = readBits(&bitReader, 2);
        if (blockType == 0) {
            /* COPY STORED BYTES*/
            /* Discard bit padding and verify the stored length against its complement.*/
            unsigned len;
            unsigned inv;
            bitReader.bits = 0;
            bitReader.count = 0;
            len = readBits(&bitReader, DEFLATE_STORED_LENGTH_BITS);
            inv = readBits(&bitReader, DEFLATE_STORED_LENGTH_BITS);
            if (bitReader.bad || (len ^ inv) != DEFLATE_STORED_LENGTH_MAX ||
                len > bitReader.n - bitReader.at ||
                !rcAppend(out, bitReader.p + bitReader.at, len)) {
                return 0;
            }
            bitReader.at += len;
            continue;
        }
        if (blockType == 1) {
            /* BUILD FIXED ALPHABETS*/
            for (index = 0; index < DEFLATE_FIXED_LITERAL_SYMBOLS; index++) {
                lens[index] = (unsigned char)(index < 144   ? BITS_PER_BYTE
                                              : index < 256 ? 9
                                              : index < 280 ? 7
                                                            : BITS_PER_BYTE);
            }
            for (index = 0; index < DEFLATE_FIXED_DISTANCE_SYMBOLS; index++) {
                distanceLengths[index] = 5;
            }
            if (!makeHuff(&lit, lens, DEFLATE_FIXED_LITERAL_SYMBOLS) ||
                !makeHuff(&dist, distanceLengths, DEFLATE_FIXED_DISTANCE_SYMBOLS)) {
                return 0;
            }
        } else if (blockType == 2) {
            /* READ DYNAMIC ALPHABETS*/
            /* The code-length alphabet describes both the literal/length and distance trees.*/
            static const unsigned proper[DEFLATE_CODE_LENGTH_SYMBOLS] = {
                DEFLATE_REPEAT_PREVIOUS_SYMBOL,
                DEFLATE_REPEAT_SHORT_ZERO_SYMBOL,
                DEFLATE_REPEAT_LONG_ZERO_SYMBOL,
                0,
                BITS_PER_BYTE,
                7,
                9,
                6,
                10,
                5,
                11,
                4,
                12,
                3,
                13,
                2,
                14,
                1,
                DEFLATE_MAX_CODE_BITS};
            unsigned literalLengthCount = readBits(&bitReader, 5) + 257;
            unsigned distanceCount = readBits(&bitReader, 5) + 1;
            unsigned codeLengthCount = readBits(&bitReader, 4) + 4;
            unsigned codeLengthIndex = 0;
            unsigned char codeLengths[19] = {0};
            Huff codeLengthTable;
            if (literalLengthCount > DEFLATE_MAX_LITERAL_SYMBOLS ||
                distanceCount > DEFLATE_MAX_DISTANCE_SYMBOLS) {
                return 0;
            }
            for (index = 0; index < codeLengthCount; index++) {
                codeLengths[proper[index]] = (unsigned char)readBits(&bitReader, 3);
            }
            if (!makeHuff(&codeLengthTable, codeLengths, DEFLATE_CODE_LENGTH_SYMBOLS)) {
                return 0;
            }

            /* EXPAND REPEATED CODE LENGTHS*/
            /* Repeat symbols must stay within the two declared alphabet lengths.*/
            while (codeLengthIndex < literalLengthCount + distanceCount && !bitReader.bad) {
                int symbol = decodeHuff(&bitReader, &codeLengthTable);
                unsigned run;
                unsigned value;
                if (symbol < 0) {
                    return 0;
                }
                if (symbol < 16) {
                    lens[codeLengthIndex++] = (unsigned char)symbol;
                    continue;
                }
                if (symbol == DEFLATE_REPEAT_PREVIOUS_SYMBOL) {
                    if (!codeLengthIndex) {
                        return 0;
                    }
                    run = readBits(&bitReader, 2) + 3;
                    value = lens[codeLengthIndex - 1];
                } else if (symbol == DEFLATE_REPEAT_SHORT_ZERO_SYMBOL) {
                    run = readBits(&bitReader, 3) + 3;
                    value = 0;
                } else if (symbol == DEFLATE_REPEAT_LONG_ZERO_SYMBOL) {
                    run = readBits(&bitReader, 7) + 11;
                    value = 0;
                } else {
                    return 0;
                }
                if (run > literalLengthCount + distanceCount - codeLengthIndex) {
                    return 0;
                }
                while (run--) {
                    lens[codeLengthIndex++] = (unsigned char)value;
                }
            }
            if (!lens[DEFLATE_END_OF_BLOCK_SYMBOL] || !makeHuff(&lit, lens, literalLengthCount)) {
                return 0;
            }
            copyBytes(lens + literalLengthCount, distanceCount, distanceLengths);
            /* A distance alphabet is unnecessary for literal-only blocks. */
            if (!makeHuff(&dist, distanceLengths, distanceCount)) {
                dist = (Huff){0};
            }
        } else {
            return 0;
        }

        /* DECODE LITERALS AND MATCHES*/
        for (;;) {
            int symbol = decodeHuff(&bitReader, &lit);
            unsigned len;
            unsigned distance;
            unsigned lengthCodeIndex;
            unsigned char byte;
            if (symbol < 0) {
                return 0;
            }
            if (symbol == DEFLATE_END_OF_BLOCK_SYMBOL) {
                break;
            }
            if (symbol < DEFLATE_LITERAL_SYMBOL_LIMIT) {
                byte = (unsigned char)symbol;
                if (!rcAppend(out, &byte, 1)) {
                    return 0;
                }
                continue;
            }
            if (symbol > DEFLATE_MAX_LITERAL_SYMBOLS - 1) {
                return 0;
            }
            lengthCodeIndex = (unsigned)symbol - DEFLATE_FIRST_LENGTH_SYMBOL;
            len = lbase[lengthCodeIndex] + readBits(&bitReader, lextra[lengthCodeIndex]);
            symbol = decodeHuff(&bitReader, &dist);
            if (symbol < 0 ||
                symbol >= DEFLATE_MAX_DISTANCE_SYMBOLS - DEFLATE_RESERVED_DISTANCE_SYMBOLS) {
                return 0;
            }
            distance = dbase[symbol] + readBits(&bitReader, dextra[symbol]);
            if (bitReader.bad || distance > out->size || len > RC_LIMIT - out->size) {
                return 0;
            }

            /* COPY THE HISTORY MATCH*/
            /* Read from the growing output so overlapping LZ77 matches expand correctly.*/
            while (len--) {
                byte = out->data[out->size - distance];
                if (!rcAppend(out, &byte, 1)) {
                    return 0;
                }
            }
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
 * @param value Value whose low bits are emitted.
 * @param bitCount Number of low-order bits to emit.
 * @author kai
 */
static void putBits(Writer *writer, unsigned value, unsigned bitCount) {
    writer->bits |= (unsigned long)value << writer->count;
    writer->count += bitCount;
    while (writer->count >= BITS_PER_BYTE) {
        unsigned char outputByte = (unsigned char)writer->bits;
        if (!rcAppend(writer->out, &outputByte, 1)) {
            writer->bad = 1;
        }
        writer->bits >>= BITS_PER_BYTE;
        writer->count -= BITS_PER_BYTE;
    }
}
/**
 * Reverse the low n bits for DEFLATE canonical codes.
 * @param value Code whose low-order bits are reversed.
 * @param bitCount Number of bits to reverse.
 * @return The low-order n bits of v in reversed order.
 * @author kai
 */
static unsigned reverseBits(unsigned value, unsigned bitCount) {
    unsigned reversed = 0;
    while (bitCount--) {
        reversed = (reversed << 1) | (value & 1);
        value >>= 1;
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
    if (symbol < 144) {
        code = 48 + symbol;
        codeLength = BITS_PER_BYTE;
    } else if (symbol < 256) {
        code = 400 + symbol - 144;
        codeLength = 9;
    } else if (symbol < 280) {
        code = symbol - 256;
        codeLength = 7;
    } else {
        code = 192 + symbol - 280;
        codeLength = BITS_PER_BYTE;
    }
    putBits(writer, reverseBits(code, codeLength), codeLength);
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
    size_t *head = malloc((DEFLATE_HASH_TABLE_SIZE * 2) * sizeof(*head));
    size_t *prev;
    size_t inputOffset = 0;
    size_t byteIndex;
    Writer writer = {out, 0, 0, 0};
    unsigned char header[2] = {0x78, 0x01};
    unsigned char tail[4];
    unsigned long sum;

    /* PREPARE THE HISTORY WINDOW*/
    /* Hash heads and history links share one allocation and one lifetime.*/
    if (!head) {
        return 0;
    }
    prev = head + DEFLATE_HASH_TABLE_SIZE;
    for (byteIndex = 0; byteIndex < DEFLATE_HASH_TABLE_SIZE; byteIndex++) {
        head[byteIndex] = prev[byteIndex] = (size_t)-1;
    }
    if (!rcAppend(out, header, 2)) {
        free(head);
        return 0;
    }
    putBits(&writer, 3, 3);

    /* Bound hash-chain searches so repetitive inputs cannot cause unbounded work.*/
    while (inputOffset < inputLength && !writer.bad) {
        unsigned best = 0;
        unsigned dist = 0;
        unsigned hash = 0;
        size_t candidate = (size_t)-1;
        unsigned attempts = 0;
        unsigned step = 1;
        unsigned matchOffset;
        if (inputOffset + 2 < inputLength) {
            hash = ((unsigned)inputData[inputOffset] * DEFLATE_HASH_MULTIPLIER_FIRST +
                    (unsigned)inputData[inputOffset + 1] * DEFLATE_HASH_MULTIPLIER_SECOND +
                    inputData[inputOffset + 2]) &
                   DEFLATE_HASH_MASK;
            candidate = head[hash];
        }

        /* SEARCH PREVIOUS MATCHES*/
        /* The chain is bounded by both the 32 KiB window and a fixed attempt count.*/
        while (candidate != (size_t)-1 && candidate < inputOffset &&
               inputOffset - candidate <= DEFLATE_WINDOW_SIZE &&
               attempts++ < DEFLATE_MAX_CHAIN_ATTEMPTS) {
            unsigned match = 0;
            while (match < DEFLATE_MAX_MATCH_LENGTH && inputOffset + match < inputLength &&
                   inputData[candidate + match] == inputData[inputOffset + match]) {
                match++;
            }
            if (match >= 3 && match > best) {
                best = match;
                dist = (unsigned)(inputOffset - candidate);
                if (match == DEFLATE_MAX_MATCH_LENGTH) {
                    break;
                }
            }
            {
                size_t next = prev[candidate & DEFLATE_HASH_MASK];
                if (next >= candidate) {
                    break;
                }
                candidate = next;
            }
        }

        /* EMIT THE SELECTED REPRESENTATION*/
        /* A match emits length/distance codes; otherwise emit one literal byte.*/
        if (best >= 3) {
            unsigned codeIndex = 0;
            while (codeIndex < DEFLATE_LAST_LENGTH_BASE_INDEX && best >= lbase[codeIndex + 1]) {
                codeIndex++;
            }
            fixedSymbol(&writer, codeIndex + DEFLATE_FIRST_LENGTH_SYMBOL);
            putBits(&writer, best - lbase[codeIndex], lextra[codeIndex]);
            codeIndex = 0;
            while (codeIndex < DEFLATE_LAST_DISTANCE_BASE_INDEX && dist >= dbase[codeIndex + 1]) {
                codeIndex++;
            }
            putBits(&writer, reverseBits(codeIndex, 5), 5);
            putBits(&writer, dist - dbase[codeIndex], dextra[codeIndex]);
            step = best;
        } else {
            fixedSymbol(&writer, inputData[inputOffset]);
        }

        /* INDEX THE CONSUMED POSITIONS*/
        for (matchOffset = 0; matchOffset < step; matchOffset++) {
            size_t pos = inputOffset + matchOffset;
            if (pos + 2 < inputLength) {
                hash = ((unsigned)inputData[pos] * DEFLATE_HASH_MULTIPLIER_FIRST +
                        (unsigned)inputData[pos + 1] * DEFLATE_HASH_MULTIPLIER_SECOND +
                        inputData[pos + 2]) &
                       DEFLATE_HASH_MASK;
                prev[pos & DEFLATE_HASH_MASK] = head[hash];
                head[hash] = pos;
            }
        }
        inputOffset += step;
    }

    /* CLOSE THE ZLIB STREAM*/
    /* Finish the block, pad its last byte, and append the checksum of the original bytes.*/
    fixedSymbol(&writer, DEFLATE_END_OF_BLOCK_SYMBOL);
    if (writer.count) {
        putBits(&writer, 0, BITS_PER_BYTE - writer.count);
    }
    sum = adler(inputData, inputLength);
    for (byteIndex = 0; byteIndex < 4; byteIndex++) {
        tail[byteIndex] = (unsigned char)(sum >> (24 - byteIndex * BITS_PER_BYTE));
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
    return leftDistance <= aboveDistance && leftDistance <= upperLeftDistance ? left
           : aboveDistance <= upperLeftDistance                               ? above
                                                                              : upperLeft;
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
    if (depth == 16) {
        return ((unsigned)row[sample * 2] << BITS_PER_BYTE) | row[(sample * 2) + 1];
    }
    if (depth == BITS_PER_BYTE) {
        return row[sample];
    }
    return (row[sample * depth / BITS_PER_BYTE] >>
            (BITS_PER_BYTE - depth - (sample * depth % BITS_PER_BYTE))) &
           ((1U << depth) - 1);
}

/**
 * Reverse one PNG scanline filter using the prior decoded row when present.
 * @param row Packed row to read or update.
 * @param prior Previous decoded row, or NULL.
 * @param length Number of bytes to process.
 * @param bpp Bytes per pixel.
 * @param filter Filter selector.
 * @return 0 for an unknown filter; row and prior must each cover length bytes.
 * @author kai
 */
int rcUnfilter(unsigned char *row, const unsigned char *prior, size_t length, size_t bytesPerPixel,
               unsigned filter) {
    size_t byteIndex;
    if (filter > 4) {
        return 0;
    }
    /* RECONSTRUCT LEFT TO RIGHT*/
    /* Left neighbours already contain decoded bytes, including for packed colour samples.*/
    for (byteIndex = 0; byteIndex < length; byteIndex++) {
        int left = byteIndex >= bytesPerPixel ? row[byteIndex - bytesPerPixel] : 0;
        int above = prior ? prior[byteIndex] : 0;
        int upperLeft = prior && byteIndex >= bytesPerPixel ? prior[byteIndex - bytesPerPixel] : 0;
        int prediction = filter == 0   ? 0
                         : filter == 1 ? left
                         : filter == 2 ? above
                         : filter == 3 ? (left + above) / 2
                                       : paeth(left, above, upperLeft);
        row[byteIndex] = (unsigned char)(row[byteIndex] + prediction);
    }
    return 1;
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
    unsigned imageWidth = 0;
    unsigned imageHeight = 0;
    unsigned depth = 0;
    unsigned type = 0;
    unsigned interlace = 0;
    unsigned channels = 0;
    unsigned paletteCount = 0;
    unsigned char palette[PNG_PALETTE_ENTRY_COUNT][PNG_RGBA_CHANNEL_COUNT];
    unsigned transparency[PNG_RGB_CHANNEL_COUNT] = {PNG_SAMPLE_RANGE_16, PNG_SAMPLE_RANGE_16,
                                                    PNG_SAMPLE_RANGE_16};
    RcBuffer packed = {0};
    RcBuffer raw = {0};
    size_t inputOffset = BITS_PER_BYTE;
    size_t cursor = 0;
    size_t pixelCount;
    int header = 0;
    int ended = 0;
    int haveData = 0;
    int closedData = 0;
    int success = 0;
    unsigned passIndex;
    unsigned passes;
    static const unsigned passStartX[PNG_ADAM7_PASS_COUNT] = {0, 4, 0, 2, 0, 1, 0};
    static const unsigned passStartY[PNG_ADAM7_PASS_COUNT] = {0, 0, 4, 0, 2, 0, 1};
    static const unsigned passStepX[PNG_ADAM7_PASS_COUNT] = {
        ADAM7_INITIAL_STEP, ADAM7_INITIAL_STEP, 4, 4, 2, 2, 1};
    static const unsigned passStepY[PNG_ADAM7_PASS_COUNT] = {
        ADAM7_INITIAL_STEP, ADAM7_INITIAL_STEP, ADAM7_INITIAL_STEP, 4, 4, 2, 2};
    fillBytes(PNG_OPAQUE_ALPHA, sizeof(palette), palette);

    /* Validate chunk order and CRCs before interpreting image samples.*/
    while (inputOffset + PNG_CHUNK_OVERHEAD <= pngLength) {
        /* VALIDATE THE NEXT CHUNK*/
        /* CRC validation covers the chunk type and payload, before either is interpreted.*/
        unsigned long length = be32(pngData + inputOffset);
        const unsigned char *name = pngData + inputOffset + 4;
        const unsigned char *body = pngData + inputOffset + BITS_PER_BYTE;
        if (length > pngLength - inputOffset - PNG_CHUNK_OVERHEAD ||
            rcCrc(name, (size_t)length + 4) != be32(body + length)) {
            goto done;
        }
        if (!header && memcmp(name, "IHDR", 4) != 0) {
            goto done;
        }
        if (!memcmp(name, "IHDR", 4)) {
            /* READ IMAGE GEOMETRY*/
            /* Colour type controls channel count; bit depth controls packed sample extraction.*/
            if (header || length != PNG_HEADER_LENGTH) {
                goto done;
            }
            header = 1;
            imageWidth = (unsigned)be32(body);
            imageHeight = (unsigned)be32(body + 4);
            depth = body[BITS_PER_BYTE];
            type = body[9];
            interlace = body[12];
            if (!imageWidth || !imageHeight || (size_t)imageWidth > RC_PIXELS / imageHeight ||
                body[10] || body[11] || interlace > 1) {
                goto done;
            }
            /* CHECK THE COLOUR-TYPE CONTRACT*/
            /* Zero entries denote reserved colour types; low depths require gray or indexed data.*/
            {
                static const unsigned char channelCount[PNG_COLOR_CHANNEL_TABLE_SIZE] = {1, 0, 3, 1,
                                                                                         2, 0, 4};
                if (type > PNG_MAX_COLOR_TYPE) {
                    goto done;
                }
                channels = channelCount[type];
                if (!channels ||
                    !(depth == BITS_PER_BYTE || (depth == 16 && type != 3) ||
                      ((type == 0 || type == 3) && (depth == 1 || depth == 2 || depth == 4)))) {
                    goto done;
                }
            }
        } else if (!memcmp(name, "PLTE", 4)) {
            unsigned entryIndex;
            if (haveData || !length || length % PNG_RGB_CHANNEL_COUNT ||
                length > PNG_MAX_PALETTE_BYTES) {
                goto done;
            }

            /* LOAD PALETTE ENTRIES*/
            /* PNG palettes store RGB triples; transparency is applied by the later tRNS chunk.*/
            paletteCount = (unsigned)length / PNG_RGB_CHANNEL_COUNT;
            for (entryIndex = 0; entryIndex < paletteCount; entryIndex++) {
                copyBytes(body + ((size_t)entryIndex * 3), 3, palette[entryIndex]);
            }
        } else if (!memcmp(name, "tRNS", 4)) {
            unsigned entryIndex;
            if (haveData) {
                goto done;
            }

            /* READ TRANSPARENCY METADATA*/
            /* Palette alpha and raw sample transparency keys follow different layouts.*/
            if (type == 3) {
                if (!paletteCount || length > paletteCount) {
                    goto done;
                }
                for (entryIndex = 0; entryIndex < length; entryIndex++) {
                    palette[entryIndex][3] = body[entryIndex];
                }
            } else if (type == 0 && length == 2) {
                transparency[0] = ((unsigned)body[0] << BITS_PER_BYTE) | body[1];
            } else if (type == 2 && length == 6) {
                for (entryIndex = 0; entryIndex < 3; entryIndex++) {
                    transparency[entryIndex] =
                        ((unsigned)body[((size_t)entryIndex * 2)] << BITS_PER_BYTE) |
                        body[((size_t)entryIndex * 2) + 1];
                }
            } else {
                goto done;
            }
        } else if (!memcmp(name, "IDAT", 4)) {
            /* COLLECT COMPRESSED PIXELS*/
            /* IDAT chunks form one zlib stream and must remain contiguous.*/
            if (closedData || !rcAppend(&packed, body, length)) {
                goto done;
            }
            haveData = 1;
        } else if (!memcmp(name, "acTL", 4)) {
            goto done;
        } else if (!memcmp(name, "IEND", 4)) {
            if (length || !haveData) {
                goto done;
            }
            ended = 1;
            inputOffset += PNG_CHUNK_OVERHEAD;
            break;
        } else {
            if (haveData) {
                closedData = 1;
            }
            if (!(name[0] & 32)) {
                goto done;
            }
        }
        inputOffset += (size_t)length + PNG_CHUNK_OVERHEAD;
    }

    /* ALLOCATE THE DECODED IMAGE*/
    /* Only inflate after the full chunk envelope and terminal marker have been checked.*/
    if (!ended || inputOffset != pngLength || !rcInflate(packed.data, packed.size, &raw) ||
        (type == 3 && !paletteCount)) {
        goto done;
    }
    pixelCount = (size_t)imageWidth * imageHeight;
    if (!pixelCount) {
        goto done;
    }
    image->rgba = calloc(pixelCount, sizeof(*image->rgba));
    if (!image->rgba) {
        goto done;
    }
    image->width = imageWidth;
    image->height = imageHeight;
    passes = interlace ? PNG_ADAM7_PASS_COUNT : 1;

    /* Each Adam7 passIndex has an independent previous-row filter context.*/
    for (passIndex = 0; passIndex < passes; passIndex++) {
        /* PREPARE ONE INTERLACE PASS*/
        /* Each passIndex writes its smaller raster into strided positions of the final image.*/
        unsigned passOriginX = interlace ? passStartX[passIndex] : 0;
        unsigned passOriginY = interlace ? passStartY[passIndex] : 0;
        unsigned passStrideX = interlace ? passStepX[passIndex] : 1;
        unsigned passStrideY = interlace ? passStepY[passIndex] : 1;
        unsigned passWidth = imageWidth > passOriginX
                                 ? (imageWidth - passOriginX + passStrideX - 1) / passStrideX
                                 : 0;
        unsigned passHeight = imageHeight > passOriginY
                                  ? (imageHeight - passOriginY + passStrideY - 1) / passStrideY
                                  : 0;
        unsigned rowIndex;
        unsigned columnIndex;
        size_t rowBytes = ((size_t)passWidth * channels * depth + 7) / BITS_PER_BYTE;
        size_t bpp = (channels * depth + 7) / BITS_PER_BYTE;
        unsigned char *prior = NULL;
        unsigned char *row = NULL;
        if (!passWidth || !passHeight) {
            continue;
        }
        if (rowBytes + 1 > raw.size || passHeight > (raw.size - cursor) / (rowBytes + 1)) {
            goto done;
        }
        for (rowIndex = 0; rowIndex < passHeight; rowIndex++) {
            /* RECONSTRUCT ONE ROW*/
            /* Reuse the inflated storage; earlier rows remain available as predictor context.*/
            unsigned filter = raw.data[cursor++];
            row = raw.data + cursor;
            cursor += rowBytes;
            if (!rcUnfilter(row, prior, rowBytes, bpp, filter)) {
                goto done;
            }

            /* EXPAND SAMPLES TO RGBA*/
            /* Retain alpha and compare transparency keys before reducing source precision.*/
            for (columnIndex = 0; columnIndex < passWidth; columnIndex++) {
                unsigned samples[4] = {0, 0, 0, 255};
                unsigned channelIndex;
                unsigned char *pixel =
                    image->rgba +
                    (((size_t)passOriginY + ((size_t)rowIndex * passStrideY)) * imageWidth +
                     passOriginX + (size_t)columnIndex * passStrideX) *
                        4;
                for (channelIndex = 0; channelIndex < channels; channelIndex++) {
                    samples[channelIndex] =
                        rcSample(row, ((size_t)columnIndex * channels) + channelIndex, depth);
                }
                if (type == 3) {
                    if (samples[0] >= paletteCount) {
                        goto done;
                    }
                    copyBytes(palette[samples[0]], 4, pixel);
                } else {
                    unsigned max = (1U << depth) - 1;
                    pixel[3] = 255;
                    if (type == 0 || type == 4) {
                        pixel[0] = pixel[1] = pixel[2] = (unsigned char)(samples[0] * 255U / max);
                        if (type == 4) {
                            pixel[3] = (unsigned char)(samples[1] * 255U / max);
                        } else if (samples[0] == transparency[0]) {
                            pixel[3] = 0;
                        }
                    } else {
                        for (channelIndex = 0; channelIndex < 3; channelIndex++) {
                            pixel[channelIndex] =
                                (unsigned char)(samples[channelIndex] * 255U / max);
                        }
                        if (type == 6) {
                            pixel[3] = (unsigned char)(samples[3] * 255U / max);
                        } else if (samples[0] == transparency[0] && samples[1] == transparency[1] &&
                                   samples[2] == transparency[2]) {
                            pixel[3] = 0;
                        }
                    }
                }
            }
            prior = row;
        }
    }

    /* VERIFY AND RELEASE DECODER STATE*/
    if (cursor != raw.size) {
        goto done;
    }
    success = 1;
done:
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
    if (pngLength >= 2 && pngData[0] == 255 && pngData[1] == 216) {
        return rcJpeg(pngData, pngLength, image, 1);
    }
    return 0;
}
