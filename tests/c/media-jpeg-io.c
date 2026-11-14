#define _POSIX_C_SOURCE 200809L

#include "../../src/c/compression/media.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * Convert the checked-in JPEG fixture and verify the SVG representation and missing-input failure.
 * @return Zero when the JPEG conversion succeeds and a missing source creates no output; otherwise
 * one.
 */
int main(void) {
    char temporaryDirectory[] = "/tmp/compression-media-jpeg-XXXXXX";
    char vectorOutput[1024] = {0};
    char missingOutput[1024] = {0};
    char ending[8] = {0};
    char header[192] = {0};
    const char *fixture = "docs/development/tests/before/test.jpeg";
    const char *missingInput = "docs/development/tests/before/missing.jpeg";
    const char endingText[] = "</svg>\n";
    MediaOptions options = {.vectorMode = 1, .force = 0, .colors = 8, .tolerance = 0.8};
    FILE *file = NULL;
    long size;
    int temporaryDescriptor;
    int passed = 0;

    temporaryDescriptor = mkstemp(temporaryDirectory);
    if (temporaryDescriptor < 0) {
        perror("mkstemp");
        return 1;
    }
    if (close(temporaryDescriptor) || unlink(temporaryDirectory) ||
        mkdir(temporaryDirectory, 0700)) {
        perror("create temporary directory");
        unlink(temporaryDirectory);
        return 1;
    }
    if (snprintf(vectorOutput, sizeof(vectorOutput), "%s/converted.svg", temporaryDirectory) < 0 ||
        snprintf(missingOutput, sizeof(missingOutput), "%s/missing.svg", temporaryDirectory) < 0) {
        fputs("Could not prepare temporary output paths.\n", stderr);
        goto cleanup;
    }

    if (!processMedia(fixture, vectorOutput, &options)) {
        fputs("The checked-in JPEG fixture could not be vectorized.\n", stderr);
        goto cleanup;
    }
    file = fopen(vectorOutput, "rb");
    if (!file) {
        fputs("The JPEG conversion did not create its SVG output.\n", stderr);
        goto cleanup;
    }
    if (fread(header, 1, sizeof(header) - 1, file) < 1 || strncmp(header, "<svg xmlns=", 11) ||
        !strstr(header, "width=\"1566\" height=\"1507\"")) {
        fputs("The JPEG conversion did not preserve the fixture dimensions in SVG.\n", stderr);
        goto cleanup;
    }
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < (long)(sizeof(endingText) - 1) ||
        fseek(file, -(long)(sizeof(endingText) - 1), SEEK_END) ||
        fread(ending, 1, sizeof(endingText) - 1, file) != sizeof(endingText) - 1 ||
        memcmp(ending, endingText, sizeof(endingText) - 1)) {
        fputs("The JPEG conversion did not finish a complete SVG document.\n", stderr);
        goto cleanup;
    }
    if (fclose(file)) {
        file = NULL;
        fputs("Could not close the converted SVG.\n", stderr);
        goto cleanup;
    }
    file = NULL;

    errno = 0;
    if (processMedia(missingInput, missingOutput, &options) || access(missingOutput, F_OK) == 0 ||
        errno != ENOENT) {
        fputs("A missing media source should fail without creating an output.\n", stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (file)
        fclose(file);
    if (vectorOutput[0])
        unlink(vectorOutput);
    if (missingOutput[0])
        unlink(missingOutput);
    if (rmdir(temporaryDirectory)) {
        perror("remove temporary directory");
        passed = 0;
    }
    return passed ? 0 : 1;
}
