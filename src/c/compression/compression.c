/**
 * @file compression.c
 * @brief Streaming Huffman codec and byte-oriented inspection helpers.
 *
 * TCMP v1 uses a 22-byte header. Raw frames store the original bytes directly;
 * Huffman frames add 256 big-endian 64-bit symbol counts before the bitstream.
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// STANDARD LIBRARIES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// FILE AND MEDIA CONTRACTS
#include "../fileIO/fileInput/fileInput.h"
#include "../fileIO/fileOutput/fileOutput.h"
#include "compression.h"
#include "media.h"

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

// LIMITS AND STATE

#define SYMBOL_COUNT 256
#define NODE_CAPACITY 511
#define BUFFER_SIZE 8192
#define HEADER_SIZE 22
#define FREQUENCY_TABLE_SIZE (SYMBOL_COUNT * 8)
#define ULL_MAX_VALUE (~0ULL)
#define FNV1A_OFFSET_BASIS 14695981039346656037ULL
#define FNV1A_PRIME 1099511628211ULL

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
    int items[SYMBOL_COUNT];
    int length;
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

// TREE
/** Order Huffman nodes by frequency with deterministic symbol tie-breaking. @author kai */
static int nodeLess(const HuffmanNode nodes[], int left, int right) {
    if (nodes[left].frequency != nodes[right].frequency)
        return nodes[left].frequency < nodes[right].frequency;
    if (nodes[left].minimumSymbol != nodes[right].minimumSymbol)
        return nodes[left].minimumSymbol < nodes[right].minimumSymbol;
    return left < right;
}

/** Remove the least node from the bounded candidate set. @author kai */
static int takeMinimum(NodeQueue *queue, const HuffmanNode nodes[]) {
    int i, minimum = 0, result;
    // SELECT AND REMOVE
    // At most 256 candidates make a linear scan simpler than maintaining a heap.
    for (i = 1; i < queue->length; i++)
        if (nodeLess(nodes, queue->items[i], queue->items[minimum]))
            minimum = i;
    result = queue->items[minimum];
    queue->items[minimum] = queue->items[--queue->length];
    return result;
}

/** Build a bounded Huffman tree from the symbol histogram. @author kai */
static int buildHuffmanTree(const unsigned long long frequencies[], HuffmanNode nodes[],
                            int *root) {
    NodeQueue queue = {{0}, 0};
    int symbol, nodeCount = 0;

    // SEED THE SYMBOL CANDIDATES
    // Only symbols present in the input receive leaves.
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        if (frequencies[symbol] == 0)
            continue;
        nodes[nodeCount] = (HuffmanNode){frequencies[symbol], symbol, -1, -1, symbol};
        queue.items[queue.length++] = nodeCount++;
    }

    // MERGE THE TWO LOWEST FREQUENCIES
    // Stable symbol tie-breaking makes the resulting prefix codes deterministic.
    while (queue.length > 1) {
        int left = takeMinimum(&queue, nodes);
        int right = takeMinimum(&queue, nodes);
        int parent = nodeCount++;

        if (nodes[left].frequency > ULL_MAX_VALUE - nodes[right].frequency)
            return 0;
        nodes[parent] = (HuffmanNode){
            nodes[left].frequency + nodes[right].frequency, -1, left, right,
            nodes[left].minimumSymbol < nodes[right].minimumSymbol ? nodes[left].minimumSymbol
                                                                   : nodes[right].minimumSymbol};
        queue.items[queue.length++] = parent;
    }
    *root = queue.length ? queue.items[0] : -1;
    return 1;
}

// PREFIX CODES
/** Walk the Huffman tree to assign prefix bits to each symbol. @author kai */
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

// SOURCE STATISTICS
/** Count input symbols and calculate its byte count and checksum. @author kai */
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
            // Each symbol count is bounded by the checked total above.
            frequencies[symbol]++;
            *checksum ^= (unsigned long long)buffer[i];
            *checksum *= FNV1A_PRIME;
        }
    }
    return !ferror(input);
}

/** Detect PDF, PNG, and JPEG signatures without consuming the input. @author kai */
static int isMediaInput(FILE *input) {
    unsigned char signature[8];
    size_t count = fread(signature, 1, sizeof(signature), input);
    if (ferror(input) || fseek(input, 0, SEEK_SET) != 0)
        return -1;
    return (count >= 5 && !memcmp(signature, "%PDF-", 5)) ||
           (count >= 8 && !memcmp(signature, "\211PNG\r\n\032\n", 8)) ||
           (count >= 2 && signature[0] == 255 && signature[1] == 216);
}

/** Calculate the complete Huffman payload length with overflow checking. @author kai */
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

// HEADER FIELDS
/** Write an unsigned container field in big-endian order. @author kai */
static int writeUnsignedLongLong(FILE *stream, unsigned long long value) {
    unsigned char bytes[8];
    int i;
    for (i = 7; i >= 0; i--) {
        bytes[i] = (unsigned char)(value & 255ULL);
        value >>= 8;
    }
    return fwrite(bytes, 1, sizeof(bytes), stream) == sizeof(bytes);
}

