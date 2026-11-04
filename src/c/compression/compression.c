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
#define FREQUENCY_TABLE_SIZE (SYMBOL_COUNT * 8)
#define ULL_MAX_VALUE (~0ULL)
#define FNV1A_OFFSET_BASIS 14695981039346656037ULL
#define FNV1A_PRIME 1099511628211ULL

/* FILE TRANSACTIONS*/
/**
 * Open a source path for binary reading and report open failures.
 * @param path Existing source file to open.
 * @return Open binary input stream, or NULL if the path cannot be opened.
 * @author kai
 */
static FILE *openInputFile(const char *path) {
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file && path)
        perror(path);
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
    if (!file && path)
        perror(path);
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
    if (fclose(file) == 0)
        return 1;
    perror(path);
    remove(path);
    return 0;
}

/**
 * Close an output stream and remove its incomplete destination.
 * @param file Open output stream to close.
 * @param path Incomplete destination path to remove.
 * @author kai
 */
static void discardOutputFile(FILE *file, const char *path) {
    fclose(file);
    remove(path);
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
    if (nodes[left].frequency != nodes[right].frequency)
        return nodes[left].frequency < nodes[right].frequency;
    if (nodes[left].minimumSymbol != nodes[right].minimumSymbol)
        return nodes[left].minimumSymbol < nodes[right].minimumSymbol;
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
    int *at = &queue->head;
    while (*at >= 0 && nodeLess(nodes, *at, node))
        at = &queue->next[*at];
    queue->next[node] = *at;
    *at = node;
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
    int symbol, nodeCount = 0;

    /* SEED THE SYMBOL CANDIDATES*/
    /* Only symbols present in the input receive leaves.*/
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        if (frequencies[symbol] == 0)
            continue;
        nodes[nodeCount] = (HuffmanNode){frequencies[symbol], symbol, -1, -1, symbol};
        queueInsert(&queue, nodes, nodeCount++);
    }

    /* MERGE THE TWO LOWEST FREQUENCIES*/
    /* Stable symbol tie-breaking makes the resulting prefix codes deterministic.*/
    while (queue.head >= 0 && queue.next[queue.head] >= 0) {
        int left = takeMinimum(&queue);
        int right = takeMinimum(&queue);
        int parent = nodeCount++;

        if (nodes[left].frequency > ULL_MAX_VALUE - nodes[right].frequency)
            return 0;
        nodes[parent] = (HuffmanNode){
            nodes[left].frequency + nodes[right].frequency, -1, left, right,
            nodes[left].minimumSymbol < nodes[right].minimumSymbol ? nodes[left].minimumSymbol
                                                                   : nodes[right].minimumSymbol};
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
 * @param frequencies Receives the count for each byte value.
 * @param totalBytes Receives the total number of input bytes.
 * @param checksum Receives the checksum of the scanned bytes.
 * @return Nonzero when the whole stream is counted and rewound; zero on read, seek, or size error.
 * @author kai
 */
static int scanInput(FILE *input, unsigned long long frequencies[], unsigned long long *totalBytes,
                     unsigned long long *checksum) {
    unsigned char buffer[BUFFER_SIZE];
    size_t count;
    int i;

    memset(frequencies, 0, sizeof(unsigned long long) * SYMBOL_COUNT);
    *totalBytes = 0;
    *checksum = FNV1A_OFFSET_BASIS;
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if ((unsigned long long)count > ULL_MAX_VALUE - *totalBytes)
            return 0;
        *totalBytes += (unsigned long long)count;
        for (i = 0; (size_t)i < count; i++) {
            unsigned int symbol = buffer[i];
            /* Each symbol count is bounded by the checked total above.*/
            frequencies[symbol]++;
            *checksum ^= (unsigned long long)buffer[i];
            *checksum *= FNV1A_PRIME;
        }
    }
    return !ferror(input);
}

/**
 * Detect PNG, JPEG, or PDF input from its leading signature bytes.
 * @param input Seekable stream whose leading bytes identify its format.
 * @return 1 for PNG/JPEG/PDF, 0 for other bytes, or -1 if reading or rewind fails.
 * @author kai
 */
