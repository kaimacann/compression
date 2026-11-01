/**
 * @file compression.h
 * @brief File-level compression and restoration entry points.
 */

#ifndef COMPRESSION_H
#define COMPRESSION_H

/*//////////////////////////////////////////////////
PUBLIC API
//////////////////////////////////////////////////*/

// FILE TRANSFORMS
/**
 * Compresses a file into the RLE container or applies PDF image-stream RLE.
 * The input is left untouched. Valid PDFs that cannot be transformed or do not
 * shrink are copied unchanged; other files use stored mode when RLE does not shrink.
 *
 * @param inputPath Existing source file.
 * @param outputPath Destination for the compressed file.
 * @return 1 when processing and writing succeed, or 0 on invalid input or I/O failure.
 */
int compressFile(const char *inputPath, const char *outputPath);
/**
 * Restores a file written by compressFile. PDF restoration removes the RLE
 * filter added to eligible image streams.
 *
 * @param inputPath RLE container or PDF produced by this program.
 * @param outputPath Destination for the restored file.
 * @return 1 when the file is restored and written, or 0 when it is not a supported output.
 */
int decompressFile(const char *inputPath, const char *outputPath);

#endif /* COMPRESSION_H */
