/** @file main.c
 * @brief Interactive compression menu with login and file encryption.
 */

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../encryption/encryption.h"
#include "../compression/compression.h"
#include "../login/login.h"

/*//////////////////////////////////////////////////
HELPERS
//////////////////////////////////////////////////*/

/** Reads and parses one numeric menu selection.
 * @param prompt Prompt written before reading input.
 * @return Parsed selection, zero for invalid input, or -1 at end of input.
 */
static int readMenuChoice(const char *prompt) {
    char line[32];
    char *end;
    long choice;

    fputs(prompt, stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
        return -1;

    choice = strtol(line, &end, 10);
    if (end == line)
        return 0;
    return (int)choice;
}

/** Reads one non-empty path into a bounded caller-provided buffer.
 * @param prompt Prompt written before reading input.
 * @param path Buffer that receives the path.
 * @param capacity Size of the path buffer.
 * @return 1 for a path, zero for invalid input, or -1 at end of input.
 */
static int readPath(const char *prompt, char *path, size_t capacity) {
    size_t length;
    int ch;

    fputs(prompt, stdout);
    if (fgets(path, (int)capacity, stdin) == NULL)
        return -1;
    length = strcspn(path, "\r\n");
    if (path[length] == '\0' && length == capacity - 1) {
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
        puts("Path is too long.");
        return 0;
    }
    path[length] = '\0';
    if (length == 0) {
        puts("Path cannot be empty.");
        return 0;
    }
    return 1;
}

/** Reads source and destination paths, then runs a file action.
 * @param inputPrompt Prompt appropriate to the action's source file.
 * @param action File-level compression or decompression function.
 * @param actionName Name used in the completion message.
 * @param encryptOutput Whether to encrypt a successful output file.
 * @param decryptInput Whether to decrypt the input file in place first.
 * @return -1 when input ends, otherwise zero after handling the request.
 */
static int runFileAction(const char *inputPrompt,
                         int (*action)(const char *, const char *),
                         const char *actionName,
                         int encryptOutput,
                         int decryptInput) {
    char inputPath[512];
    char outputPath[512];
    int pathStatus = readPath(inputPrompt, inputPath, sizeof(inputPath));
    int success;

    if (pathStatus < 0)
        return -1;
    if (pathStatus == 0)
        return 0;

    pathStatus = readPath("Output path: ", outputPath, sizeof(outputPath));
    if (pathStatus < 0)
        return -1;
    if (pathStatus == 0)
        return 0;

    if (decryptInput && encryptFile(inputPath, KEY) != 0) {
        printf("%s failed.\n", actionName);
        return 0;
    }

    success = action(inputPath, outputPath);
    if (success && encryptOutput)
        success = encryptFile(outputPath, KEY) == 0;
    printf("%s %s.\n", actionName, success ? "complete" : "failed");
    return 0;
}

/** Displays the top-level menu and returns the user's selection.
 * @return Parsed menu selection, zero for invalid input, or -1 at EOF.
 */
static int showMainMenu(void) {
    puts("\n=== Compression demo ===");
    puts("1. Compress & encrypt file");
    puts("2. Decrypt & decompress file");
    puts("3. Exit");
    return readMenuChoice("Selection: ");
}

/*//////////////////////////////////////////////////
APPLICATION ENTRY POINT
//////////////////////////////////////////////////*/

/** @brief Runs the compression menu after successful login.
 * @return Zero after the menu closes, nonzero when login fails.
 */
int main(void) {
    int selection;

    if (loginFunction() != 0) {
        puts("Login failed.");
        return EXIT_FAILURE;
    }

    puts("Compression program");
    do {
        selection = showMainMenu();
        switch (selection) {
        case 1:
            if (runFileAction("Input path (PDF or image): ", compressFile,
                              "Compression", 1, 0) < 0)
                selection = -1;
            break;
        case 2:
            if (runFileAction("Input path: ", decompressFile,
                              "Decompression", 0, 1) < 0)
                selection = -1;
            break;
        default:
            break;
        }
    } while (selection != 3 && selection != -1);

    puts("Done.");
    return EXIT_SUCCESS;
}