static int isMediaInput(FILE *input) {
    unsigned char signature[8];
    size_t count = fread(signature, 1, sizeof(signature), input);
    if (ferror(input) || fseek(input, 0, SEEK_SET) != 0)
        return -1;
    return (count >= 5 && !memcmp(signature, "%PDF-", 5)) ||
           (count >= 8 && !memcmp(signature, "\211PNG\r\n\032\n", 8)) ||
           (count >= 2 && signature[0] == 255 && signature[1] == 216);
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
        if (length != 0 && frequencies[symbol] > ULL_MAX_VALUE / length)
            return 0;
        contribution = frequencies[symbol] * length;
        if (contribution > ULL_MAX_VALUE - *bitCount)
            return 0;
        *bitCount += contribution;
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
    unsigned char bytes[8];
    int i;
    for (i = 7; i >= 0; i--) {
        bytes[i] = (unsigned char)(value & 255ULL);
        value >>= 8;
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
    unsigned char bytes[8];
    int i;
    if (fread(bytes, 1, sizeof(bytes), stream) != sizeof(bytes))
        return 0;
    *value = 0;
    for (i = 0; i < 8; i++)
        *value = (*value << 8) | bytes[i];
    return 1;
}

/**
 * Write the archive magic, mode, original size, and mode-specific metadata.
 * @param output Destination archive stream.
 * @param mode Archive encoding mode recorded in the header.
 * @param originalSize Uncompressed byte count recorded for restoration.
 * @param checksum Checksum of the original input bytes.
 * @param frequencies Symbol counts required by Huffman mode.
 * @return Nonzero when all required header fields are written.
 * @author kai
 */
static int writeHeader(FILE *output, int mode, unsigned long long originalSize,
                       unsigned long long checksum, const unsigned long long frequencies[]) {
    unsigned char prefix[6] = {'T', 'C', 'M', 'P', 1, (unsigned char)mode};
    int symbol;

    if (fwrite(prefix, 1, sizeof(prefix), output) != sizeof(prefix) ||
        !writeUnsignedLongLong(output, originalSize) || !writeUnsignedLongLong(output, checksum))
        return 0;
    if (mode == 1) {
        for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
            if (!writeUnsignedLongLong(output, frequencies[symbol]))
                return 0;
        }
    }
    return 1;
}

/**
 * Validate and parse an archive header before payload decoding.
 * @param input Archive stream positioned at its first byte.
 * @param mode Receives the validated encoding mode.
 * @param originalSize Receives the expected restored byte count.
 * @param checksum Receives the expected original-data checksum.
 * @param frequencies Receives the Huffman symbol counts when present.
 * @return Nonzero when the header fields and frequency total validate.
 * @author kai
 */
static int readHeader(FILE *input, int *mode, unsigned long long *originalSize,
                      unsigned long long *checksum, unsigned long long frequencies[]) {
    unsigned char prefix[6];
    int symbol;

    if (fread(prefix, 1, sizeof(prefix), input) != sizeof(prefix) || memcmp(prefix, "TCMP\1", 5) ||
        prefix[5] > 1 || !readUnsignedLongLong(input, originalSize) ||
        !readUnsignedLongLong(input, checksum))
        return 0;

    *mode = prefix[5];
    memset(frequencies, 0, sizeof(unsigned long long) * SYMBOL_COUNT);
    if (*mode == 1) {
        unsigned long long total = 0;
        for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
            if (!readUnsignedLongLong(input, &frequencies[symbol]) ||
                frequencies[symbol] > ULL_MAX_VALUE - total)
                return 0;
            total += frequencies[symbol];
        }
        if (total != *originalSize)
            return 0;
    }
    return 1;
}

/*//////////////////////////////////////////////////
PAYLOADS
//////////////////////////////////////////////////*/

/* ENCODING*/
/**
 * Copy exactly the declared payload length and report short reads or writes.
 * @param input Source stream positioned at the payload.
 * @param output Destination stream for copied bytes.
 * @param size Exact number of payload bytes to transfer.
 * @param observed Optional histogram for the encoder's second-pass validation.
 * @param checksum Receives or verifies the payload checksum.
 * @return Nonzero when the declared bytes copy successfully and no trailing bytes remain.
 * @author kai
 */
static int copyPayload(FILE *input, FILE *output, unsigned long long size,
                       unsigned long long observed[], unsigned long long *checksum) {
    unsigned char buffer[BUFFER_SIZE];
    size_t i;
    if (observed)
        memset(observed, 0, sizeof(unsigned long long) * SYMBOL_COUNT);
    *checksum = FNV1A_OFFSET_BASIS;

    /* COPY AND MEASURE THE DECLARED PAYLOAD*/
    /* Both directions share length/checksum checks; encoding also rebuilds symbol counts.*/
    while (size) {
        size_t count = size < sizeof(buffer) ? (size_t)size : sizeof(buffer);
        if (fread(buffer, 1, count, input) != count || fwrite(buffer, 1, count, output) != count)
            return 0;
        for (i = 0; i < count; i++) {
            if (observed)
                observed[buffer[i]]++;
            *checksum ^= buffer[i];
            *checksum *= FNV1A_PRIME;
        }
        size -= count;
    }
    return fgetc(input) == EOF && !ferror(input);
}

