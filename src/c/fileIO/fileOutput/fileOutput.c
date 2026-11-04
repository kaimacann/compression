/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include <stdio.h>

#include "fileOutput.h"

/*//////////////////////////////////////////////////
FILE OUTPUT
//////////////////////////////////////////////////*/

/** Report the current placeholder browse result.
 * @return 0 because saved-file browsing is not implemented in this module.
 * @author haadi
 */
int browseFiles(void) { return 0; }

/** Write an entire byte buffer to a destination path.
 * @param outputPath Destination opened in binary write mode.
 * @param data Bytes to write; may be NULL only when dataSize is zero.
 * @param dataSize Number of bytes requested for output.
 * @return 1 when all bytes are written and the file closes successfully; otherwise 0.
 * @author haadi
 */
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