/** Read a complete big-endian container field. @author kai */
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

/** Write the TCMP method, original length, checksum, and optional histogram. @author kai */
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

/** Validate a TCMP header and its declared symbol frequencies. @author kai */
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

// ENCODING
/** Copy exactly the declared bytes and reject truncation or trailing data.
 * @param observed Optional histogram for the encoder's second-pass validation.
 * @author kai
 */
static int copyPayload(FILE *input, FILE *output, unsigned long long size,
                       unsigned long long observed[], unsigned long long *checksum) {
    unsigned char buffer[BUFFER_SIZE];
    size_t i;
    if (observed)
        memset(observed, 0, sizeof(unsigned long long) * SYMBOL_COUNT);
    *checksum = FNV1A_OFFSET_BASIS;

    // COPY AND MEASURE THE DECLARED PAYLOAD
    // Both directions share length/checksum checks; encoding also rebuilds symbol counts.
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

/** Encode symbols and verify the second input scan matches the first. @author kai */
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

    // ENCODE EACH INPUT SYMBOL
    // The second scan rebuilds counts and checksum while packing the selected prefix codes.
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

    // PAD THE LAST PAYLOAD BYTE
    // Unused low bits stay zero so the decoder can verify the end of the frame.
    if (usedBits > 0) {
        outByte = (unsigned char)(outByte << (8 - usedBits));
        if (fputc(outByte, output) == EOF)
            return 0;
    }
    return 1;
}

// VALIDATION
/** Compare complete symbol histograms. @author kai */
static int sameFrequencies(const unsigned long long first[], const unsigned long long second[]) {
    return memcmp(first, second, sizeof(unsigned long long) * SYMBOL_COUNT) == 0;
}

/** Read one high-order Huffman payload bit. @author kai */
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

/** Reject nonzero padding and trailing payload bytes. @author kai */
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

/** Restore a Huffman payload and validate counts, checksum, and padding. @author kai */
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

// FORMAT ROUTING
/** Dispatch recognised media losslessly and archive other input. @author kai */
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

// ARCHIVE OPERATIONS
/** Write a new TCMP archive, selecting Huffman only when its frame is smaller. @author kai */
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
    int root;
    int useHuffman;

    // VALIDATE SOURCE AND DESTINATION
    if (input == NULL || outputPath == NULL || outputPath[0] == '\0') {
        fputs("Input and output paths are required.\n", stderr);
        goto closeInput;
    }

    // MEASURE THE SOURCE
    // The histogram and checksum also detect changes between the counting and encoding passes.
    if (!scanInput(input, frequencies, &totalBytes, &checksum)) {
        fputs("Could not read the complete input or its size exceeded the supported range.\n",
              stderr);
        goto closeInput;
    }
    if (totalBytes > ULL_MAX_VALUE - HEADER_SIZE - FREQUENCY_TABLE_SIZE) {
        fputs("Input is too large for the TCMP v1 size fields.\n", stderr);
        goto closeInput;
    }

    // BUILD PREFIX CODES
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

    // SELECT THE COMPLETE FRAME SIZE
    // Include the histogram overhead before choosing Huffman over raw storage.
    encodedBytes = bitCount / 8ULL + (bitCount % 8ULL != 0 ? 1ULL : 0ULL);
    useHuffman =
        totalBytes > FREQUENCY_TABLE_SIZE && encodedBytes < totalBytes - FREQUENCY_TABLE_SIZE;
    outputBytes = HEADER_SIZE + totalBytes;
    if (useHuffman)
        outputBytes = HEADER_SIZE + FREQUENCY_TABLE_SIZE + encodedBytes;

    // CREATE AND WRITE THE SELECTED FRAME
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

    // FINALISE ONLY AFTER VALIDATED ENCODING
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
    // CLOSE OWNED HANDLES
    // A failed exclusive create must never remove a pre-existing destination.
    if (input != NULL)
        fclose(input);
    if (output != NULL)
        discardOutputFile(output, outputPath);
    return 0;
}