/**
 * Encode input symbols with the supplied Huffman codes and pad the final byte.
 * @param input Source stream positioned at the bytes to encode.
 * @param output Destination archive stream.
 * @param codes Bit pattern assigned to each byte value.
 * @param codeLengths Number of significant bits in each code.
 * @param observed Optional counts used to verify the second input pass.
 * @param checksum Receives the checksum of encoded source bytes.
 * @return Nonzero when the full payload encodes and writes successfully.
 * @author kai
 */
static int writeHuffmanPayload(FILE *input, FILE *output, const unsigned char codes[][SYMBOL_COUNT],
                               const unsigned short codeLengths[], unsigned long long observed[],
                               unsigned long long *checksum) {
    unsigned char buffer[BUFFER_SIZE];
    unsigned char outByte = 0;
    int usedBits = 0;
    size_t count;
    size_t i;

    memset(observed, 0, sizeof(unsigned long long) * SYMBOL_COUNT);
    *checksum = FNV1A_OFFSET_BASIS;

    /* ENCODE EACH INPUT SYMBOL*/
    /* The second scan rebuilds counts and checksum while packing the selected prefix codes.*/
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        for (i = 0; i < count; i++) {
            unsigned int symbol = buffer[i];
            unsigned int bit;
            observed[symbol]++;
            *checksum ^= (unsigned long long)symbol;
            *checksum *= FNV1A_PRIME;
            for (bit = 0; bit < codeLengths[symbol]; bit++) {
                outByte = (unsigned char)((outByte << 1) | codes[symbol][bit]);
                usedBits++;
                if (usedBits == 8) {
                    if (fputc(outByte, output) == EOF)
                        return 0;
                    outByte = 0;
                    usedBits = 0;
                }
            }
        }
    }
    if (ferror(input))
        return 0;

    /* PAD THE LAST PAYLOAD BYTE*/
    /* Unused low bits stay zero so the decoder can verify the end of the frame.*/
    if (usedBits > 0) {
        outByte = (unsigned char)(outByte << (8 - usedBits));
        if (fputc(outByte, output) == EOF)
            return 0;
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
        if (next == EOF)
            return 0;
        reader->byte = (unsigned char)next;
        reader->remaining = 8;
    }
    reader->remaining--;
    *bit = (reader->byte >> reader->remaining) & 1;
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
        unsigned int paddingMask = (1u << reader->remaining) - 1u;
        if (((unsigned int)reader->byte & paddingMask) != 0)
            return 0;
    }
    next = fgetc(input);
    return next == EOF && !ferror(input);
}

/**
 * Decode exactly the original byte count and write each recovered symbol.
 * @param input Encoded payload stream.
 * @param output Destination for restored bytes.
 * @param originalSize Exact number of bytes to decode.
 * @param frequencies Expected counts used to reject invalid symbols.
 * @param nodes Validated Huffman tree nodes.
 * @param root Root index of the decoding tree.
 * @param checksum Receives the checksum of restored bytes.
 * @return Nonzero when all symbols, frequencies, padding, and output writes validate.
 * @author kai
 */
static int writeDecodedHuffman(FILE *input, FILE *output, unsigned long long originalSize,
                               const unsigned long long frequencies[], const HuffmanNode nodes[],
                               int root, unsigned long long *checksum) {
    BitReader reader = {0, 0};
    unsigned long long decoded[SYMBOL_COUNT] = {0};
    unsigned long long written;
    *checksum = FNV1A_OFFSET_BASIS;

    for (written = 0; written < originalSize; written++) {
        int nodeIndex = root;
        int bit;
        if (nodes[nodeIndex].symbol >= 0 && (!bitReaderGet(input, &reader, &bit) || bit != 0))
            return 0;
        while (nodes[nodeIndex].symbol < 0) {
            if (!bitReaderGet(input, &reader, &bit))
                return 0;
            nodeIndex = bit == 0 ? nodes[nodeIndex].left : nodes[nodeIndex].right;
        }
        if (nodes[nodeIndex].symbol < 0 || nodes[nodeIndex].symbol >= SYMBOL_COUNT)
            return 0;
        if (decoded[nodes[nodeIndex].symbol] >= frequencies[nodes[nodeIndex].symbol])
            return 0;
        decoded[nodes[nodeIndex].symbol]++;
        *checksum ^= (unsigned long long)nodes[nodeIndex].symbol;
        *checksum *= FNV1A_PRIME;
        if (fputc(nodes[nodeIndex].symbol, output) == EOF)
            return 0;
    }
    if (!sameFrequencies(decoded, frequencies))
        return 0;
    return finishBitReader(input, &reader);
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
    MediaOptions options = {0, 0, 16, 0.5, NULL};
    int media;
    if (!input)
        return 0;
    media = isMediaInput(input);
    if (fclose(input) || media < 0)
        return 0;
    return media ? processMedia(inputPath, outputPath, &options)
                 : archiveFile(inputPath, outputPath);
}

