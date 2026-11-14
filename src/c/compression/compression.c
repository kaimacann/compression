/**
 * @file compression.c
 * @brief Streaming Huffman codec and byte-oriented inspection helpers.
 *
 * Huffman frames add 256 big-endian 64-bit symbol counts before the bitstream.
 * @author kai
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* STANDARD LIBRARIES*/
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FILE AND MEDIA CONTRACTS*/
#include "compression.h"
#include "media.h"

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

/* LIMITS AND STATE*/

#define SYMBOL_COUNT 256
#define NODE_CAPACITY 511
#define BUFFER_SIZE 8192
#define HEADER_SIZE 22
#define FREQUENCY_TABLE_SIZE ((unsigned long long)SYMBOL_COUNT * BITS_PER_BYTE)
#define ULL_MAX_VALUE (~0ULL)
#define FNV1A_OFFSET_BASIS 14695981039346656037ULL
#define FNV1A_PRIME 1099511628211ULL
#define BITS_PER_BYTE 8
#define PDF_SIGNATURE_LENGTH 5
#define PNG_SIGNATURE_LENGTH BITS_PER_BYTE
#define JPEG_SIGNATURE_LENGTH 2
#define JPEG_START_MARKER (SYMBOL_COUNT - 1)
#define JPEG_FRAME_MARKER 216
#define ARCHIVE_PREFIX_LENGTH 6
#define ARCHIVE_MAGIC_LENGTH 5
#define ARCHIVE_VERSION 1
#define RAW_MODE 0
#define HUFFMAN_MODE 1
#define REPORT_LIMIT 20
#define TEXT_SPACE 32
#define TEXT_TILDE 126
#define DEFAULT_PALETTE_COLORS 16
#define DEFAULT_CONTOUR_TOLERANCE 0.5
#define DECIMAL_BASE 10
#define MOST_FREQUENT_COUNT 10

/* FILE TRANSACTIONS*/
/**
 * Open a source path for binary reading and report open failures.
 * @param path Existing source file to open.
 * @return Open binary input stream, or NULL if the path cannot be opened.
 * @author kai
 */
static FILE *openInputFile(const char *path) {
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file && path) {
        perror(path);
    }
    return file;
}

/**
 * Create a new output path exclusively so existing files are never truncated.
 * @param path New destination to create without truncating an existing file.
 * @return New binary output stream, or NULL when the destination already exists or cannot be
 * created.
 * @author kai
 */
static FILE *createOutputFile(const char *path) {
    FILE *file = path ? fopen(path, "wbx") : NULL;
    if (!file && path) {
        perror(path);
    }
    return file;
}

/**
 * Flush and close an output stream, removing its partial file if finalization fails.
 * @param file Open output stream to close.
 * @param path Destination path to remove if closing the stream fails.
 * @return Nonzero when flush and close succeed; zero after removing a failed output.
 * @author kai
 */
static int finishOutputFile(FILE *file, const char *path) {
    if (fclose(file) == 0) {
        return 1;
    }
    perror(path);
    if (remove(path) != 0) {
        perror(path);
    }
    return 0;
}

/**
 * Close an owned input stream and report a close failure.
 * @param file Open input stream.
 * @param path Input path associated with a close failure.
 * @return Nonzero when the stream closes successfully.
 * @author kai
 */
static int closeInputFile(FILE *file, const char *path) {
    if (fclose(file) == 0) {
        return 1;
    }
    perror(path);
    return 0;
}

/**
 * Close an output stream and remove its incomplete destination.
 * @param file Open output stream to close.
 * @param path Incomplete destination path to remove.
 * @author kai
 */
static void discardOutputFile(FILE *file, const char *path) {
    if (fclose(file) != 0) {
        perror(path);
    }
    if (remove(path) != 0) {
        perror(path);
    }
}

#ifdef DEBUG
#define DEBUG_LOG(...) fprintf(stderr, "[debug] " __VA_ARGS__)
#else
#define DEBUG_LOG(...) ((void)0)
#endif

typedef struct {
    unsigned long long frequency;
    int symbol;
    int left;
    int right;
    int minimumSymbol;
} HuffmanNode;

typedef struct {
    int next[NODE_CAPACITY];
    int head;
} NodeQueue;

typedef struct {
    unsigned char byte;
    int remaining;
} BitReader;

typedef struct {
    int symbol;
    unsigned long long frequency;
} SymbolFrequency;

typedef struct {
    unsigned long long frequencies[SYMBOL_COUNT];
    unsigned long long totalBytes;
    unsigned long long checksum;
} InputStatistics;

typedef struct {
    HuffmanNode nodes[NODE_CAPACITY];
    unsigned char codes[SYMBOL_COUNT][SYMBOL_COUNT];
    unsigned char codePath[SYMBOL_COUNT];
    unsigned short codeLengths[SYMBOL_COUNT];
    int root;
    unsigned long long bitCount;
} HuffmanEncoding;

typedef struct {
    int mode;
    unsigned long long originalSize;
    unsigned long long checksum;
    unsigned long long *frequencies;
} ArchiveMetadata;

typedef struct {
    FILE *input;
    FILE *output;
} StreamPair;

typedef struct {
    unsigned long long *frequencies;
    unsigned long long checksum;
} PayloadMetrics;
typedef struct {
    StreamPair streams;
    const ArchiveMetadata *metadata;
    const InputStatistics *statistics;
    const HuffmanEncoding *encoding;
    int useHuffman;
    PayloadMetrics *metrics;
} ArchiveWriteContext;

/*//////////////////////////////////////////////////
HUFFMAN
//////////////////////////////////////////////////*/

