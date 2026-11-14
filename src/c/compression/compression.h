#ifndef COMPRESSION_H
#define COMPRESSION_H

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* PROCESSING OPERATIONS*/
/** Paths used by compression operations.
 * @author kai
 */
typedef struct {
    /** Source file path. */
    const char *input;
    /** Destination file path. */
    const char *output;
} CompressionPaths;

/** Preserve supported media or archive non-media bytes in TCMP.
 * @param inputPath Source file; its contents determine media routing.
 * @param outputPath New destination; existing files are preserved.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int compressFile(const char *inputPath, const char *outputPath);

/** Encode any byte stream as the private TCMP container.
 * @param paths Source file and new archive destination.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int archiveFile(CompressionPaths paths);

/** Restore bytes from a valid TCMP container.
 * @param paths Archive source and new restored-file destination.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int decompressFile(CompressionPaths paths);

/** Print size and the ten most frequent byte values in a file.
 * @param inputPath Source file.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int inspectFile(const char *inputPath);

/** Search a file for a literal byte sequence using Knuth-Morris-Pratt.
 * @param inputPath Source file.
 * @param pattern Non-empty byte sequence to find.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int searchFile(const char *inputPath, const char *pattern);

#endif