/* ARCHIVE OPERATIONS*/
/**
 * Encode a supported media file and its vector exports as one archive.
 * @param inputPath Source file path.
 * @param outputPath New archive destination; an existing file is preserved.
 * @return Nonzero when the archive is written and verified against a second source pass.
 * @author kai
 */
int archiveFile(const char *inputPath, const char *outputPath) {
    FILE *input = openInputFile(inputPath);
    FILE *output = NULL;
    unsigned long long frequencies[SYMBOL_COUNT];
    unsigned long long observed[SYMBOL_COUNT];
    unsigned long long totalBytes;
    unsigned long long checksum;
    unsigned long long observedChecksum;
    unsigned long long bitCount;
    unsigned long long encodedBytes;
    unsigned long long outputBytes;
    HuffmanNode nodes[NODE_CAPACITY];
    unsigned char codes[SYMBOL_COUNT][SYMBOL_COUNT] = {{0}};
    unsigned char codePath[SYMBOL_COUNT];
    unsigned short codeLengths[SYMBOL_COUNT] = {0};
    int root = -1;
    int useHuffman;

    /* VALIDATE SOURCE AND DESTINATION*/
    if (input == NULL || outputPath == NULL || outputPath[0] == '\0') {
        fputs("Input and output paths are required.\n", stderr);
        goto closeInput;
    }

    /* MEASURE THE SOURCE*/
    /* The histogram and checksum also detect changes between the counting and encoding passes.*/
    if (!scanInput(input, frequencies, &totalBytes, &checksum)) {
        fputs("Could not read the complete input or its size exceeded the supported range.\n",
              stderr);
        goto closeInput;
    }
    if (totalBytes > ULL_MAX_VALUE - HEADER_SIZE - FREQUENCY_TABLE_SIZE) {
        fputs("Input is too large for the TCMP v1 size fields.\n", stderr);
        goto closeInput;
    }

    /* BUILD PREFIX CODES*/
    if (!buildHuffmanTree(frequencies, nodes, &root)) {
        fputs("Input is too large to represent in the TCMP format.\n", stderr);
        goto closeInput;
    }
    if (root >= 0)
        assignCodes(nodes, root, codePath, 0, codes, codeLengths);
    if (!calculateEncodedBits(frequencies, codeLengths, &bitCount)) {
        fputs("Encoded size exceeds the supported range.\n", stderr);
        goto closeInput;
    }

    /* SELECT THE COMPLETE FRAME SIZE*/
    /* Include the histogram overhead before choosing Huffman over raw storage.*/
    encodedBytes = bitCount / 8ULL + (bitCount % 8ULL != 0 ? 1ULL : 0ULL);
    useHuffman =
        totalBytes > FREQUENCY_TABLE_SIZE && encodedBytes < totalBytes - FREQUENCY_TABLE_SIZE;
    outputBytes = HEADER_SIZE + totalBytes;
    if (useHuffman)
        outputBytes = HEADER_SIZE + FREQUENCY_TABLE_SIZE + encodedBytes;

    /* CREATE AND WRITE THE SELECTED FRAME*/
    if (fseek(input, 0, SEEK_SET) != 0) {
        perror(inputPath);
        goto closeInput;
    }
    output = createOutputFile(outputPath);
    if (output == NULL)
        goto closeInput;

    DEBUG_LOG("input=%s bytes=%llu mode=%s\n", inputPath, totalBytes,
              useHuffman ? "huffman" : "raw");
    if (!writeHeader(output, useHuffman ? 1 : 0, totalBytes, checksum, frequencies))
        goto cleanup;
    if (!(useHuffman
              ? writeHuffmanPayload(input, output, codes, codeLengths, observed, &observedChecksum)
              : copyPayload(input, output, totalBytes, observed, &observedChecksum)) ||
        !sameFrequencies(frequencies, observed) || observedChecksum != checksum)
        goto cleanup;

    /* FINALISE ONLY AFTER VALIDATED ENCODING*/
    if (fclose(input) != 0) {
        input = NULL;
        perror(inputPath);
        goto cleanup;
    }
    input = NULL;
    if (!finishOutputFile(output, outputPath))
        return 0;
    printf("Compressed %llu bytes into %llu bytes (%s frame); FNV-1a 64 %016llX: %s\n", totalBytes,
           outputBytes, useHuffman ? "Huffman" : "raw", checksum, outputPath);
    return 1;

cleanup:
    fputs("Compression failed while writing the output; the incomplete file was removed.\n",
          stderr);
closeInput:
    /* CLOSE OWNED HANDLES*/
    /* A failed exclusive create must never remove a pre-existing destination.*/
    if (input != NULL)
        fclose(input);
    if (output != NULL)
        discardOutputFile(output, outputPath);
    return 0;
}

