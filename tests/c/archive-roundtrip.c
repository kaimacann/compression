#define _XOPEN_SOURCE 700

#include "../../src/c/compression/compression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Write an exact byte sequence to a temporary fixture file.
 * @param path Destination path to create or truncate.
 * @param bytes Source bytes to write.
 * @param length Number of source bytes to write.
 * @return Nonzero when the full sequence is written and the file closes cleanly.
 */
static int writeBytes(const char *path, const unsigned char *bytes, size_t length) {
    FILE *file = fopen(path, "wb");
    int success;

    if (!file)
        return 0;
    success = fwrite(bytes, 1, length, file) == length;
    return fclose(file) == 0 && success;
}

/**
 * Compare two files byte for byte, including their lengths.
 * @param left First file path.
 * @param right Second file path.
 * @return Nonzero when both files contain the same complete byte sequence.
 */
static int filesEqual(const char *left, const char *right) {
    FILE *a = fopen(left, "rb");
    FILE *b = fopen(right, "rb");
    int result = a && b;

    if (result) {
        int x, y;
        do {
            x = fgetc(a);
            y = fgetc(b);
            if (x != y) {
                result = 0;
                break;
            }
        } while (x != EOF);
        result = result && !ferror(a) && !ferror(b);
    }
    if (a)
        fclose(a);
    if (b)
        fclose(b);
    return result;
}

/**
 * Archive and restore one fixture while asserting its selected frame mode.
 * @param directory Temporary directory for the generated files.
 * @param name Fixture name used to derive its paths.
 * @param source Original bytes to archive.
 * @param length Number of original bytes.
 * @param expectedMode Expected raw or Huffman frame value from the container header.
 * @return Nonzero when the frame mode and restored bytes match the expectations.
 */
static int roundTrip(const char *directory, const char *name, const unsigned char *source,
                     size_t length, int expectedMode) {
    char input[1024], archive[1024], restored[1024];
    FILE *file;
    int mode;
    int success = 0;

    snprintf(input, sizeof(input), "%s/%s.input", directory, name);
    snprintf(archive, sizeof(archive), "%s/%s.tcmp", directory, name);
    snprintf(restored, sizeof(restored), "%s/%s.restored", directory, name);
    if (!writeBytes(input, source, length) || !archiveFile((CompressionPaths){input, archive}))
        goto cleanup;
    file = fopen(archive, "rb");
    if (!file)
        goto cleanup;
    if (fseek(file, 5, SEEK_SET)) {
        fclose(file);
        goto cleanup;
    }
    mode = fgetc(file);
    if (fclose(file) || mode != expectedMode)
        goto cleanup;
    if (!decompressFile((CompressionPaths){archive, restored}) || !filesEqual(input, restored))
        goto cleanup;
    success = 1;

cleanup:
    remove(input);
    remove(archive);
    remove(restored);
    return success;
}

int main(void) {
    char directory[] = "/tmp/compression-archive-roundtrip-XXXXXX";
    char input[1024], archive[1024], corrupted[1024], restored[1024];
    unsigned char raw[256];
    unsigned char repeated[16384];
    int byte;
    int temporaryDescriptor;
    FILE *file;
    size_t index;
    int passed = 0;

    for (index = 0; index < sizeof(raw); index++)
        raw[index] = (unsigned char)index;
    memset(repeated, 0x42, sizeof(repeated));

    temporaryDescriptor = mkstemp(directory);
    if (temporaryDescriptor < 0) {
        perror("mkstemp");
        return 1;
    }
    close(temporaryDescriptor);
    if (remove(directory) || mkdir(directory, 0700)) {
        perror("create temporary directory");
        remove(directory);
        return 1;
    }
    if (!roundTrip(directory, "raw", raw, sizeof(raw), 0) ||
        !roundTrip(directory, "huffman", repeated, sizeof(repeated), 1)) {
        fputs("Raw or Huffman archive round-trip failed.\n", stderr);
        goto cleanup;
    }

    snprintf(input, sizeof(input), "%s/corrupt.input", directory);
    snprintf(archive, sizeof(archive), "%s/corrupt.tcmp", directory);
    snprintf(corrupted, sizeof(corrupted), "%s/corrupt-bad.tcmp", directory);
    snprintf(restored, sizeof(restored), "%s/corrupt.restored", directory);
    if (!writeBytes(input, raw, sizeof(raw)) || !archiveFile((CompressionPaths){input, archive}))
        goto cleanup;
    file = fopen(archive, "r+b");
    if (!file || fseek(file, -1, SEEK_END)) {
        if (file)
            fclose(file);
        fputs("Could not read the archive corruption fixture.\n", stderr);
        goto cleanup;
    }
    byte = fgetc(file);
    if (byte == EOF || fseek(file, -1, SEEK_END)) {
        fclose(file);
        fputs("Could not read the archive corruption fixture.\n", stderr);
        goto cleanup;
    }
    byte ^= 1;
    if (fputc(byte, file) == EOF) {
        perror("write corrupt archive");
        fclose(file);
        goto cleanup;
    }
    if (fclose(file)) {
        perror("close corrupt archive");
        goto cleanup;
    }
    if (rename(archive, corrupted)) {
        perror("rename corrupt archive");
        goto cleanup;
    }
    if (decompressFile((CompressionPaths){corrupted, restored}) || access(restored, F_OK) == 0) {
        fputs("A corrupt archive was accepted or left a partial output.\n", stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    remove(input);
    remove(archive);
    remove(corrupted);
    remove(restored);
    rmdir(directory);
    return passed ? 0 : 1;
}
