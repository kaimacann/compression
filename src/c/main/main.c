/**
 * @file main.c Command-line and interactive application entry point.
 * @author haadi
 * Compile with: make
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compression/compression.h"
#include "../compression/media.h"
#include "../encryption/encryption.h"
#include "../login/login.h"

/**
 * Archive input bytes and encrypt the resulting TCMP file in place.
 * @param inputPath Existing file to archive.
 * @param outputPath Destination for the encrypted archive.
 * @return 1 when archiving and encryption both succeed; otherwise 0.
 * @author haadi
 */
static int compressAndEncrypt(const char *inputPath, const char *outputPath) {
    if (!archiveFile(inputPath, outputPath))
        return 0;
    if (encryptFile(outputPath, KEY) != 0) {
        fprintf(stderr, "Could not encrypt the archive: %s\n", outputPath);
        if (remove(outputPath) != 0)
            perror("Could not remove the unencrypted archive");
        return 0;
    }
    return 1;
}

/**
 * Temporarily decrypt an archive, restore its payload, then re-encrypt it.
 * @param inputPath Encrypted TCMP archive to restore and preserve.
 * @param outputPath Destination for the restored original bytes.
 * @return 1 when restoration and source re-encryption both succeed; otherwise 0.
 * @author haadi
 */
static int decryptAndDecompress(const char *inputPath, const char *outputPath) {
    int restored;
    if (encryptFile(inputPath, KEY) != 0) {
        fprintf(stderr, "Could not decrypt the archive: %s\n", inputPath);
        return 0;
    }
    restored = decompressFile(inputPath, outputPath);
    if (encryptFile(inputPath, KEY) != 0) {
        fprintf(stderr, "Could not restore the encrypted archive: %s\n", inputPath);
        return 0;
    }
    return restored;
}

/*//////////////////////////////////////////////////
CLI
//////////////////////////////////////////////////*/

/**
 * Print supported command forms and their required arguments.
 * @param stream Destination for the usage text.
 * @author haadi
 */
static void usage(FILE *stream) {
    fputs("Usage:\n"
          "  compression-demo compress <input> <output.tcmp>\n"
          "  compression-demo decompress <input.tcmp> <output>\n"
          "  compression-demo inspect <input>\n"
          "  compression-demo search <input> <literal>\n"
          "  compression-demo process <input> <output> --mode lossless|vector [options]\n"
          "    --svg-dir <directory> --colors <2-256> --tolerance <pixels> --force\n"
          "    vector: lossy Terra sketch output, not general-purpose image compression\n"
          "  compression-demo --help\n",
          stream);
}

/**
 * Parse a positive palette size within the supported 2–256 range.
 * @param text Command-line value expected to be an integer from 2 through 256.
 * @param value Receives the validated palette size.
 * @return 1 when the value is within the supported range; otherwise 0.
 * @author haadi
 */
static int parseColors(const char *text, unsigned *value) {
    char *end;
    unsigned long parsed;
    parsed = strtoul(text, &end, 10);
    if (end == text || *end || parsed < 2 || parsed > 256)
        return 0;
    *value = (unsigned)parsed;
    return 1;
}

/**
 * Parse a finite, nonnegative contour tolerance.
 * @param text Command-line value expected to be a finite nonnegative number.
 * @param value Receives the validated contour tolerance.
 * @return 1 when the value is within the supported range; otherwise 0.
 * @author haadi
 */
static int parseTolerance(const char *text, double *value) {
    char *end;
    double parsed;
    parsed = strtod(text, &end);
    if (end == text || *end || !(parsed >= 0.0 && parsed <= 2.0))
        return 0;
    *value = parsed;
    return 1;
}

/**
 * Validate process options and dispatch the requested media operation.
 * @param argc Number of command-line arguments, including the executable name.
 * @param argv Arguments containing the input, output, and processing options.
 * @return 1 when the command succeeds; 0 when arguments or the selected operation fail.
 * @author haadi
 */
static int processCommand(int argc, char **argv) {
    MediaOptions options = {0, 0, 2, 1.0, NULL};
    int modeSeen = 0, i;
    for (i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "--mode") && i + 1 < argc) {
            const char *mode = argv[++i];
            if (modeSeen++ || (strcmp(mode, "lossless") && strcmp(mode, "vector")))
                return 0;
            options.vectorMode = !strcmp(mode, "vector");
        } else if (!strcmp(argv[i], "--svg-dir") && i + 1 < argc) {
            options.imagesDirectory = argv[++i];
        } else if (!strcmp(argv[i], "--colors") && i + 1 < argc) {
            if (!parseColors(argv[++i], &options.colors))
                return 0;
        } else if (!strcmp(argv[i], "--tolerance") && i + 1 < argc) {
            if (!parseTolerance(argv[++i], &options.tolerance))
                return 0;
        } else if (!strcmp(argv[i], "--force")) {
            options.force = 1;
        } else {
            return 0;
        }
    }
    if (!modeSeen) {
        fputs("Choose --mode lossless or --mode vector.\n", stderr);
        return 0;
    }
    return processMedia(argv[2], argv[3], &options);
}