/* TREE*/
/**
 * Compare two Huffman nodes by weight, then by stable symbol order.
 * @param nodes Tree nodes containing weight and stable symbol keys.
 * @param left First node index to compare.
 * @param right Second node index to compare.
 * @return Nonzero when left should precede right in deterministic queue order.
 * @author kai
 */
static int nodeLess(const HuffmanNode nodes[], int left, int right) {
    if (nodes[left].frequency != nodes[right].frequency) {
        return nodes[left].frequency < nodes[right].frequency;
    }
    if (nodes[left].minimumSymbol != nodes[right].minimumSymbol) {
        return nodes[left].minimumSymbol < nodes[right].minimumSymbol;
    }
    return left < right;
}

/**
 * Insert a node into the frequency-ordered Huffman work queue.
 * @param queue Sorted queue that receives the node index.
 * @param nodes Tree nodes used to order the insertion.
 * @param node Unused node index to insert.
 * @author kai
 */
static void queueInsert(NodeQueue *queue, const HuffmanNode nodes[], int node) {
    int *nodeLink = &queue->head;
    while (*nodeLink >= 0 && nodeLess(nodes, *nodeLink, node)) {
        nodeLink = &queue->next[*nodeLink];
    }
    queue->next[node] = *nodeLink;
    *nodeLink = node;
}

/**
 * Remove and return the lowest-weight node from the Huffman work queue.
 * @param queue Non-empty work queue whose head has minimum weight.
 * @return Index of the removed minimum-weight node.
 * @author kai
 */
static int takeMinimum(NodeQueue *queue) {
    int result = queue->head;
    queue->head = queue->next[result];
    return result;
}

/**
 * Build a deterministic Huffman tree from symbol frequencies and return its root.
 * @param frequencies Count for each input byte value.
 * @param nodes Storage for leaves and generated internal nodes.
 * @param root Receives the root node index; -1 denotes an empty alphabet.
 * @return Nonzero when a valid tree is built; zero if the alphabet cannot fit the node storage.
 * @author kai
 */
static int buildHuffmanTree(const unsigned long long frequencies[], HuffmanNode nodes[],
                            int *root) {
    NodeQueue queue = {{0}, -1};
    int symbol;
    int nodeCount = 0;

    /* SEED THE SYMBOL CANDIDATES*/
    /* Only symbols present in the input receive leaves.*/
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        if (frequencies[symbol] == 0) {
            continue;
        }
        nodes[nodeCount] = (HuffmanNode){frequencies[symbol], symbol, -1, -1, symbol};
        queueInsert(&queue, nodes, nodeCount++);
    }

    /* MERGE THE TWO LOWEST FREQUENCIES*/
    /* Stable symbol tie-breaking makes the resulting prefix codes deterministic.*/
    while (queue.head >= 0 && queue.next[queue.head] >= 0) {
        int left = takeMinimum(&queue);
        int right = takeMinimum(&queue);
        int parent = nodeCount++;

        if (nodes[left].frequency > ULL_MAX_VALUE - nodes[right].frequency) {
            return 0;
        }
        nodes[parent] = (HuffmanNode){
            nodes[left].frequency + nodes[right].frequency,
            -1,
            left,
            right,
            nodes[left].minimumSymbol < nodes[right].minimumSymbol ? nodes[left].minimumSymbol
                                                                   : nodes[right].minimumSymbol,
        };
        queueInsert(&queue, nodes, parent);
    }
    *root = queue.head;
    return 1;
}

/* PREFIX CODES*/
/**
 * Walk a Huffman tree to assign each leaf its bit path and code length.
 * @param nodes Completed Huffman tree.
 * @param nodeIndex Current node index in the tree.
 * @param path Bit path being built and copied to leaf code entries.
 * @param depth Current recursion depth in the Huffman tree.
 * @param codes Receives each symbol's Huffman bit sequence.
 * @param codeLengths Receives the bit length for each byte symbol.
 * @author kai
 */
static void assignCodes(const HuffmanNode nodes[], int nodeIndex, unsigned char path[], int depth,
                        unsigned char codes[][SYMBOL_COUNT], unsigned short codeLengths[]) {
    if (nodes[nodeIndex].symbol >= 0) {
        int symbol = nodes[nodeIndex].symbol;
        if (depth == 0) {
            path[0] = 0;
            depth = 1;
        }
        memcpy(codes[symbol], path, (size_t)depth);
        codeLengths[symbol] = (unsigned short)depth;
        return;
    }

    path[depth] = 0;
    assignCodes(nodes, nodes[nodeIndex].left, path, depth + 1, codes, codeLengths);
    path[depth] = 1;
    assignCodes(nodes, nodes[nodeIndex].right, path, depth + 1, codes, codeLengths);
}

/*//////////////////////////////////////////////////
CONTAINER
//////////////////////////////////////////////////*/

/* SOURCE STATISTICS*/
/**
 * Count input bytes and symbol frequencies, then rewind the stream.
 * @param input Seekable input stream to scan and rewind.
 * @param statistics Receives byte frequencies, total size, and checksum.
 * @return Nonzero when the whole stream is counted and rewound; zero on read, seek, or size error.
 * @author kai
 */
static int scanInput(FILE *input, InputStatistics *statistics) {
    unsigned char buffer[BUFFER_SIZE];
    size_t count;
    size_t index;
    int symbol;

    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        statistics->frequencies[symbol] = 0;
    }
    statistics->totalBytes = 0;
    statistics->checksum = FNV1A_OFFSET_BASIS;
    do {
        count = fread(buffer, 1, sizeof(buffer), input);
        if ((unsigned long long)count > ULL_MAX_VALUE - statistics->totalBytes) {
            return 0;
        }
        statistics->totalBytes += (unsigned long long)count;
        for (index = 0; index < count; index++) {
            unsigned int symbolValue = buffer[index];
            /* Each symbol count is bounded by the checked total above.*/
            statistics->frequencies[symbolValue]++;
            statistics->checksum ^= (unsigned long long)buffer[index];
            statistics->checksum *= FNV1A_PRIME;
        }
    } while (count == sizeof(buffer));
    return !ferror(input);
}