/** Validate and restore a TCMP archive into a new file. @author kai */
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

    // VALIDATE THE ARCHIVE REQUEST
    if (input == NULL || outputPath == NULL || outputPath[0] == '\0') {
        fputs("Input and output paths are required.\n", stderr);
        goto cleanup;
    }

    // READ THE FRAME CONTRACT
    if (!readHeader(input, &mode, &originalSize, &expectedChecksum, frequencies)) {
        fputs("Input is not a valid or supported TCMP v1 file.\n", stderr);
        goto cleanup;
    }
    if (mode == 1 && (!buildHuffmanTree(frequencies, nodes, &root) || root < 0)) {
        fputs("TCMP Huffman tree is invalid.\n", stderr);
        goto cleanup;
    }

    // RESTORE THE DECLARED PAYLOAD
    output = createOutputFile(outputPath);
    if (output == NULL)
        goto cleanup;
    DEBUG_LOG("input=%s bytes=%llu mode=%s\n", inputPath, originalSize,
              mode == 1 ? "huffman" : "raw");

    success = mode == 0 ? copyPayload(input, output, originalSize, NULL, &actualChecksum)
                        : writeDecodedHuffman(input, output, originalSize, frequencies, nodes, root,
                                              &actualChecksum);

    // VERIFY INTEGRITY BEFORE FINALISING
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
    // DISCARD ONLY AN OUTPUT CREATED BY THIS OPERATION
    if (input != NULL)
        fclose(input);
    if (output != NULL)
        discardOutputFile(output, outputPath);
    return 0;
}

/*//////////////////////////////////////////////////
INSPECTION
//////////////////////////////////////////////////*/

// HISTOGRAM SORTING
/** Compare histogram records by descending frequency and symbol tie-break. @author kai */
static int frequencyBefore(const SymbolFrequency *left, const SymbolFrequency *right) {
    if (left->frequency != right->frequency)
        return left->frequency > right->frequency;
    return left->symbol < right->symbol;
}

/** Sort histogram records by recursively merging adjacent ranges. @author kai */
static void mergeSortFrequencies(SymbolFrequency values[], SymbolFrequency scratch[], int begin,
                                 int end) {
    int middle, left = begin, right, out = begin;
    if (end - begin < 2)
        return;

    // SORT EACH HALF
    middle = begin + (end - begin) / 2;
    mergeSortFrequencies(values, scratch, begin, middle);
    mergeSortFrequencies(values, scratch, middle, end);
    right = middle;

    // MERGE AND COPY BACK
    // The same scratch range serves every recursion level after its children finish.
    while (left < middle && right < end)
        scratch[out++] =
            frequencyBefore(&values[left], &values[right]) ? values[left++] : values[right++];
    while (left < middle)
        scratch[out++] = values[left++];
    while (right < end)
        scratch[out++] = values[right++];
    memcpy(values + begin, scratch + begin, (size_t)(end - begin) * sizeof(*values));
}

/** Print byte count and the ten most frequent byte values. @author kai */
int inspectFile(const char *inputPath) {
    FILE *input = openInputFile(inputPath);
    unsigned long long frequencies[SYMBOL_COUNT];
    unsigned long long totalBytes;
    unsigned long long checksum;
    SymbolFrequency values[SYMBOL_COUNT];
    SymbolFrequency scratch[SYMBOL_COUNT];
    int count = 0;
    int symbol;

    // MEASURE THE INPUT
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

    // SORT OCCUPIED HISTOGRAM ENTRIES
    for (symbol = 0; symbol < SYMBOL_COUNT; symbol++) {
        if (frequencies[symbol] > 0) {
            values[count++] = (SymbolFrequency){symbol, frequencies[symbol]};
        }
    }
    mergeSortFrequencies(values, scratch, 0, count);
    DEBUG_LOG("inspect=%s bytes=%llu symbols=%d\n", inputPath, totalBytes, count);
    printf("File: %s\nSize: %llu bytes\nDistinct byte values: %d\nFNV-1a 64 checksum: %016llX\n",
           inputPath, totalBytes, count, checksum);

    // REPORT THE LEADING SYMBOLS
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

// BYTE SEARCH
/** Search literal input bytes with KMP and report match offsets. @author kai */
int searchFile(const char *inputPath, const char *pattern) {
    FILE *input;
    size_t patternLength;
    size_t *prefix;
    size_t matched = 0;
    unsigned long long offset = 0;
    unsigned long long matches = 0;
    int nextByte;
    size_t i;

    // VALIDATE THE SEARCH REQUEST
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

    // BUILD THE KMP FAILURE TABLE
    // Fallback lengths preserve earlier partial matches without rescanning input bytes.
    prefix[0] = 0;
    for (i = 1; i < patternLength; i++) {
        size_t candidate = prefix[i - 1];
        while (candidate > 0 && pattern[i] != pattern[candidate])
            candidate = prefix[candidate - 1];
        if (pattern[i] == pattern[candidate])
            candidate++;
        prefix[i] = candidate;
    }

    // SCAN WITHOUT BACKTRACKING THE INPUT
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

    // REPORT THE COMPLETE MATCH COUNT
    // Only the first twenty positions are printed, but every match contributes to the total.
    if (matches == 0)
        printf("No match for \"%s\" in %s.\n", pattern, inputPath);
    else
        printf("Found %llu match(es) in %s%s.\n", matches, inputPath,
               matches > 20 ? " (first 20 positions shown)" : "");
    DEBUG_LOG("search=%s patternBytes=%lu matches=%llu\n", inputPath, (unsigned long)patternLength,
              matches);
    return 1;
}