/**
 * Dispatch one supported command-line operation.
 * @param argc Number of command-line arguments, including the executable name.
 * @param argv Parsed command and its operands.
 * @return 1 when the selected command succeeds; otherwise 0.
 * @author haadi
 */
static int runCommand(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        usage(stdout);
        return 1;
    }
    if (argc == 4 && !strcmp(argv[1], "compress"))
        return compressAndEncrypt(argv[2], argv[3]);
    if (argc == 4 && !strcmp(argv[1], "decompress"))
        return decryptAndDecompress(argv[2], argv[3]);
    if (argc == 3 && !strcmp(argv[1], "inspect"))
        return inspectFile(argv[2]);
    if (argc == 4 && !strcmp(argv[1], "search"))
        return searchFile(argv[2], argv[3]);
    if (argc >= 5 && !strcmp(argv[1], "process"))
        return processCommand(argc, argv);
    usage(stderr);
    return 0;
}

/*//////////////////////////////////////////////////
INTERACTIVE MODE
//////////////////////////////////////////////////*/

/**
 * Read one prompted path into a bounded caller-owned buffer.
 * @param prompt Message shown before reading the path.
 * @param path Caller-owned buffer receiving the trimmed path.
 * @param capacity Buffer size including space for the terminating NUL.
 * @param allowEmpty Whether a blank line is accepted for the optional output path.
 * @return 1 for a complete nonempty line, 0 for an invalid or overlong line, or -1 on end-of-input.
 * @author haadi
 */
static int readPath(const char *prompt, char *path, size_t capacity, int allowEmpty) {
    size_t length;
    int ch;
    fputs(prompt, stdout);
    if (!fgets(path, (int)capacity, stdin))
        return -1;
    length = strcspn(path, "\r\n");
    if (length == capacity - 1 && path[length] == '\0') {
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
        puts("Path is too long.");
        return 0;
    }
    path[length] = '\0';
    if (!length && !allowEmpty) {
        puts("Path cannot be empty.");
        return 0;
    }
    return 1;
}

/**
 * Confirm that an input path can be read and contains at least one byte.
 * @param path User-supplied source path.
 * @return 1 for a readable generic file, 2 for PNG/JPEG, 3 for PDF, or 0 for unreadable or empty.
 * @author haadi
 */
static int validateInput(const char *path) {
    FILE *file = fopen(path, "rb");
    unsigned char signature[8] = {0};
    size_t count;
    int byte, ok = 0, media = 0;
    if (!file) {
        perror(path);
        return 0;
    }
    count = fread(signature, 1, sizeof(signature), file);
    media = count >= 5 && !memcmp(signature, "%PDF-", 5) ? 2
            : (count >= 8 && !memcmp(signature, "\211PNG\r\n\032\n", 8)) ||
                    (count >= 2 && signature[0] == 255 && signature[1] == 216)
                ? 1
                : 0;
    if (ferror(file)) {
        fclose(file);
        fprintf(stderr, "Could not read input: %s\n", path);
        return 0;
    }
    rewind(file);
    byte = fgetc(file);
    if (byte != EOF) {
        while (byte != EOF)
            byte = fgetc(file);
        ok = !ferror(file);
    }
    fclose(file);
    if (!ok)
        fprintf(stderr, "Input is empty or unreadable: %s\n", path);
    return ok ? media + 1 : 0;
}

/**
 * Derive a sibling output path by adding the compression suffix before its extension.
 * @param input Source path whose directory and extension are retained.
 * @param output Buffer receiving the derived path.
 * @param capacity Output buffer size including its terminating NUL.
 * @param extension Optional replacement extension, without the leading dot.
 * @return 1 when the derived path fits, otherwise 0.
 * @author haadi
 */
static int defaultOutputPath(const char *input, char *output, size_t capacity,
                             const char *extension) {
    const char *slash = strrchr(input, '/');
    const char *dot = strrchr(input, '.');
    size_t stem = strlen(input), suffix = strlen("-compressed");
    if (dot && (!slash || dot > slash))
        stem = (size_t)(dot - input);
    size_t ending = extension ? strlen(extension) + 1 : strlen(input + stem);
    if (stem + suffix + ending + 1 > capacity)
        return 0;
    memcpy(output, input, stem);
    memcpy(output + stem, "-compressed", suffix);
    if (extension) {
        output[stem + suffix] = '.';
        strcpy(output + stem + suffix + 1, extension);
    } else
        strcpy(output + stem + suffix, input + stem);
    return 1;
}