/**
 * Measure an archive source and validate its representable frame size.
 * @param input Source stream.
 * @param statistics Receives source frequencies, size, and checksum.
 * @return Nonzero when the complete source is readable and fits TCMP v1.
 * @author kai
 */
static int measureArchiveSource(FILE *input, InputStatistics *statistics) {
    if (!scanInput(input, statistics)) {
        (void)fputs("Could not read the complete input or its size exceeded the supported range.\n",
                    stderr);
        return 0;
    }
    if (statistics->totalBytes > ULL_MAX_VALUE - (unsigned long long)HEADER_SIZE -
                                     (unsigned long long)FREQUENCY_TABLE_SIZE) {
        (void)fputs("Input is too large for the TCMP v1 size fields.\n", stderr);
        return 0;
    }
    return 1;
}

/**
 * Detect PNG, JPEG, or PDF input from its leading signature bytes.
 * @param input Seekable stream whose leading bytes identify its format.
 * @return 1 for PNG/JPEG/PDF, 0 for other bytes, or -1 if reading or rewind fails.
 * @author kai
 */
static int isMediaInput(FILE *input) {
    unsigned char signature[PNG_SIGNATURE_LENGTH];
    size_t count = fread(signature, 1, sizeof(signature), input);
    if (ferror(input) || fseek(input, 0, SEEK_SET) != 0) {
        return -1;
    }
    return (count >= PDF_SIGNATURE_LENGTH &&
            memcmp(signature, "%PDF-", PDF_SIGNATURE_LENGTH) == 0) ||
           (count >= PNG_SIGNATURE_LENGTH &&
            memcmp(signature, "\211PNG\r\n\032\n", PNG_SIGNATURE_LENGTH) == 0) ||
           (count >= JPEG_SIGNATURE_LENGTH && signature[0] == JPEG_START_MARKER &&
            signature[1] == JPEG_FRAME_MARKER);
}

/**
 * Sum the encoded bit cost for all symbols using their code lengths.
 * @param frequencies Count for each byte value.
 * @param codeLengths Huffman code length assigned to each byte value.
 * @param bitCount Receives the total encoded bit count, or fails on overflow.
 * @return Nonzero when the bit total fits; zero if multiplication or addition would overflow.
 * @author kai
 */
static int calculateEncodedBits(const unsigned long long frequencies[],
                                const unsigned short codeLengths[], unsigned long long *bitCount) {
    int symbol;

    *bitCount = 0;
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        unsigned long long length = codeLengths[symbol];
        unsigned long long contribution;
        if (length != 0 && frequencies[symbol] > ULL_MAX_VALUE / length) {
            return 0;
        }
        contribution = frequencies[symbol] * length;
        if (contribution > ULL_MAX_VALUE - *bitCount) {
            return 0;
        }
        *bitCount += contribution;
    }
    return 1;
}

/**
 * Build a deterministic Huffman table and measure its encoded payload size.
 * @param statistics Source frequency statistics.
 * @param encoding Receives the Huffman tree, codes, and total encoded bits.
 * @return Nonzero when a valid encoding fits the supported integer range.
 * @author kai
 */
static int prepareArchiveEncoding(const InputStatistics *statistics, HuffmanEncoding *encoding) {
    encoding->root = -1;
    if (!buildHuffmanTree(statistics->frequencies, encoding->nodes, &encoding->root)) {
        (void)fputs("Input is too large to represent in the TCMP format.\n", stderr);
        return 0;
    }
    if (encoding->root >= 0) {
        assignCodes(encoding->nodes, encoding->root, encoding->codePath, 0, encoding->codes,
                    encoding->codeLengths);
    }
    if (!calculateEncodedBits(statistics->frequencies, encoding->codeLengths,
                              &encoding->bitCount)) {
        (void)fputs("Encoded size exceeds the supported range.\n", stderr);
        return 0;
    }
    return 1;
}

/* HEADER FIELDS*/
/**
 * Write an integer as eight big-endian bytes.
 * @param stream Destination stream.
 * @param value Integer to encode as eight big-endian bytes.
 * @return Nonzero when all eight bytes are written.
 * @author kai
 */
static int writeUnsignedLongLong(FILE *stream, unsigned long long value) {
    unsigned char bytes[BITS_PER_BYTE];
    int byteIndex;
    for (byteIndex = BITS_PER_BYTE - 1; byteIndex >= 0; byteIndex--) {
        bytes[byteIndex] = (unsigned char)(value & (SYMBOL_COUNT - 1ULL));
        value >>= (unsigned)BITS_PER_BYTE;
    }
    return fwrite(bytes, 1, sizeof(bytes), stream) == sizeof(bytes);
}

/**
 * Read an eight-byte big-endian integer and fail on a truncated stream.
 * @param stream Source stream positioned at the integer.
 * @param value Receives the decoded integer only after a full read.
 * @return Nonzero when all eight bytes are read; zero for a truncated stream.
 * @author kai
 */
static int readUnsignedLongLong(FILE *stream, unsigned long long *value) {
    unsigned char bytes[BITS_PER_BYTE];
    int byteIndex;
    if (fread(bytes, 1, sizeof(bytes), stream) != sizeof(bytes)) {
        return 0;
    }
    *value = 0;
    for (byteIndex = 0; byteIndex < BITS_PER_BYTE; byteIndex++) {
        *value = (*value << (unsigned)BITS_PER_BYTE) | (unsigned long long)bytes[byteIndex];
    }
    return 1;
}

