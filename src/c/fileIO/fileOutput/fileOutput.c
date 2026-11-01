/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include <stdio.h>

#include "fileOutput.h"

/*//////////////////////////////////////////////////
FILE OUTPUT
//////////////////////////////////////////////////*/

// FILE BROWSING PLACEHOLDER
int browseFiles(void) { return 0; }

// BINARY OUTPUT
int writeOutputFile(const char *outputPath, const unsigned char *data, size_t dataSize) {
    FILE *destination;
    int success;
    if (outputPath == NULL || (data == NULL && dataSize != 0))
        return 0;
    destination = fopen(outputPath, "wb");
    if (destination == NULL) {
        perror(outputPath);
        return 0;
    }
    success = dataSize == 0 || fwrite(data, 1, dataSize, destination) == dataSize;
    if (fclose(destination) != 0)
        success = 0;
    if (!success) {
        fprintf(stderr, "Could not write the complete output file: %s\n", outputPath);
        return 0;
    }
    return 1;
}