/**
 * Validate and restore an archive to a staged output file.
 * @param inputPath Source file path.
 * @param outputPath New destination for the restored input.
 * @return Nonzero when the archive validates and its restored output is finalized.
 * @author kai
 */
int decompressFile(const char *inputPath, const char *outputPath) {
    FILE *input = openInputFile(inputPath);
    FILE *output = NULL;
    unsigned long long frequencies[SYMBOL_COUNT];
    unsigned long long originalSize;
    unsigned long long expectedChecksum;
    unsigned long long actualChecksum;
    HuffmanNode nodes[NODE_CAPACITY];
    int root;
    int mode;
    int success = 0;

    /* VALIDATE THE ARCHIVE REQUEST*/
    if (input == NULL || outputPath == NULL || outputPath[0] == '\0') {
        fputs("Input and output paths are required.\n", stderr);
        goto cleanup;
    }

    /* READ THE FRAME CONTRACT*/
    if (!readHeader(input, &mode, &originalSize, &expectedChecksum, frequencies)) {
        fputs("Input is not a valid or supported TCMP v1 file.\n", stderr);
        goto cleanup;
    }
    if (mode == 1 && (!buildHuffmanTree(frequencies, nodes, &root) || root < 0)) {
        fputs("TCMP Huffman tree is invalid.\n", stderr);
        goto cleanup;
    }

    /* RESTORE THE DECLARED PAYLOAD*/
    output = createOutputFile(outputPath);
    if (output == NULL)
        goto cleanup;
    DEBUG_LOG("input=%s bytes=%llu mode=%s\n", inputPath, originalSize,
              mode == 1 ? "huffman" : "raw");

    success = mode == 0 ? copyPayload(input, output, originalSize, NULL, &actualChecksum)
                        : writeDecodedHuffman(input, output, originalSize, frequencies, nodes, root,
                                              &actualChecksum);

    /* VERIFY INTEGRITY BEFORE FINALISING*/
    if (!success || actualChecksum != expectedChecksum) {
        fputs("The TCMP payload is truncated, corrupt, has trailing data, or fails its checksum.\n",
              stderr);
        goto cleanup;
    }
    if (fclose(input) != 0) {
        input = NULL;
        perror(inputPath);
        goto cleanup;
    }
    if (!finishOutputFile(output, outputPath))
        return 0;

    printf("Restored %llu bytes (FNV-1a 64 %016llX) to %s\n", originalSize, actualChecksum,
           outputPath);
    return 1;

cleanup:
    /* DISCARD ONLY AN OUTPUT CREATED BY THIS OPERATION*/
    if (input != NULL)
        fclose(input);
    if (output != NULL)
        discardOutputFile(output, outputPath);
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
    if (left->frequency != right->frequency)
        return left->frequency > right->frequency;
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
    int middle, left = begin, right, out = begin;
    if (end - begin < 2)
        return;

    /* SORT EACH HALF*/
    middle = begin + (end - begin) / 2;
    mergeSortFrequencies(values, scratch, begin, middle);
    mergeSortFrequencies(values, scratch, middle, end);
    right = middle;

    /* MERGE AND COPY BACK*/
    /* The same scratch range serves every recursion level after its children finish.*/
    while (left < middle && right < end)
        scratch[out++] =
            frequencyBefore(&values[left], &values[right]) ? values[left++] : values[right++];
    while (left < middle)
        scratch[out++] = values[left++];
    while (right < end)
        scratch[out++] = values[right++];
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
    unsigned long long frequencies[SYMBOL_COUNT];
    unsigned long long totalBytes;
    unsigned long long checksum;
    SymbolFrequency values[SYMBOL_COUNT];
    SymbolFrequency scratch[SYMBOL_COUNT];
    int count = 0;
    int symbol;

    /* MEASURE THE INPUT*/
    if (input == NULL)
        return 0;
    if (!scanInput(input, frequencies, &totalBytes, &checksum)) {
        fputs("Could not read the complete input.\n", stderr);
        fclose(input);
        return 0;
    }
    if (fclose(input) != 0) {
        perror(inputPath);
        return 0;
    }

    /* SORT OCCUPIED HISTOGRAM ENTRIES*/
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        if (frequencies[symbol] > 0) {
            values[count++] = (SymbolFrequency){symbol, frequencies[symbol]};
        }
    }
    mergeSortFrequencies(values, scratch, 0, count);
    DEBUG_LOG("inspect=%s bytes=%llu symbols=%d\n", inputPath, totalBytes, count);
    printf("File: %s\nSize: %llu bytes\nDistinct byte values: %d\nFNV-1a 64 checksum: %016llX\n",
           inputPath, totalBytes, count, checksum);

    /* REPORT THE LEADING SYMBOLS*/
    puts("Most frequent bytes:");
    for (symbol = 0; symbol < count && symbol < 10; symbol++) {
        int byte = values[symbol].symbol;
        if (byte >= 32 && byte <= 126)
            printf("  0x%02X '%c'  %llu\n", (unsigned int)byte, byte, values[symbol].frequency);
        else
            printf("  0x%02X      %llu\n", (unsigned int)byte, values[symbol].frequency);
    }
    return 1;
}