/**
 * Write the archive magic, mode, original size, and mode-specific metadata.
 * @param output Destination archive stream.
 * @param metadata Archive mode, original size, checksum, and optional symbol counts.
 * @return Nonzero when all required header fields are written.
 * @author kai
 */
static int writeHeader(FILE *output, const ArchiveMetadata *metadata) {
    unsigned char prefix[ARCHIVE_PREFIX_LENGTH] = {
        'T', 'C', 'M', 'P', ARCHIVE_VERSION, (unsigned char)metadata->mode,
    };
    int symbol;

    if (fwrite(prefix, 1, sizeof(prefix), output) != sizeof(prefix) ||
        !writeUnsignedLongLong(output, metadata->originalSize) ||
        !writeUnsignedLongLong(output, metadata->checksum)) {
        return 0;
    }
    if (metadata->mode == HUFFMAN_MODE) {
        for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
            if (!writeUnsignedLongLong(output, metadata->frequencies[symbol])) {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Validate and parse an archive header before payload decoding.
 * @param input Archive stream positioned at its first byte.
 * @param metadata Receives the validated mode, size, checksum, and optional symbol counts.
 * @return Nonzero when the header fields and frequency total validate.
 * @author kai
 */
static int readHeader(FILE *input, ArchiveMetadata *metadata) {
    unsigned char prefix[ARCHIVE_PREFIX_LENGTH];
    int symbol;

    if (fread(prefix, 1, sizeof(prefix), input) != sizeof(prefix) ||
        memcmp(prefix, "TCMP\1", ARCHIVE_MAGIC_LENGTH) != 0 ||
        prefix[ARCHIVE_PREFIX_LENGTH - 1] > HUFFMAN_MODE ||
        !readUnsignedLongLong(input, &metadata->originalSize) ||
        !readUnsignedLongLong(input, &metadata->checksum)) {
        return 0;
    }

    metadata->mode = prefix[ARCHIVE_PREFIX_LENGTH - 1];
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        metadata->frequencies[symbol] = 0;
    }
    if (metadata->mode == HUFFMAN_MODE) {
        unsigned long long total = 0;
        for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
            if (!readUnsignedLongLong(input, &metadata->frequencies[symbol]) ||
                metadata->frequencies[symbol] > ULL_MAX_VALUE - total) {
                return 0;
            }
            total += metadata->frequencies[symbol];
        }
        if (total != metadata->originalSize) {
            return 0;
        }
    }
    return 1;
}

/*//////////////////////////////////////////////////
PAYLOADS
//////////////////////////////////////////////////*/

/* ENCODING*/
/**
 * Copy exactly the declared payload length and report short reads or writes.
 * @param streams Source and destination streams for the payload transfer.
 * @param size Exact number of payload bytes to transfer.
 * @param metrics Receives the checksum and optional symbol counts.
 * @return Nonzero when the declared bytes copy successfully and no trailing bytes remain.
 * @author kai
 */
static int copyPayload(StreamPair streams, unsigned long long size, PayloadMetrics *metrics) {
    unsigned char buffer[BUFFER_SIZE];
    size_t byteIndex;
    int symbol;
    if (metrics->frequencies) {
        for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
            metrics->frequencies[symbol] = 0;
        }
    }
    metrics->checksum = FNV1A_OFFSET_BASIS;

    /* COPY AND MEASURE THE DECLARED PAYLOAD*/
    /* Both directions share length/checksum checks; encoding also rebuilds symbol counts.*/
    while (size) {
        size_t count = size < sizeof(buffer) ? (size_t)size : sizeof(buffer);
        if (fread(buffer, 1, count, streams.input) != count ||
            fwrite(buffer, 1, count, streams.output) != count) {
            return 0;
        }
        for (byteIndex = 0; byteIndex < count; byteIndex++) {
            if (metrics->frequencies) {
                metrics->frequencies[buffer[byteIndex]]++;
            }
            metrics->checksum ^= buffer[byteIndex];
            metrics->checksum *= FNV1A_PRIME;
        }
        size -= count;
    }
    return fgetc(streams.input) == EOF && !ferror(streams.input);
}

/**
 * Encode input symbols with the supplied Huffman codes and pad the final byte.
 * @param streams Source and destination streams for encoded payload.
 * @param codes Bit pattern assigned to each byte value.
 * @param codeLengths Number of significant bits in each code.
 * @param metrics Receives the checksum and symbol counts for encoded bytes.
 * @return Nonzero when the full payload encodes and writes successfully.
 * @author kai
 */
static int writeHuffmanPayload(StreamPair streams, const unsigned char codes[][SYMBOL_COUNT],
                               const unsigned short codeLengths[], PayloadMetrics *metrics) {
    unsigned char buffer[BUFFER_SIZE];
    unsigned char outByte = 0;
    int usedBits = 0;
    size_t count;
    size_t byteIndex;
    int symbolIndex;

    for (symbolIndex = 0; symbolIndex < SYMBOL_COUNT; symbolIndex++) {
        metrics->frequencies[symbolIndex] = 0;
    }
    metrics->checksum = FNV1A_OFFSET_BASIS;

    /* ENCODE EACH INPUT SYMBOL*/
    /* The second scan rebuilds counts and checksum while packing the selected prefix codes.*/
    do {
        count = fread(buffer, 1, sizeof(buffer), streams.input);
        for (byteIndex = 0; byteIndex < count; byteIndex++) {
            unsigned int symbol = buffer[byteIndex];
            unsigned int bit;
            metrics->frequencies[symbol]++;
            metrics->checksum ^= (unsigned long long)symbol;
            metrics->checksum *= FNV1A_PRIME;
            for (bit = 0; bit < codeLengths[symbol]; bit++) {
                outByte = (unsigned char)(((unsigned)outByte << 1U) | codes[symbol][bit]);
                usedBits++;
                if (usedBits == BITS_PER_BYTE) {
                    if (fputc(outByte, streams.output) == EOF) {
                        return 0;
                    }
                    outByte = 0;
                    usedBits = 0;
                }
            }
        }
    } while (count == sizeof(buffer));
    if (ferror(streams.input)) {
        return 0;
    }

    /* PAD THE LAST PAYLOAD BYTE*/
    /* Unused low bits stay zero so the decoder can verify the end of the frame.*/
    if (usedBits > 0) {
        outByte = (unsigned char)(outByte << (unsigned)(BITS_PER_BYTE - usedBits));
        if (fputc(outByte, streams.output) == EOF) {
            return 0;
        }
    }
    return 1;
}

