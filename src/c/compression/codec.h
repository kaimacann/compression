#ifndef COMPRESSION_CODEC_H
#define COMPRESSION_CODEC_H

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* STANDARD LIBRARIES*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*//////////////////////////////////////////////////
TYPES
//////////////////////////////////////////////////*/

/* RASTER STORAGE*/
#define RC_LIMIT (256UL * 1024UL * 1024UL)
#define RC_PIXELS (16UL * 1024UL * 1024UL)
/**
 * Bounded byte storage whose buffer owns its allocated memory.
 * @author kai
 */
typedef struct {
    /**
     * Owned bytes.
     * @author kai
     */
    unsigned char *data;
    /**
     * Bytes currently stored.
     * @author kai
     */
    size_t size;
    /**
     * Allocated bytes.
     * @author kai
     */
    size_t capacity;
} RcBuffer;
/**
 * Decoded raster image in row-major RGBA form.
 * @author kai
 */
typedef struct {
    /**
     * Pixel width.
     * @author kai
     */
    unsigned width;
    /**
     * Pixel height.
     * @author kai
     */
    unsigned height;
    /**
     * Owned, row-major RGBA pixels.
     * @author kai
     */
    unsigned char *rgba;
} RcImage;

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* CODECS AND BUFFER OWNERSHIP*/
/**
 * Append bytes within the processing memory limit.
 * @param buffer Destination buffer.
 * @param data Bytes to append.
 * @param length Number of bytes to append.
 * @return Nonzero on success.
 * @author kai
 */
int rcAppend(RcBuffer *buffer, const void *data, size_t length);
/**
 * Decode PNG or JPEG identified by its signature.
 * @param data Encoded source bytes.
 * @param length Source length.
 * @param image Receives owned RGBA pixels on success.
 * @return Nonzero on success.
 * @author kai
 */
int rcDecode(const unsigned char *data, size_t length, RcImage *image);
/**
 * Decode baseline or progressive JPEG pixels.
 * @param data Encoded JPEG bytes.
 * @param length Source length.
 * @param image Receives owned RGBA pixels on success.
 * @param orient Apply EXIF orientation when nonzero.
 * @return Nonzero on success.
 * @author kai
 */
int rcJpeg(const unsigned char *data, size_t length, RcImage *image, int orient);
/**
 * Inflate and validate a zlib-wrapped DEFLATE stream.
 * @param data Compressed zlib bytes.
 * @param length Source length.
 * @param output Receives decoded bytes.
 * @return Nonzero on success.
 * @author kai
 */
int rcInflate(const unsigned char *data, size_t length, RcBuffer *output);
/**
 * Encode a zlib stream using fixed Huffman codes and bounded LZ77 search.
 * @param data Bytes to encode.
 * @param length Source length.
 * @param output Receives encoded bytes.
 * @return Nonzero on success.
 * @author kai
 */
int rcDeflate(const unsigned char *data, size_t length, RcBuffer *output);
/* SHARED PIXEL PRIMITIVES*/
/**
 * Extract one legal 1/2/4/8/16-bit sample from a packed row.
 * @param row Packed samples.
 * @param sample Zero-based sample index.
 * @param bits Bits per sample.
 * @return The sample value.
 * @author kai
 */
unsigned rcSample(const unsigned char *row, size_t sample, unsigned bits);
/**
 * Reverse a PNG or PDF row filter; NULL prior supplies zero neighbours.
 * @param row Encoded row, replaced by decoded samples.
 * @param prior Previous decoded row, or NULL for the first row.
 * @param length Row length in bytes.
 * @param bpp Bytes per pixel.
 * @param filter Filter type from zero through four.
 * @return Nonzero for a supported filter.
 * @author kai
 */
int rcUnfilter(unsigned char *row, const unsigned char *prior, size_t length, size_t bpp,
               unsigned filter);
/**
 * Calculate the CRC-32 used by PNG chunks.
 * @param data Source bytes.
 * @param length Source length.
 * @return CRC-32 value.
 * @author kai
 */
unsigned long rcCrc(const unsigned char *data, size_t length);
#endif