/**
 * Process a raster or PDF as a lossy SVG vector export.
 * @param input Validated PNG, JPEG, or PDF path.
 * @param output Destination for the SVG or rebuilt PDF.
 * @return 1 when vector processing succeeds; otherwise 0.
 * @author haadi
 */
static int vectorizeMedia(const char *input, const char *output) {
    MediaOptions options = {1, 0, 2, 1.0, NULL};
    return processMedia(input, output, &options);
}

/**
 * Prompt for input and output paths, then invoke the selected file action.
 * @param label Action name displayed in the input/output prompts.
 * @param action File operation called after both paths are read.
 * @return The path-read status on input failure, or 1 after the operation result is reported.
 * @author haadi
 */
static int runInteractiveAction(const char *label, int (*action)(const char *, const char *)) {
    char input[512], output[512], mode[32];
    const char *extension = NULL;
    int kind;
    int status = readPath("Input path: ", input, sizeof(input), 0);
    if (status <= 0)
        return status;
    kind = validateInput(input);
    if (!kind)
        return 1;
    if (!strcmp(label, "Compression") && kind > 1) {
        puts("\nChoose a media output style:");
        puts("+-- 1) Preserve media unchanged (lossless copy)");
        if (kind == 3)
            puts("`-- 2) Terra sketch mode (lossy PDF rebuild + SVG sidecars)");
        else
            puts("`-- 2) Terra sketch mode (lossy SVG, two-colour default)");
        puts("    Note: tuned for simple, low-detail Terra sketches; not general-purpose.");
        status = readPath("Choose 1 or 2: ", mode, sizeof(mode), 0);
        if (status <= 0)
            return status;
        if (!strcmp(mode, "1"))
            extension = kind == 3 ? "pdf" : NULL;
        else if (!strcmp(mode, "2")) {
            action = vectorizeMedia;
            extension = kind == 3 ? "pdf" : "svg";
        } else {
            puts("Invalid compression type.");
            return 1;
        }
    } else if (!strcmp(label, "Compression")) {
        extension = "tcmp";
        action = compressAndEncrypt;
    } else if (!strcmp(label, "Decompression")) {
        action = decryptAndDecompress;
    }
    status =
        readPath("Output path (Enter for sibling -compressed name): ", output, sizeof(output), 1);
    if (status <= 0)
        return status;
    if (!output[0] && !defaultOutputPath(input, output, sizeof(output), extension)) {
        puts("Could not derive an output path; enter one explicitly.");
        return 1;
    }
    printf("%s %s.\n", label, action(input, output) ? "complete" : "failed");
    return 1;
}

/**
 * Run the interactive compression menu until the user exits.
 * @return 1 after menu exit or end-of-input.
 * @author haadi
 */
static int runMenu(void) {
    char line[32];
    int choice;
    char *end;
    long parsed;
    do {
        puts("\n+----------------------------------------------+");
        puts("|             TERRA COMPRESSION                |");
        puts("+----------------------------------------------+");
        puts("| 1) Process a file                            |");
        puts("|    +-- PNG/JPEG/PDF: choose output style     |");
        puts("|    `-- Other files: lossless TCMP archive    |");
        puts("| 2) Restore a TCMP archive                    |");
        puts("| 3) Exit                                      |");
        puts("+----------------------------------------------+");
        fputs("Choose [1-3]: ", stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        if (!strchr(line, '\n')) {
            int ch = getchar();
            if (ch != '\n' && ch != EOF) {
                while (ch != '\n' && ch != EOF)
                    ch = getchar();
                puts("Invalid selection.");
                choice = 0;
                continue;
            }
        }
        parsed = strtol(line, &end, 10);
        while (*end == ' ' || *end == '\t')
            end++;
        if (end == line || (*end != '\n' && *end != '\r' && *end != '\0') || parsed < 1 ||
            parsed > 3)
            choice = 0;
        else
            choice = (int)parsed;
        if (!choice)
            puts("Invalid selection.");
        if (choice == 1)
            runInteractiveAction("Compression", compressFile);
        else if (choice == 2)
            runInteractiveAction("Decompression", decompressFile);
    } while (choice != 3);
    return 1;
}

/*//////////////////////////////////////////////////
ENTRY POINT
//////////////////////////////////////////////////*/

/**
 * Select command-line or interactive mode.
 * @param argc Number of command-line arguments.
 * @param argv Argument vector; selects command-line mode when supplied.
 * @return Process exit status for the C runtime.
 * @author haadi
 */
int main(int argc, char **argv) {
    int status;
    if (!(argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) &&
        loginFunction() != 0) {
        fputs("Login failed; access denied.\n", stderr);
        return EXIT_FAILURE;
    }
    status = argc == 1 ? runMenu() : runCommand(argc, argv);
    return status ? EXIT_SUCCESS : EXIT_FAILURE;
}