/* VALIDATION*/
/**
 * Check whether two symbol-frequency tables match exactly.
 * @param first First byte-frequency table.
 * @param second Second byte-frequency table to compare.
 * @return Nonzero only when every symbol count matches.
 * @author kai
 */
static int sameFrequencies(const unsigned long long first[], const unsigned long long second[]) {
    return memcmp(first, second, sizeof(unsigned long long) * SYMBOL_COUNT) == 0;
}

/**
 * Read one payload bit, loading the next byte when needed.
 * @param input Encoded payload stream.
 * @param reader Persistent bit-reader state.
 * @param bit Receives the next bit, or remains unchanged on failure.
 * @return Nonzero when one bit is read; zero at end of input.
 * @author kai
 */
static int bitReaderGet(FILE *input, BitReader *reader, int *bit) {
    if (reader->remaining == 0) {
        int next = fgetc(input);
        if (next == EOF) {
            return 0;
        }
        reader->byte = (unsigned char)next;
        reader->remaining = BITS_PER_BYTE;
    }
    reader->remaining--;
    *bit = (int)(((unsigned)reader->byte >> (unsigned)reader->remaining) & 1U);
    return 1;
}

/**
 * Check that the Huffman payload ends with valid zero padding and no extra data.
 * @param input Payload stream after the expected symbols were decoded.
 * @param reader Reader state containing unused padding bits.
 * @return Nonzero when remaining padding is zero and no trailing data exists.
 * @author kai
 */
static int finishBitReader(FILE *input, const BitReader *reader) {
    int next;
    if (reader->remaining > 0) {
        unsigned int paddingMask = (1U << (unsigned)reader->remaining) - 1U;
        if (((unsigned int)reader->byte & paddingMask) != 0U) {
            return 0;
        }
    }
    next = fgetc(input);
    return next == EOF && !ferror(input);
}

/**
 * Decode one symbol while validating each traversed Huffman node.
 * @param input Encoded payload stream.
 * @param reader Persistent bit-reader state.
 * @param nodes Huffman tree nodes addressed by the payload.
 * @param root Index of the validated tree root.
 * @param symbol Receives the decoded byte symbol.
 * @return Nonzero when a valid leaf is reached.
 * @author kai
 */
static int readHuffmanSymbol(FILE *input, BitReader *reader, const HuffmanNode nodes[], int root,
                             int *symbol) {
    int nodeIndex = root;
    while (nodeIndex >= 0 && nodeIndex < NODE_CAPACITY && nodes[nodeIndex].symbol < 0) {
        int bit;
        if (!bitReaderGet(input, reader, &bit)) {
            return 0;
        }
        nodeIndex = bit == 0 ? nodes[nodeIndex].left : nodes[nodeIndex].right;
    }
    if (nodeIndex < 0 || nodeIndex >= NODE_CAPACITY || nodes[nodeIndex].symbol < 0 ||
        nodes[nodeIndex].symbol >= SYMBOL_COUNT) {
        return 0;
    }
    *symbol = nodes[nodeIndex].symbol;
    return 1;
}

/**
 * Decode exactly the original byte count and write each recovered symbol.
 * @param streams Encoded input and restored output streams.
 * @param originalSize Exact number of bytes to decode.
 * @param frequencies Expected counts used to reject invalid symbols.
 * @param nodes Validated Huffman tree nodes.
 * @param root Root index of the decoding tree.
 * @param metrics Receives the checksum of restored bytes.
 * @return Nonzero when all symbols, frequencies, padding, and output writes validate.
 * @author kai
 */
static int writeDecodedHuffman(StreamPair streams, unsigned long long originalSize,
                               const unsigned long long frequencies[], const HuffmanNode nodes[],
                               int root, PayloadMetrics *metrics) {
    BitReader reader = {0, 0};
    unsigned long long decoded[SYMBOL_COUNT] = {0};
    unsigned long long written;
    metrics->checksum = FNV1A_OFFSET_BASIS;

    if (root < 0 || root >= NODE_CAPACITY) {
        return 0;
    }
    if (originalSize && nodes[root].symbol < 0 &&
        (nodes[root].left < 0 || nodes[root].left >= NODE_CAPACITY || nodes[root].right < 0 ||
         nodes[root].right >= NODE_CAPACITY)) {
        return 0;
    }

    for (written = 0; written < originalSize; written++) {
        int symbol;
        int bit;
        if (nodes[root].symbol >= 0 && (!bitReaderGet(streams.input, &reader, &bit) || bit != 0)) {
            return 0;
        }
        if (!readHuffmanSymbol(streams.input, &reader, nodes, root, &symbol)) {
            return 0;
        }
        if (decoded[symbol] >= frequencies[symbol]) {
            return 0;
        }
        decoded[symbol]++;
        metrics->checksum ^= (unsigned long long)symbol;
        metrics->checksum *= FNV1A_PRIME;
        if (fputc(symbol, streams.output) == EOF) {
            return 0;
        }
    }
    if (!sameFrequencies(decoded, frequencies)) {
        return 0;
    }
    return finishBitReader(streams.input, &reader);
}

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* FORMAT ROUTING*/
/**
 * Compress a file into the repository archive format, preserving input on failure.
 * @param inputPath Source file path.
 * @param outputPath New archive or processed-media destination.
 * @return Nonzero when media processing or TCMP archiving succeeds.
 * @author kai
 */
