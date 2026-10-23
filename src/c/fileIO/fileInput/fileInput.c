/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fileInput.h"

/*//////////////////////////////////////////////////
FILE PATH INPUT
//////////////////////////////////////////////////*/

static const char *fileNameFromPath(const char *path) {
    const char *forwardSlash = strrchr(path, '/');
    const char *backwardSlash = strrchr(path, '\\');
    const char *separator = forwardSlash;

    if (separator == NULL || (backwardSlash != NULL && backwardSlash > separator))
        separator = backwardSlash;
    return separator == NULL ? path : separator + 1;
}

static int hasExtension(const char *path, const char *extension) {
    const char *fileName = fileNameFromPath(path);
    const char *actualExtension = strrchr(fileName, '.');

    if (actualExtension == NULL)
        return 0;
    while (*actualExtension != '\0' && *extension != '\0') {
        if (tolower((unsigned char)*actualExtension) != tolower((unsigned char)*extension))
            return 0;
        actualExtension++;
        extension++;
    }
    return *actualExtension == '\0' && *extension == '\0';
}

static char detectFileType(const char *path) {
    // KEEP PDF INPUT DISTINCT SO THE DOCUMENT FLOW CAN FIND IMAGE OBJECTS
    if (hasExtension(path, ".pdf"))
        return 'p';

    // DOCUMENT EXTENSIONS USE THE DOCUMENT STUB FLOW
    if (hasExtension(path, ".txt") || hasExtension(path, ".doc") || hasExtension(path, ".docx") ||
        hasExtension(path, ".rtf") || hasExtension(path, ".odt"))
        return 'd';

    // IMAGE EXTENSIONS USE THE IMAGE STUB FLOW
    if (hasExtension(path, ".png") || hasExtension(path, ".jpg") || hasExtension(path, ".jpeg") ||
        hasExtension(path, ".gif") || hasExtension(path, ".bmp") || hasExtension(path, ".webp") ||
        hasExtension(path, ".tif") || hasExtension(path, ".tiff") || hasExtension(path, ".svg"))
        return 'i';

    return 'o';
}

int readInputFile(const char *inputPath, file_t *fileP) {
    FILE *source;
    long fileSize;
    size_t dataSize;
    unsigned char *data = NULL;

    if (inputPath == NULL || fileP == NULL)
        return 0;

    // OPEN IN BINARY MODE SO THE BUFFER PRESERVES EVERY BYTE
    source = fopen(inputPath, "rb");
    if (source == NULL) {
        perror(inputPath);
        return 0;
    }
    if (fseek(source, 0, SEEK_END) != 0) {
        perror(inputPath);
        fclose(source);
        return 0;
    }
    fileSize = ftell(source);
    if (fileSize < 0 || (uintmax_t)fileSize > SIZE_MAX) {
        fprintf(stderr, "Could not determine a supported size for: %s\n", inputPath);
        fclose(source);
        return 0;
    }
    if (fseek(source, 0, SEEK_SET) != 0) {
        perror(inputPath);
        fclose(source);
        return 0;
    }

    // ALLOCATE THE EXACT BUFFER AND READ THE COMPLETE FILE
    dataSize = (size_t)fileSize;
    if (dataSize > 0) {
        data = malloc(dataSize);
        if (data == NULL) {
            fprintf(stderr, "Could not allocate %zu bytes for: %s\n", dataSize, inputPath);
            fclose(source);
            return 0;
        }
        if (fread(data, 1, dataSize, source) != dataSize) {
            fprintf(stderr, "Could not read the complete input file: %s\n", inputPath);
            free(data);
            fclose(source);
            return 0;
        }
    }
    if (fclose(source) != 0) {
        perror(inputPath);
        free(data);
        return 0;
    }

    // STORE LOADED BYTES AND PATH METADATA IN THE FILE RECORD
    fileP->data = data;
    fileP->dataSize = dataSize;
    fileP->type = detectFileType(inputPath);
    snprintf(fileP->name, sizeof(fileP->name), "%s", fileNameFromPath(inputPath));
    return 1;
}

/* INPUT HELPER */

/** Drains the current input line before createFile reads a filename. */
void clearInput(void) {
    int ch;

    // DISCARD THE REST OF THE PREVIOUS MENU LINE
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}

/*//////////////////////////////////////////////////
FILE SELECTION FLOW
//////////////////////////////////////////////////*/

int selectFiles(char username[]) {
    char folderAddress[150];

    // KEEP THE INPUT FOLDER FIXED UNTIL NAVIGATION IS IMPLEMENTED
    strcpy(folderAddress, "./src/inputFolder");

    // SHOW THE AVAILABLE ENTRIES, THEN CAPTURE ONE RECORD
    displayFiles(folderAddress);
    file_t newFile = createFile(folderAddress, username);
    (void)newFile;
    return 1;
}

void displayFiles(char folderAddress[]) {
    char prompt[153] = "ls -1 ";

    // BUILD THE EXISTING NUMBERED LISTING COMMAND FOR THE MENU
    printf("Select one of the following files:\n");
    strcat(prompt, folderAddress);
    strcat(prompt, "| cat -n");
    system(prompt);
}

file_t createFile(char folderAddress[], char username[]) {
    char fileName[100];
    file_t newFile = {0};

    // REMOVE THE PREVIOUS SELECTION BEFORE READING THE NAME
    clearInput();
    printf("Enter file name > ");
    fgets(fileName, 100, stdin);

    // POPULATE THE OWNER FIELD; OTHER RECORD FIELDS REMAIN UNFINISHED
    strcpy(newFile.userName, username);

    // LOCATE AN EXTENSION FOR THE PENDING FILE-TYPE ASSIGNMENT
    char *type = strchr(fileName, '.');
    (void)folderAddress;
    (void)type;
    return newFile;
}
