#ifndef COMPRESSION_CODEC_H
#define COMPRESSION_CODEC_H

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// STANDARD LIBRARIES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*//////////////////////////////////////////////////
TYPES
//////////////////////////////////////////////////*/

// RASTER STORAGE
#define RC_LIMIT (256UL * 1024UL * 1024UL)
#define RC_PIXELS (16UL * 1024UL * 1024UL)
typedef struct {
    unsigned char *data;
    size_t size, capacity;
} RcBuffer;
typedef struct {
    unsigned width, height;
    unsigned char *rgba;
} RcImage;

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

// CODECS AND BUFFER OWNERSHIP
/** Append bytes within the processing memory limit. @author kai */
int rcAppend(RcBuffer *buffer, const void *data, size_t length);
/** Decode PNG or JPEG identified by its signature. @author kai */
int rcDecode(const unsigned char *data, size_t length, RcImage *image);
/** Decode baseline/progressive JPEG pixels. @author kai */
int rcJpeg(const unsigned char *data, size_t length, RcImage *image, int orient);
/** Inflate and validate a zlib-wrapped DEFLATE stream. @author kai */
int rcInflate(const unsigned char *data, size_t length, RcBuffer *output);
/** Encode a zlib stream using fixed Huffman codes and bounded LZ77 search. @author kai */
int rcDeflate(const unsigned char *data, size_t length, RcBuffer *output);
// SHARED PIXEL PRIMITIVES
/** Extract one legal 1/2/4/8/16-bit sample from a packed row. @author kai */
unsigned rcSample(const unsigned char *row, size_t sample, unsigned bits);
/** Reverse a PNG/PDF row filter; NULL prior supplies zero neighbours. @author kai */
int rcUnfilter(unsigned char *row, const unsigned char *prior, size_t length, size_t bpp,
               unsigned filter);
/** Calculate the CRC-32 used by PNG chunks. @author kai */
unsigned long rcCrc(const unsigned char *data, size_t length);
#endif
