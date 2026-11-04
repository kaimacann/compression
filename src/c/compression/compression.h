#ifndef COMPRESSION_H
#define COMPRESSION_H

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* PROCESSING OPERATIONS*/
/** Preserve supported media or archive non-media bytes in TCMP.
 * @param inputPath Source file; its contents determine media routing.
 * @param outputPath New destination; existing files are preserved.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int compressFile(const char *inputPath, const char *outputPath);

/** Encode any byte stream as the private TCMP container.
 * @param inputPath Source file.
 * @param outputPath New archive destination.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int archiveFile(const char *inputPath, const char *outputPath);

/** Restore bytes from a valid TCMP container.
 * @param inputPath Archive source.
 * @param outputPath New restored-file destination.
 * @return 1 on success, otherwise 0.
 * @author kai
 */
int decompressFile(const char *inputPath, const char *outputPath);

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