int compressFile(const char *inputPath, const char *outputPath) {
    FILE *input = openInputFile(inputPath);
    MediaOptions options = {0, 0, DEFAULT_PALETTE_COLORS, DEFAULT_CONTOUR_TOLERANCE, NULL};
    int media;
    if (!input) {
        return 0;
    }
    media = isMediaInput(input);
    if (fclose(input) || media < 0) {
        return 0;
    }
    return media ? processMedia(inputPath, outputPath, &options)
                 : archiveFile((CompressionPaths){inputPath, outputPath});
}

/* ARCHIVE OPERATIONS*/
/**
 * Write an archive header and payload, then verify the second source pass.
 * @param context Archive streams, metadata, and payload encoding state.
 * @return Nonzero when writing and verification both succeed.
 * @author kai
 */
static int writeArchiveFrame(ArchiveWriteContext *context) {
    if (!writeHeader(context->streams.output, context->metadata)) {
        return 0;
    }
    if (!(context->useHuffman
              ? writeHuffmanPayload(context->streams, context->encoding->codes,
                                    context->encoding->codeLengths, context->metrics)
              : copyPayload(context->streams, context->statistics->totalBytes, context->metrics)) ||
        !sameFrequencies(context->statistics->frequencies, context->metrics->frequencies) ||
        context->metrics->checksum != context->statistics->checksum) {
        return 0;
    }
    return 1;
}

/**
 * Encode a supported media file and its vector exports as one archive.
 * @param paths Source file path and new archive destination; existing files are preserved.
 * @return Nonzero when the archive is written and verified against a second source pass.
 * @author kai
 */
int archiveFile(CompressionPaths paths) {
    const char *inputPath = paths.input;
    const char *outputPath = paths.output;
    FILE *input = openInputFile(inputPath);
    FILE *output = NULL;
    InputStatistics statistics;
    unsigned long long observed[SYMBOL_COUNT];
    StreamPair streams;
    PayloadMetrics metrics = {observed, 0};
    unsigned long long encodedBytes;
    unsigned long long outputBytes;
    ArchiveMetadata metadata;
    HuffmanEncoding encoding = {0};
    int useHuffman;

    /* VALIDATE SOURCE AND DESTINATION*/
    if (input == NULL || outputPath == NULL || outputPath[0] == '\0') {
        (void)fputs("Input and output paths are required.\n", stderr);
        goto closeInput;
    }

    /* MEASURE THE SOURCE*/
    /* The histogram and checksum also detect changes between the counting and encoding passes.*/
    if (!measureArchiveSource(input, &statistics)) {
        goto closeInput;
    }

    /* BUILD PREFIX CODES*/
    if (!prepareArchiveEncoding(&statistics, &encoding)) {
        goto closeInput;
    }

    /* SELECT THE COMPLETE FRAME SIZE*/
    /* Include the histogram overhead before choosing Huffman over raw storage.*/
    encodedBytes = (encoding.bitCount / (unsigned long long)BITS_PER_BYTE) +
                   (encoding.bitCount % (unsigned long long)BITS_PER_BYTE != 0 ? 1ULL : 0ULL);
    useHuffman = statistics.totalBytes > FREQUENCY_TABLE_SIZE &&
                 encodedBytes < statistics.totalBytes - (unsigned long long)FREQUENCY_TABLE_SIZE;
    outputBytes = (unsigned long long)HEADER_SIZE + statistics.totalBytes;
    if (useHuffman) {
        outputBytes = (unsigned long long)HEADER_SIZE + (unsigned long long)FREQUENCY_TABLE_SIZE +
                      encodedBytes;
    }

    /* CREATE AND WRITE THE SELECTED FRAME*/
    if (fseek(input, 0, SEEK_SET) != 0) {
        perror(inputPath);
        goto closeInput;
    }
    output = createOutputFile(outputPath);
    if (output == NULL) {
        goto closeInput;
    }

    DEBUG_LOG("input=%s bytes=%llu mode=%s\n", inputPath, statistics.totalBytes,
              useHuffman ? "huffman" : "raw");
    streams = (StreamPair){input, output};
    metadata = (ArchiveMetadata){
        useHuffman ? HUFFMAN_MODE : RAW_MODE,
        statistics.totalBytes,
        statistics.checksum,
        statistics.frequencies,
    };
    if (!writeArchiveFrame(&(ArchiveWriteContext){
            streams,
            &metadata,
            &statistics,
            &encoding,
            useHuffman,
            &metrics,
        })) {
        goto cleanup;
    }

    /* FINALISE ONLY AFTER VALIDATED ENCODING*/
    if (fclose(input) != 0) {
        input = NULL;
        perror(inputPath);
        goto cleanup;
    }
    input = NULL;
    if (!finishOutputFile(output, outputPath)) {
        return 0;
    }
    (void)printf("Compressed %llu bytes into %llu bytes (%s frame); FNV-1a 64 %016llX: %s\n",
                 statistics.totalBytes, outputBytes, useHuffman ? "Huffman" : "raw",
                 statistics.checksum, outputPath);
    return 1;

cleanup:
    (void)fputs("Compression failed while writing the output; the incomplete file was removed.\n",
                stderr);
closeInput:
    /* CLOSE OWNED HANDLES*/
    /* A failed exclusive create must never remove a pre-existing destination.*/
    if (input != NULL) {
        (void)closeInputFile(input, inputPath);
    }
    if (output != NULL) {
        discardOutputFile(output, outputPath);
    }
    return 0;
}

