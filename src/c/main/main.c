/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/** @file main.c
 * @brief Interactive compression demo menu and input handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compression/compression.h"

/*//////////////////////////////////////////////////
MENU INPUT
//////////////////////////////////////////////////*/

/** Reads and parses one numeric menu selection.
 * @param prompt Prompt written before reading input.
 * @return Parsed selection, zero for invalid input, or -1 at end of input.
 */
static int readMenuChoice(const char *prompt) {
    char line[32];
    char *end;
    long choice;

    // READ THE NEXT INPUT BUFFER; KEEP EOF DISTINCT FROM INVALID INPUT
    fputs(prompt, stdout);
    if (fgets(line, sizeof(line), stdin) == NULL)
        return -1;

    // PARSE THE LEADING DECIMAL CHOICE; A NON-NUMERIC LINE IS INVALID
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

    // READ ONE PATH AND REMOVE ITS LINE ENDING
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

/*//////////////////////////////////////////////////
MENU DISPLAY
//////////////////////////////////////////////////*/

/** Displays the top-level menu and returns the user's selection.
 * @return Parsed menu selection, zero for invalid input, or -1 at EOF.
 */
static int showMainMenu(void) {
    // SHOW THE TOP-LEVEL COMPRESSION ACTIONS
    puts("\n=== Compression stub ===");
    puts("1. Compress file");
    puts("2. Decompress file");
    puts("3. Exit");

    // READ THE SELECTION
    return readMenuChoice("Selection: ");
}

/*//////////////////////////////////////////////////
APPLICATION ENTRY POINT
//////////////////////////////////////////////////*/

/** @brief Runs the compression demo menu until the user exits or input ends.
 * @return Zero after the demo menu closes.
 */
int main(void) {
    int selection;
    char inputPath[512];
    char outputPath[512];

    // INTRODUCE THE DEMO
    puts("Compression program stub");

    // KEEP DISPATCHING MENU CHOICES UNTIL EXIT OR EOF
    do {
        selection = showMainMenu();

        // ROUTE EACH ACTION TO ITS FILE-LEVEL COMPRESSION FLOW
        switch (selection) {
        case 1: {
            int pathStatus = readPath("Input path (PDF or image): ", inputPath, sizeof(inputPath));
            if (pathStatus < 0) {
                selection = -1;
                break;
            }
            if (pathStatus == 0)
                break;

            pathStatus = readPath("Output path: ", outputPath, sizeof(outputPath));
            if (pathStatus < 0) {
                selection = -1;
                break;
            }
            if (pathStatus == 1)
                compressFile(inputPath, outputPath);
            break;
        }
        case 2: {
            int pathStatus = readPath("Input path: ", inputPath, sizeof(inputPath));
            if (pathStatus < 0) {
                selection = -1;
                break;
            }
            if (pathStatus == 0)
                break;

            pathStatus = readPath("Output path: ", outputPath, sizeof(outputPath));
            if (pathStatus < 0) {
                selection = -1;
                break;
            }
            if (pathStatus == 1)
                decompressFile(inputPath, outputPath);
            break;
        }
        default:
            break;
        }
    } while (selection != 3 && selection != -1);

    // CLOSE THE MENU SESSION
    puts("Done.");
    return 0;
}