/* BYTE SEARCH*/
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
    size_t i;

    /* VALIDATE THE SEARCH REQUEST*/
    if (pattern == NULL || pattern[0] == '\0') {
        fputs("Search text cannot be empty.\n", stderr);
        return 0;
    }
    input = openInputFile(inputPath);
    if (input == NULL)
        return 0;
    patternLength = strlen(pattern);
    if (patternLength > (size_t)-1 / sizeof(*prefix)) {
        fputs("Search text is too long.\n", stderr);
        fclose(input);
        return 0;
    }
    prefix = malloc(patternLength * sizeof(*prefix));
    if (prefix == NULL) {
        fputs("Could not allocate the search index.\n", stderr);
        fclose(input);
        return 0;
    }

    /* BUILD THE KMP FAILURE TABLE*/
    /* Fallback lengths preserve earlier partial matches without rescanning input bytes.*/
    prefix[0] = 0;
    for (i = 1; i < patternLength; i++) {
        size_t candidate = prefix[i - 1];
        while (candidate > 0 && pattern[i] != pattern[candidate])
            candidate = prefix[candidate - 1];
        if (pattern[i] == pattern[candidate])
            candidate++;
        prefix[i] = candidate;
    }

    /* SCAN WITHOUT BACKTRACKING THE INPUT*/
    while ((nextByte = fgetc(input)) != EOF) {
        while (matched > 0 && (unsigned char)pattern[matched] != (unsigned char)nextByte)
            matched = prefix[matched - 1];
        if ((unsigned char)pattern[matched] == (unsigned char)nextByte)
            matched++;
        if (matched == patternLength) {
            if (matches < 20)
                printf("Match at byte offset %llu\n",
                       offset + 1 - (unsigned long long)patternLength);
            matches++;
            matched = prefix[matched - 1];
        }
        offset++;
    }
    free(prefix);
    if (ferror(input)) {
        perror(inputPath);
        fclose(input);
        return 0;
    }
    if (fclose(input) != 0) {
        perror(inputPath);
        return 0;
    }

    /* REPORT THE COMPLETE MATCH COUNT*/
    /* Only the first twenty positions are printed, but every match contributes to the total.*/
    if (matches == 0)
        printf("No match for \"%s\" in %s.\n", pattern, inputPath);
    else
        printf("Found %llu match(es) in %s%s.\n", matches, inputPath,
               matches > 20 ? " (first 20 positions shown)" : "");
    DEBUG_LOG("search=%s patternBytes=%lu matches=%llu\n", inputPath, (unsigned long)patternLength,
              matches);
    return 1;
}