/**
 * Validate and restore an archive to a staged output file.
 * @param paths Archive source and new destination for the restored input.
 * @return Nonzero when the archive validates and its restored output is finalized.
 * @author kai
 */
int decompressFile(CompressionPaths paths) {
    const char *inputPath = paths.input;
    const char *outputPath = paths.output;
    FILE *input = openInputFile(inputPath);
    FILE *output = NULL;
    unsigned long long frequencies[SYMBOL_COUNT];
    ArchiveMetadata metadata = {0, 0, 0, frequencies};
    PayloadMetrics metrics = {NULL, 0};
    StreamPair streams;
    HuffmanNode nodes[NODE_CAPACITY];
    int root = -1;
    int success = 0;

    /* VALIDATE THE ARCHIVE REQUEST*/
    if (input == NULL || outputPath == NULL || outputPath[0] == '\0') {
        (void)fputs("Input and output paths are required.\n", stderr);
        goto cleanup;
    }

    /* READ THE FRAME CONTRACT*/
    if (!readHeader(input, &metadata)) {
        (void)fputs("Input is not a valid or supported TCMP v1 file.\n", stderr);
        goto cleanup;
    }
    if (metadata.mode == HUFFMAN_MODE &&
        (!buildHuffmanTree(frequencies, nodes, &root) || root < 0)) {
        (void)fputs("TCMP Huffman tree is invalid.\n", stderr);
        goto cleanup;
    }

    /* RESTORE THE DECLARED PAYLOAD*/
    output = createOutputFile(outputPath);
    if (output == NULL) {
        goto cleanup;
    }
    DEBUG_LOG("input=%s bytes=%llu mode=%s\n", inputPath, metadata.originalSize,
              metadata.mode == HUFFMAN_MODE ? "huffman" : "raw");
    streams = (StreamPair){input, output};

    success = metadata.mode == RAW_MODE ? copyPayload(streams, metadata.originalSize, &metrics)
                                        : writeDecodedHuffman(streams, metadata.originalSize,
                                                              frequencies, nodes, root, &metrics);

    /* VERIFY INTEGRITY BEFORE FINALISING*/
    if (!success || metrics.checksum != metadata.checksum) {
        (void)fputs(
            "The TCMP payload is truncated, corrupt, has trailing data, or fails its checksum.\n",
            stderr);
        goto cleanup;
    }
    if (fclose(input) != 0) {
        input = NULL;
        perror(inputPath);
        goto cleanup;
    }
    if (!finishOutputFile(output, outputPath)) {
        return 0;
    }

    (void)printf("Restored %llu bytes (FNV-1a 64 %016llX) to %s\n", metadata.originalSize,
                 metrics.checksum, outputPath);
    return 1;

cleanup:
    /* DISCARD ONLY AN OUTPUT CREATED BY THIS OPERATION*/
    if (input != NULL) {
        (void)closeInputFile(input, inputPath);
    }
    if (output != NULL) {
        discardOutputFile(output, outputPath);
    }
    return 0;
}

/*//////////////////////////////////////////////////
INSPECTION
//////////////////////////////////////////////////*/

/* HISTOGRAM SORTING*/
/**
 * Order symbols by frequency with a stable byte-value tie break.
 * @param left First symbol and frequency pair.
 * @param right Second pair used for stable ordering.
 * @return Nonzero when left ranks before right by frequency and byte value.
 * @author kai
 */
static int frequencyBefore(const SymbolFrequency *left, const SymbolFrequency *right) {
    if (left->frequency != right->frequency) {
        return left->frequency > right->frequency;
    }
    return left->symbol < right->symbol;
}

/**
 * Sort symbol records by frequency using the supplied scratch array.
 * @param values Symbol records to sort in place.
 * @param scratch Temporary array with room for the same records.
 * @param begin Inclusive first index of the range.
 * @param end Exclusive end index of the range.
 * @author kai
 */
static void mergeSortFrequencies(SymbolFrequency values[], SymbolFrequency scratch[], int begin,
                                 int end) {
    int middle;
    int left = begin;
    int right;
    int out = begin;
    if (end - begin < 2) {
        return;
    }

    /* SORT EACH HALF*/
    middle = begin + ((end - begin) / 2);
    mergeSortFrequencies(values, scratch, begin, middle);
    mergeSortFrequencies(values, scratch, middle, end);
    right = middle;

    /* MERGE AND COPY BACK*/
    /* The same scratch range serves every recursion level after its children finish.*/
    while (left < middle && right < end) {
        scratch[out++] =
            frequencyBefore(&values[left], &values[right]) ? values[left++] : values[right++];
    }
    while (left < middle) {
        scratch[out++] = values[left++];
    }
    while (right < end) {
        scratch[out++] = values[right++];
    }
    memcpy(values + begin, scratch + begin, (size_t)(end - begin) * sizeof(*values));
}

/**
 * Report format and size information for an input file.
 * @param inputPath Source file path.
 * @return Nonzero when the file is read and its statistics are reported.
 * @author kai
 */
