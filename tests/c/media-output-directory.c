#define _POSIX_C_SOURCE 200809L

#include "../../src/c/compression/media.c"

#include <sys/stat.h>
#include <unistd.h>

/**
 * Verifies a forced write leaves a populated directory target untouched.
 * @return Zero when the directory contents survive, otherwise one for setup or assertion failure.
 */
int main(void) {
    char temporaryFile[] = "/tmp/compression-media-output-XXXXXX";
    char destination[1024] = {0};
    char sentinel[1024] = {0};
    const unsigned char source[] = "original";
    const unsigned char replacement[] = "replacement";
    const char contents[] = "keep this file";
    FILE *file = NULL;
    Output output = {0};
    struct stat metadata;
    char actual[sizeof(contents)] = {0};
    int temporaryDescriptor;
    int passed = 0;

    temporaryDescriptor = mkstemp(temporaryFile);
    if (temporaryDescriptor < 0) {
        perror("mkstemp");
        return 1;
    }
    close(temporaryDescriptor);
    if (remove(temporaryFile) || mkdir(temporaryFile, 0700)) {
        perror("create temporary directory");
        remove(temporaryFile);
        return 1;
    }
    snprintf(destination, sizeof(destination), "%s/output", temporaryFile);
    snprintf(sentinel, sizeof(sentinel), "%s/keep.txt", destination);
    if (mkdir(destination, 0700)) {
        perror("mkdir");
        goto cleanup;
    }
    file = fopen(sentinel, "wb");
    if (!file) {
        fputs("Could not prepare the output directory fixture.\n", stderr);
        goto cleanup;
    }
    if (fwrite(contents, 1, sizeof(contents), file) != sizeof(contents)) {
        fclose(file);
        file = NULL;
        fputs("Could not prepare the output directory fixture.\n", stderr);
        goto cleanup;
    }
    if (fclose(file)) {
        file = NULL;
        fputs("Could not close the output directory fixture.\n", stderr);
        goto cleanup;
    }
    file = NULL;

    output.path = copyText(destination);
    output.data = replacement;
    output.length = sizeof(replacement) - 1;
    if (!output.path) {
        fputs("Could not allocate the output path.\n", stderr);
        goto cleanup;
    }
    if (writeOutputs(&output, 1, source, sizeof(source) - 1, 1)) {
        if (remove(destination) || !output.backup || rename(output.backup, destination))
            fputs("Could not restore the directory after the regression was reproduced.\n", stderr);
        fputs("--force unexpectedly replaced a directory target.\n", stderr);
        goto cleanup;
    }
    if (lstat(destination, &metadata) || !S_ISDIR(metadata.st_mode)) {
        fputs("The directory output target was changed.\n", stderr);
        goto cleanup;
    }
    file = fopen(sentinel, "rb");
    if (!file) {
        fputs("The directory contents could not be read after the rejected replacement.\n", stderr);
        goto cleanup;
    }
    if (fread(actual, 1, sizeof(actual), file) != sizeof(actual)) {
        fclose(file);
        file = NULL;
        fputs("The directory contents could not be read after the rejected replacement.\n", stderr);
        goto cleanup;
    }
    if (fclose(file)) {
        file = NULL;
        fputs("The directory contents could not be closed after verification.\n", stderr);
        goto cleanup;
    }
    file = NULL;
    if (memcmp(actual, contents, sizeof(contents))) {
        fputs("The directory contents changed after the rejected replacement.\n", stderr);
        goto cleanup;
    }
    passed = 1;

cleanup:
    if (file)
        fclose(file);
    if (output.stage)
        remove(output.stage);
    free(output.path);
    free(output.stage);
    free(output.backup);
    if (sentinel[0])
        remove(sentinel);
    if (destination[0])
        rmdir(destination);
    rmdir(temporaryFile);
    return passed ? 0 : 1;
}
