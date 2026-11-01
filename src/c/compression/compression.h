#ifndef COMPRESSION_H
#define COMPRESSION_H

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

// PROCESSING OPERATIONS
/** Process media losslessly or archive other input. @author kai */
int compressFile(const char *inputPath, const char *outputPath);

/** Explicitly encode any byte stream as TCMP. @author kai */
int archiveFile(const char *inputPath, const char *outputPath);

/** Restore bytes from a valid TCMP container. @author kai */
int decompressFile(const char *inputPath, const char *outputPath);

/** Print size and the ten most frequent byte values in a file. @author kai */
int inspectFile(const char *inputPath);

/** Search a file for a literal byte sequence using Knuth-Morris-Pratt. @author kai */
int searchFile(const char *inputPath, const char *pattern);

#endif