int inspectFile(const char *inputPath) {
    FILE *input = openInputFile(inputPath);
    InputStatistics statistics;
    SymbolFrequency values[SYMBOL_COUNT];
    SymbolFrequency scratch[SYMBOL_COUNT];
    int count = 0;
    int symbol;

    /* MEASURE THE INPUT*/
    if (input == NULL) {
        return 0;
    }
    if (!scanInput(input, &statistics)) {
        (void)fputs("Could not read the complete input.\n", stderr);
        (void)closeInputFile(input, inputPath);
        return 0;
    }
    if (fclose(input) != 0) {
        perror(inputPath);
        return 0;
    }

    /* SORT OCCUPIED HISTOGRAM ENTRIES*/
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        if (statistics.frequencies[symbol] > 0) {
            values[count++] = (SymbolFrequency){symbol, statistics.frequencies[symbol]};
        }
    }
    mergeSortFrequencies(values, scratch, 0, count);
    DEBUG_LOG("inspect=%s bytes=%llu symbols=%d\n", inputPath, statistics.totalBytes, count);
    (void)printf(
        "File: %s\nSize: %llu bytes\nDistinct byte values: %d\nFNV-1a 64 checksum: %016llX\n",
        inputPath, statistics.totalBytes, count, statistics.checksum);

    /* REPORT THE LEADING SYMBOLS*/
    (void)puts("Most frequent bytes:");
    for (symbol = 0; symbol < count && symbol < MOST_FREQUENT_COUNT; symbol++) {
        int byte = values[symbol].symbol;
        if (byte >= TEXT_SPACE && byte <= TEXT_TILDE) {
            (void)printf("  0x%02X '%c'  %llu\n", (unsigned int)byte, byte,
                         values[symbol].frequency);
        } else {
            (void)printf("  0x%02X      %llu\n", (unsigned int)byte, values[symbol].frequency);
        }
    }
    return 1;
}

/* BYTE SEARCH*/
/**
 * Build the KMP failure table for a byte pattern.
 * @param pattern Search bytes.
 * @param patternLength Number of pattern bytes.
 * @param prefix Receives the longest proper prefix ending at each pattern position.
 * @author kai
 */
static void buildSearchPrefix(const char *pattern, size_t patternLength, size_t prefix[]) {
    size_t prefixIndex;
    prefix[0] = 0;
    for (prefixIndex = 1; prefixIndex < patternLength; prefixIndex++) {
        size_t candidate = prefix[prefixIndex - 1];
        while (candidate > 0 && pattern[prefixIndex] != pattern[candidate]) {
            candidate = prefix[candidate - 1];
        }
        if (pattern[prefixIndex] == pattern[candidate]) {
            candidate++;
        }
        prefix[prefixIndex] = candidate;
    }
}

/**
 * Advance KMP matching state by one input byte.
 * @param matched Previously matched prefix length.
 * @param nextByte Input byte.
 * @param pattern Search bytes.
 * @param prefix KMP failure table.
 * @return New matched prefix length.
 * @author kai
 */
static size_t advanceSearch(size_t matched, int nextByte, const char *pattern,
                            const size_t prefix[]) {
    while (matched > 0 && (unsigned char)pattern[matched] != (unsigned char)nextByte) {
        matched = prefix[matched - 1];
    }
    if ((unsigned char)pattern[matched] == (unsigned char)nextByte) {
        matched++;
    }
    return matched;
}

/**
 * Search a file for a literal byte pattern and report matching offsets.
 * @param inputPath Source file to scan.
 * @param pattern Non-empty byte sequence to find.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int searchFile(const char *inputPath, const char *pattern) {
    FILE *input;
    size_t patternLength;
    size_t *prefix;
    size_t matched = 0;
    unsigned long long offset = 0;
    unsigned long long matches = 0;
    int nextByte;

    /* VALIDATE THE SEARCH REQUEST*/
    if (pattern == NULL || pattern[0] == '\0') {
        (void)fputs("Search text cannot be empty.\n", stderr);
        return 0;
    }
    input = openInputFile(inputPath);
    if (input == NULL) {
        return 0;
    }
    patternLength = strlen(pattern);
    if (patternLength > (size_t)-1 / sizeof(*prefix)) {
        (void)fputs("Search text is too long.\n", stderr);
        (void)closeInputFile(input, inputPath);
        return 0;
    }
    prefix = malloc(patternLength * sizeof(*prefix));
    if (prefix == NULL) {
        (void)fputs("Could not allocate the search index.\n", stderr);
        (void)closeInputFile(input, inputPath);
        return 0;
    }

    /* BUILD THE KMP FAILURE TABLE*/
    /* Fallback lengths preserve earlier partial matches without rescanning input bytes.*/
    buildSearchPrefix(pattern, patternLength, prefix);

    /* SCAN WITHOUT BACKTRACKING THE INPUT*/
    while ((nextByte = fgetc(input)) != EOF) {
        matched = advanceSearch(matched, nextByte, pattern, prefix);
        if (matched == patternLength) {
            if (matches < REPORT_LIMIT) {
                (void)printf("Match at byte offset %llu\n",
                             offset + 1 - (unsigned long long)patternLength);
            }
            matches++;
            matched = prefix[matched - 1];
        }
        offset++;
    }
    free(prefix);
    if (ferror(input)) {
        perror(inputPath);
        (void)closeInputFile(input, inputPath);
        return 0;
    }
    if (fclose(input) != 0) {
        perror(inputPath);
        return 0;
    }

    /* REPORT THE COMPLETE MATCH COUNT*/
    /* Only the first twenty positions are printed, but every match contributes to the total.*/
    if (matches == 0) {
        (void)printf("No match for \"%s\" in %s.\n", pattern, inputPath);
    } else {
        (void)printf("Found %llu match(es) in %s%s.\n", matches, inputPath,
                     matches > REPORT_LIMIT ? " (first 20 positions shown)" : "");
    }
    DEBUG_LOG("search=%s patternBytes=%lu matches=%llu\n", inputPath, (unsigned long)patternLength,
              matches);
    return 1;
}
