/**
 * @file jpeg.c Original baseline and progressive Huffman JPEG decoder.
 * @author kai
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* SHARED CONTRACT*/
#include "codec.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

/* PROCESSING STATE*/
enum {
    JPEG_BLOCK_SIDE = 8,
    JPEG_BLOCK_COEFFICIENTS = JPEG_BLOCK_SIDE * JPEG_BLOCK_SIDE,
    JPEG_CHANNEL_COUNT = 3,
    JPEG_TABLE_COUNT = 4,
    JPEG_SAMPLE_MAX = 255,
    JPEG_HUFFMAN_SYMBOL_CAPACITY = 256,
    JPEG_HUFFMAN_MAX_CODE_LENGTH = 16,
    JPEG_MAX_DIMENSION = 1048576,
    JPEG_MARKER_SOF0 = 192,
    JPEG_MARKER_SOF2 = 194,
    JPEG_MARKER_DHT = 196,
    JPEG_MARKER_DQT = 219,
    JPEG_MARKER_DRI = 221,
    JPEG_MARKER_SOS = 218,
    JPEG_MARKER_APP1 = 225,
    JPEG_MARKER_APP14 = 238,
    JPEG_MARKER_SOI = 216,
    JPEG_MARKER_EOI = 217,
    JPEG_MARKER_RESTART_FIRST = 208,
    JPEG_MARKER_RESTART_LAST = 215,
    JPEG_EXIF_TIFF_MAGIC = 42,
    JPEG_EXIF_ORIENTATION_TAG = 274,
    JPEG_EXIF_SHORT_TYPE = 3,
    JPEG_EXIF_ENTRY_BYTES = 12,
    JPEG_EXIF_HEADER_BYTES = 6,
    JPEG_MAX_DC_CATEGORY = 11,
    JPEG_ENTROPY_BYTE_BITS = 8,
    JPEG_WORD_SHIFT_BITS = JPEG_ENTROPY_BYTE_BITS * 2,
    JPEG_MAX_ORIENTATION = 8,
    JPEG_EXIF_MINIMUM_BYTES = 14,
    JPEG_FRAME_COMPONENT_COUNT_OFFSET = 5,
    JPEG_AC_ZERO_RUN_LENGTH = 16,
    JPEG_LAST_COEFFICIENT = JPEG_BLOCK_COEFFICIENTS - 1,
    JPEG_ORIENTATION_TRANSPOSE_START = 5,
    JPEG_APP14_SIGNATURE_BYTES = 5,
    JPEG_SAMPLING_NIBBLE_SHIFT = 4,
    JPEG_EXIF_OFFSET_BASE = 2,
    JPEG_MAX_SAMPLE_VALUE = 255,
    JPEG_NIBBLE_MASK = 15,
    JPEG_RESTART_SEQUENCE_MASK = 7,
    JPEG_RESTART_MARKER_BASE = 208,
    JPEG_BASELINE_MAX_AC_CATEGORY = 10,
    JPEG_MAX_PROGRESSIVE_APPROXIMATION = 13,
    JPEG_MAX_MCU_BLOCKS = 10,
    JPEG_COEFFICIENT_LIMIT = JPEG_MAX_DIMENSION,
    JPEG_FRAME_HEADER_BYTES = 6,
    JPEG_FRAME_COMPONENT_BYTES = 3,
    JPEG_COMPONENT_MAX_SAMPLING = 4,
    JPEG_COMPONENT_ID_OFFSET = 6,
    JPEG_COMPONENT_SAMPLING_OFFSET = 7,
    JPEG_COMPONENT_QUANTIZATION_OFFSET = 8,
    JPEG_PIXEL_CHANNELS = 4,
    JPEG_TRANSFORM_CENTER = 128,
    JPEG_TRANSFORM_DIVISOR = 4,
    JPEG_QUANTIZATION_WIDE_BYTES = 128,
    JPEG_QUANTIZATION_NARROW_BYTES = 64,
    JPEG_HUFFMAN_COUNT_BYTES = 16,
    JPEG_RESTART_INTERVAL_BYTES = 2,
    JPEG_APP14_MINIMUM_BYTES = 12,
    JPEG_APP14_TRANSFORM_OFFSET = 11,
    JPEG_MARKER_TEM = 1,
    JPEG_MARKER_SOF_RESERVED_FIRST = 192,
    JPEG_MARKER_SOF_RESERVED_LAST = 207,
    JPEG_MARKER_DNL = 220,
};
#define JPEG_HALF_SAMPLE 0.5
#define JPEG_TWO_SAMPLES 2.0
#define JPEG_IDCT_DC_SCALE 8.0
#define JPEG_IDCT_NORMALIZATION 0.7071067811865475
#define JPEG_PI 3.14159265358979323846
#define JPEG_YCBCR_RED_SCALE 1.402
#define JPEG_YCBCR_GREEN_BLUE_SCALE 0.344136
#define JPEG_YCBCR_GREEN_RED_SCALE 0.714136
#define JPEG_YCBCR_BLUE_SCALE 1.772
static const unsigned char zig[JPEG_BLOCK_COEFFICIENTS] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};
typedef struct {
    unsigned count[JPEG_HUFFMAN_MAX_CODE_LENGTH + 1], symbol[JPEG_HUFFMAN_SYMBOL_CAPACITY];
} JHuff;
typedef struct {
    unsigned id, h, v, q, bx, by, actualX, actualY;
    int dc, last[JPEG_BLOCK_COEFFICIENTS];
    int *coef;
    unsigned char *samples;
} JComp;
typedef struct {
    const unsigned char *p;
    size_t n, at;
    unsigned bits, count;
    int bad;
    unsigned w, h, nc, mh, mv, mx, my, restart, progressive, rgb, orientation;
    unsigned quant[JPEG_TABLE_COUNT][JPEG_BLOCK_COEFFICIENTS];
    JHuff tables[2][JPEG_TABLE_COUNT];
    JComp c[JPEG_CHANNEL_COUNT];
} Jpeg;
/** Per-block table selectors and spectral approximation range. */
typedef struct {
    unsigned dcTable;               /**< DC Huffman selector. */
    unsigned acTable;               /**< AC Huffman selector. */
    unsigned spectralStart;         /**< First selected coefficient. */
    unsigned spectralEnd;           /**< Last selected coefficient. */
    unsigned previousApproximation; /**< Prior progressive bit plane. */
    unsigned currentApproximation;  /**< Current progressive bit plane. */
} BlockScan;
/** Component selections and MCU dimensions for one validated scan. */
typedef struct {
    unsigned componentCount;               /**< Number of selected components. */
    unsigned indices[JPEG_CHANNEL_COUNT];  /**< Frame indices of selected components. */
    unsigned dcTables[JPEG_CHANNEL_COUNT]; /**< Selected DC Huffman tables. */
    unsigned acTables[JPEG_CHANNEL_COUNT]; /**< Selected AC Huffman tables. */
    BlockScan band;                        /**< Selected progressive spectral band. */
    unsigned mcuColumns;                   /**< Number of horizontal scan units. */
    unsigned mcuRows;                      /**< Number of vertical scan units. */
} ScanConfig;
/** Coordinates of one macroblock in a scan. */
typedef struct {
    unsigned x; /**< Horizontal MCU coordinate. */
    unsigned y; /**< Vertical MCU coordinate. */
} McuPosition;
/** Coordinates of one output pixel. */
typedef struct {
    unsigned x; /**< Horizontal output coordinate. */
    unsigned y; /**< Vertical output coordinate. */
} PixelPosition;
/** Spectral cursor and progressive end-of-band run state. */
typedef struct {
    unsigned spectralIndex; /**< Next coefficient position. */
    unsigned eob;           /**< Remaining EOB blocks. */
} AcProgress;
/** MCU, restart marker, and end-of-band state for a scan. */
typedef struct {
    unsigned unit;         /**< MCU position within the restart interval. */
    unsigned restartIndex; /**< Next restart marker number. */
    unsigned eob;          /**< Remaining progressive EOB blocks. */
} ScanProgress;
/** Bounded marker payload parsed from the JPEG stream. */
typedef struct {
    int marker;                /**< JPEG marker code. */
    const unsigned char *body; /**< Marker payload start. */
    size_t payloadLength;      /**< Marker payload length in bytes. */
} JpegSegment;

/*//////////////////////////////////////////////////
DECODING
//////////////////////////////////////////////////*/

/* ENTROPY*/
/**
 * Read a big-endian 16-bit JPEG marker or segment value.
 * @param bytes Two JPEG bytes in big-endian order.
 * @return The unsigned 16-bit value represented by the bytes.
 * @author kai
 */
static unsigned j16(const unsigned char *bytes) {
    return ((unsigned)bytes[0] << (unsigned)JPEG_ENTROPY_BYTE_BITS) | (unsigned)bytes[1];
}
/**
 * Read entropy-coded JPEG bits and reject invalid byte stuffing.
 * @param decoder Decoder state holding the entropy stream and bit cache.
 * @param n Number of entropy bits to consume.
 * @return The next n bits, or zero while marking decoder state invalid on truncation.
 * @author kai
 */
static unsigned jbits(Jpeg *decoder, unsigned n) {
    unsigned value = 0;
    while (n--) {
        if (!decoder->count) {
            unsigned byte;
            if (decoder->at >= decoder->n) {
                decoder->bad = 1;
                return 0;
            }
            byte = decoder->p[decoder->at++];
            if (byte == JPEG_SAMPLE_MAX) {
                unsigned stuffedByte;
                if (decoder->at >= decoder->n) {
                    decoder->bad = 1;
                    return 0;
                }
                stuffedByte = decoder->p[decoder->at++];
                if (stuffedByte != 0) {
                    decoder->bad = 1;
                    return 0;
                }
            }
            decoder->bits = byte;
            decoder->count = JPEG_ENTROPY_BYTE_BITS;
        }
        value = (value << 1U) | ((decoder->bits >> (--decoder->count)) & 1U);
    }
    return value;
}
/**
 * Decode one symbol from a JPEG canonical Huffman table.
 * @param decoder JPEG decoder state used to read the code bits.
 * @param table Canonical JPEG table for this symbol class.
 * @return Decoded symbol; an invalid code marks the decoder state bad.
 * @author kai
 */
static int jhuff(Jpeg *decoder, const JHuff *table) {
    unsigned code = 0;
    unsigned first = 0;
    unsigned index = 0;
    unsigned codeLength;
    /* An unregistered table has zero counts and cannot match any code in this bounded walk.*/
    for (codeLength = 1; codeLength <= JPEG_HUFFMAN_MAX_CODE_LENGTH; codeLength++) {
        code = (code << 1U) | jbits(decoder, 1);
        if (decoder->bad) {
            return 0;
        }
        if (code >= first && code - first < table->count[codeLength]) {
            return (int)table->symbol[index + code - first];
        }
        index += table->count[codeLength];
        first = (first + table->count[codeLength]) << 1U;
    }
    decoder->bad = 1;
    return 0;
}
/**
 * Read and sign-extend a JPEG magnitude value.
 * @param decoder Decoder state supplying the magnitude bits.
 * @param n Magnitude bit count from the Huffman symbol.
 * @return The JPEG sign-extended magnitude, or zero when the count is unsupported.
 * @author kai
 */
static int jreceive(Jpeg *decoder, unsigned n) {
    unsigned bits;
    if (n > JPEG_HUFFMAN_MAX_CODE_LENGTH) {
        decoder->bad = 1;
        return 0;
    }
    bits = jbits(decoder, n);
    return n && bits < (1U << (n - 1)) ? (int)bits - (int)((1U << n) - 1) : (int)bits;
}
/**
 * Read the next JPEG marker, skipping marker fill bytes.
 * @param decoder Decoder state positioned at the next marker prefix.
 * @return Marker byte, or -1 for a missing or truncated marker.
 * @author kai
 */
static int nextMarker(Jpeg *decoder) {
    unsigned marker;
    if (decoder->at >= decoder->n) {
        return -1;
    }
    if (decoder->p[decoder->at++] != JPEG_SAMPLE_MAX) {
        return -1;
    }
    do {
        if (decoder->at >= decoder->n) {
            return -1;
        }
        marker = decoder->p[decoder->at++];
    } while (marker == JPEG_SAMPLE_MAX);
    return (int)marker;
}

/* METADATA*/
/**
 * Read a TIFF 16-bit value in the byte order declared by EXIF.
 * @param bytes Two TIFF bytes to interpret.
 * @param littleEndian Nonzero selects little-endian byte order.
 * @return The unsigned 16-bit TIFF value.
 * @author kai
 */
static unsigned ex16(const unsigned char *bytes, int littleEndian) {
    return littleEndian
               ? ((unsigned)bytes[1] << (unsigned)JPEG_ENTROPY_BYTE_BITS) | (unsigned)bytes[0]
               : j16(bytes);
}
/**
 * Read a TIFF 32-bit value in the byte order declared by EXIF.
 * @param bytes Four TIFF bytes to interpret.
 * @param littleEndian Nonzero selects little-endian byte order.
 * @return The unsigned 32-bit TIFF value.
 * @author kai
 */
static unsigned long ex32(const unsigned char *bytes, int littleEndian) {
    return littleEndian ? (unsigned long)ex16(bytes, 1) |
                              ((unsigned long)ex16(bytes + 2, 1) << (unsigned)JPEG_WORD_SHIFT_BITS)
                        : ((unsigned long)ex16(bytes, 0) << (unsigned)JPEG_WORD_SHIFT_BITS) |
                              ex16(bytes + 2, 0);
}
/**
 * Read a valid EXIF orientation tag; default to normal orientation when absent or malformed.
 * @param payload APP1 payload beginning with an EXIF header.
 * @param n Available payload length, used to bound TIFF offsets.
 * @return EXIF orientation from one through eight, defaulting to one when absent or invalid.
 * @author kai
 */
static unsigned exifOrientation(const unsigned char *payload, size_t n) {
    unsigned long off;
    unsigned count;
    unsigned entryIndex;
    int littleEndian;
    if (n < JPEG_EXIF_MINIMUM_BYTES || memcmp(payload, "Exif\0\0", JPEG_EXIF_HEADER_BYTES) != 0) {
        return 1;
    }
    payload += JPEG_EXIF_HEADER_BYTES;
    n -= JPEG_EXIF_HEADER_BYTES;
    littleEndian = payload[0] == 'I' && payload[1] == 'I';
    if (!littleEndian && !(payload[0] == 'M' && payload[1] == 'M')) {
        return 1;
    }
    if (ex16(payload + 2, littleEndian) != JPEG_EXIF_TIFF_MAGIC) {
        return 1;
    }
    off = ex32(payload + 4, littleEndian);
    if (off > n - 2) {
        return 1;
    }
    count = ex16(payload + off, littleEndian);
    if (count > (n - off - 2) / JPEG_EXIF_ENTRY_BYTES) {
        return 1;
    }
    for (entryIndex = 0; entryIndex < count; entryIndex++) {
        const unsigned char *tag = payload + off + 2 + ((size_t)entryIndex * JPEG_EXIF_ENTRY_BYTES);
        if (ex16(tag, littleEndian) == JPEG_EXIF_ORIENTATION_TAG &&
            ex16(tag + 2, littleEndian) == JPEG_EXIF_SHORT_TYPE &&
            ex32(tag + 4, littleEndian) == 1) {
            unsigned orientation =
                ex16(tag + JPEG_EXIF_OFFSET_BASE + JPEG_EXIF_SHORT_TYPE, littleEndian);
            return orientation >= 1 && orientation <= JPEG_MAX_ORIENTATION ? orientation : 1;
        }
    }
    return 1;
}

/* COEFFICIENTS*/
/**
 * Validate and store JPEG frame dimensions and component count.
 * @param decoder JPEG state receiving basic frame metadata.
 * @param payload Frame segment payload.
 * @param length Payload length from the marker.
 * @return Nonzero when dimensions and component count are supported.
 * @author kai
 */
static int frameGeometry(Jpeg *decoder, const unsigned char *payload, size_t length) {
    if (length < JPEG_FRAME_HEADER_BYTES || payload[0] != JPEG_ENTROPY_BYTE_BITS || decoder->nc) {
        return 0;
    }
    decoder->h = j16(payload + 1);
    decoder->w = j16(payload + 3);
    decoder->nc = payload[JPEG_FRAME_COMPONENT_COUNT_OFFSET];
    return decoder->w && decoder->h && (size_t)decoder->w <= RC_PIXELS / decoder->h &&
           (decoder->nc == 1 || decoder->nc == 3) &&
           length == JPEG_FRAME_HEADER_BYTES + (decoder->nc * JPEG_FRAME_COMPONENT_BYTES);
}

/**
 * Parse JPEG frame dimensions, sampling factors, and quantization table references.
 * @param decoder JPEG state receiving frame metadata.
 * @param payload Frame segment payload.
 * @param n Payload length from the marker.
 * @return Nonzero when frame geometry and bounded coefficient planes are valid.
 * @author kai
 */
static int frame(Jpeg *decoder, const unsigned char *payload, size_t n) {
    unsigned componentIndex;

    /* VALIDATE FRAME GEOMETRY*/
    if (!frameGeometry(decoder, payload, n)) {
        return 0;
    }

    /* REGISTER COMPONENT SAMPLING*/
    /* Sampling factors determine the MCU grid and each padded coefficient plane.*/
    for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
        JComp *component = &decoder->c[componentIndex];
        unsigned priorComponentIndex;
        component->id =
            payload[JPEG_COMPONENT_ID_OFFSET + (componentIndex * JPEG_FRAME_COMPONENT_BYTES)];
        component->h = (unsigned)payload[JPEG_COMPONENT_SAMPLING_OFFSET +
                                         (componentIndex * JPEG_FRAME_COMPONENT_BYTES)] >>
                       (unsigned)JPEG_SAMPLING_NIBBLE_SHIFT;
        component->v = (unsigned)payload[JPEG_COMPONENT_SAMPLING_OFFSET +
                                         (componentIndex * JPEG_FRAME_COMPONENT_BYTES)] &
                       (unsigned)JPEG_NIBBLE_MASK;
        component->q = payload[JPEG_COMPONENT_QUANTIZATION_OFFSET +
                               (componentIndex * JPEG_FRAME_COMPONENT_BYTES)];
        if (!component->h || !component->v || component->h > 4 || component->v > 4 ||
            component->q > 3) {
            return 0;
        }
        for (priorComponentIndex = 0; priorComponentIndex < componentIndex; priorComponentIndex++) {
            if (decoder->c[priorComponentIndex].id == component->id) {
                return 0;
            }
        }
        if (component->h > decoder->mh) {
            decoder->mh = component->h;
        }
        if (component->v > decoder->mv) {
            decoder->mv = component->v;
        }
        for (priorComponentIndex = 0; priorComponentIndex < JPEG_BLOCK_COEFFICIENTS;
             priorComponentIndex++) {
            component->last[priorComponentIndex] = -1;
        }
    }
    decoder->mx =
        (decoder->w + (JPEG_BLOCK_SIDE * decoder->mh) - 1) / (JPEG_BLOCK_SIDE * decoder->mh);
    decoder->my =
        (decoder->h + (JPEG_BLOCK_SIDE * decoder->mv) - 1) / (JPEG_BLOCK_SIDE * decoder->mv);
    {
        /* ALLOCATE BOUNDED COEFFICIENT PLANES*/
        /* Track the combined allocation so component padding cannot bypass the memory limit.*/
        size_t total = 0;
        unsigned blocks = 0;
        for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
            JComp *component = &decoder->c[componentIndex];
            size_t count;
            component->bx = decoder->mx * component->h;
            component->by = decoder->my * component->v;
            component->actualX =
                ((decoder->w * component->h) + (JPEG_BLOCK_SIDE * decoder->mh) - 1) /
                (JPEG_BLOCK_SIDE * decoder->mh);
            component->actualY =
                ((decoder->h * component->v) + (JPEG_BLOCK_SIDE * decoder->mv) - 1) /
                (JPEG_BLOCK_SIDE * decoder->mv);
            count = (size_t)component->bx * component->by * JPEG_BLOCK_COEFFICIENTS;
            if (count > RC_LIMIT / sizeof(int) || total > RC_LIMIT - (count * sizeof(int))) {
                return 0;
            }
            total += count * sizeof(int);
            blocks += component->h * component->v;
            component->coef = calloc(count, sizeof(int));
            if (!component->coef) {
                return 0;
            }
        }
        if (blocks > JPEG_MAX_MCU_BLOCKS) {
            return 0;
        }
    }
    return 1;
}
/**
 * Apply one progressive AC refinement bit to an already nonzero coefficient.
 * @param decoder Progressive JPEG decoder state.
 * @param coefficient Component whose coefficient block is being refined.
 * @param bit Refinement bit to apply at the current spectral position.
 * @return Nonzero when the refinement bits are valid.
 * @author kai
 */
static int refineNonzero(Jpeg *decoder, int *coefficient, unsigned bit) {
    if (jbits(decoder, 1) && ((unsigned)abs(*coefficient) & bit) == 0) {
        *coefficient += *coefficient > 0 ? (int)bit : -(int)bit;
    }
    return !decoder->bad;
}

/**
 * Decode an initial JPEG AC spectral band.
 * @param decoder Decoder state for entropy input.
 * @param block Block receiving decoded coefficients.
 * @param scan AC table and spectral range.
 * @param bit Current progressive bit plane.
 * @param progress Spectral position and end-of-band run state.
 * @return Nonzero when the spectral band decodes successfully.
 * @author kai
 */
static int decodeInitialAc(Jpeg *decoder, int *block, BlockScan scan, unsigned bit,
                           AcProgress *progress) {
    if (progress->eob) {
        progress->eob--;
        return 1;
    }
    while (progress->spectralIndex <= scan.spectralEnd && !decoder->bad) {
        int symbol = jhuff(decoder, &decoder->tables[1][scan.acTable]);
        unsigned run = (unsigned)symbol >> (unsigned)JPEG_SAMPLING_NIBBLE_SHIFT;
        unsigned category = (unsigned)symbol & (unsigned)JPEG_NIBBLE_MASK;
        if (!category) {
            if (run == JPEG_NIBBLE_MASK) {
                progress->spectralIndex += JPEG_AC_ZERO_RUN_LENGTH;
                if (progress->spectralIndex > scan.spectralEnd + 1) {
                    return 0;
                }
                continue;
            }
            if (decoder->progressive) {
                progress->eob = (1U << run) + jbits(decoder, run) - 1;
            }
            return !decoder->bad;
        }
        if (category > JPEG_BASELINE_MAX_AC_CATEGORY ||
            progress->spectralIndex + run > scan.spectralEnd) {
            return 0;
        }
        progress->spectralIndex += run;
        block[zig[progress->spectralIndex++]] = jreceive(decoder, category) * (int)bit;
    }
    return !decoder->bad;
}

/**
 * Apply one progressive JPEG AC refinement symbol.
 * @param decoder Decoder state for entropy input.
 * @param block Block whose coefficients are refined.
 * @param scan AC table and spectral range.
 * @param bit Current progressive bit plane.
 * @param progress Spectral position and end-of-band run state.
 * @return Nonzero when the refinement symbol is valid.
 * @author kai
 */
static int refineAcSymbol(Jpeg *decoder, int *block, BlockScan scan, unsigned bit,
                          AcProgress *progress) {
    int symbol = jhuff(decoder, &decoder->tables[1][scan.acTable]);
    int value = 0;
    unsigned run = (unsigned)symbol >> (unsigned)JPEG_SAMPLING_NIBBLE_SHIFT;
    unsigned category = (unsigned)symbol & (unsigned)JPEG_NIBBLE_MASK;
    if (category) {
        if (category != 1) {
            return 0;
        }
        value = jbits(decoder, 1) ? (int)bit : -(int)bit;
    } else if (run != JPEG_NIBBLE_MASK) {
        progress->eob = (1U << run) + jbits(decoder, run);
        return !decoder->bad;
    } else {
        run = JPEG_AC_ZERO_RUN_LENGTH;
    }

    while (progress->spectralIndex <= scan.spectralEnd) {
        int *coefficient = &block[zig[progress->spectralIndex]];
        if (*coefficient) {
            if (!refineNonzero(decoder, coefficient, bit)) {
                return 0;
            }
        } else if (!run) {
            if (value) {
                *coefficient = value;
                progress->spectralIndex++;
            }
            break;
        } else {
            run--;
        }
        progress->spectralIndex++;
    }
    return !run && !(value && progress->spectralIndex > scan.spectralEnd + 1);
}

/**
 * Decode or refine the DC coefficient for one JPEG block.
 * @param decoder Decoder state for entropy input.
 * @param component Component state holding the DC predictor.
 * @param block Block receiving the coefficient.
 * @param scan Table selector and approximation information.
 * @param bit Current progressive bit plane.
 * @return Nonzero when the DC value is valid.
 * @author kai
 */
static int decodeDc(Jpeg *decoder, JComp *component, int *block, BlockScan scan, unsigned bit) {
    if (!scan.spectralStart) {
        if (!scan.previousApproximation) {
            int category = jhuff(decoder, &decoder->tables[0][scan.dcTable]);
            int delta;
            if (category < 0 || category > JPEG_MAX_DC_CATEGORY) {
                return 0;
            }
            delta = jreceive(decoder, (unsigned)category);
            if (component->dc > JPEG_COEFFICIENT_LIMIT || component->dc < -JPEG_COEFFICIENT_LIMIT) {
                return 0;
            }
            component->dc += delta;
            if (component->dc > (int)(JPEG_COEFFICIENT_LIMIT / bit) ||
                component->dc < -(int)(JPEG_COEFFICIENT_LIMIT / bit)) {
                return 0;
            }
            block[0] = component->dc * (int)bit;
        } else if (jbits(decoder, 1)) {
            block[0] += block[0] >= 0 ? (int)bit : -(int)bit;
        }
    }
    return !decoder->bad;
}
/**
 * Decode one JPEG block for sequential or progressive scans.
 * @param decoder Decoder state for the current scan.
 * @param component Component and predictor state for this block.
 * @param block Receives the 64 quantized coefficients.
 * @param scan Table selectors and spectral approximation range.
 * @param eob Remaining end-of-band run for progressive AC data.
 * @return Nonzero when the block is decoded without invalid coefficients or truncated bits.
 * @author kai
 */
static int decodeBlock(Jpeg *decoder, JComp *component, int *block, BlockScan scan, unsigned *eob) {
    unsigned spectralIndex = scan.spectralStart;
    unsigned bit = 1U << scan.currentApproximation;

    /* DECODE OR REFINE THE DC VALUE*/
    /* DC coefficients are differences from the previous block in the same component.*/
    if (!scan.spectralStart) {
        if (!decodeDc(decoder, component, block, scan, bit)) {
            return 0;
        }
        spectralIndex = 1;
        if (!scan.spectralEnd) {
            return !decoder->bad;
        }
    }

    /* DECODE INITIAL AC COEFFICIENTS*/
    /* An EOB run can skip the remaining spectral band across several blocks.*/
    AcProgress progress = {spectralIndex, *eob};
    if (!scan.previousApproximation) {
        int success = decodeInitialAc(decoder, block, scan, bit, &progress);
        *eob = progress.eob;
        return success;
    }

    /* REFINE PROGRESSIVE AC COEFFICIENTS*/
    /* New coefficients consume a sign bit; existing nonzero values consume refinement bits.*/
    if (!progress.eob) {
        while (progress.spectralIndex <= scan.spectralEnd && !decoder->bad) {
            if (!refineAcSymbol(decoder, block, scan, bit, &progress)) {
                return 0;
            }
        }
    }

    /* REFINE THE REMAINDER OF AN EOB BLOCK*/
    if (progress.eob) {
        for (; progress.spectralIndex <= scan.spectralEnd; progress.spectralIndex++) {
            int *coefficient = &block[zig[progress.spectralIndex]];
            if (*coefficient && !refineNonzero(decoder, coefficient, bit)) {
                return 0;
            }
        }
        progress.eob--;
    }
    *eob = progress.eob;
    return !decoder->bad;
}
/**
 * Validate and populate one JPEG scan's component selectors and spectral range.
 * @param decoder JPEG state receiving approximation progression.
 * @param payload Scan segment header.
 * @param length Bytes in the scan segment.
 * @param config Receives selected component and MCU metadata.
 * @return Nonzero when the scan header is valid.
 * @author kai
 */
/**
 * Validate and map the component/table selectors in a scan header.
 * @param decoder JPEG frame state.
 * @param payload Scan segment header.
 * @param length Bytes in the scan segment.
 * @param config Receives selected component and Huffman table indices.
 * @return Nonzero when all selectors refer to distinct known components.
 * @author kai
 */
static int readScanComponents(Jpeg *decoder, const unsigned char *payload, size_t length,
                              ScanConfig *config) {
    unsigned scanComponentIndex;
    if (!decoder->nc || length < 4) {
        return 0;
    }
    config->componentCount = payload[0];
    if (!config->componentCount || config->componentCount > decoder->nc ||
        length != 1 + (config->componentCount * 2) + 3) {
        return 0;
    }
    for (scanComponentIndex = 0; scanComponentIndex < config->componentCount;
         scanComponentIndex++) {
        unsigned componentIndex;
        unsigned selectorOffset = 2 + (scanComponentIndex * 2);
        for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
            if (decoder->c[componentIndex].id == payload[selectorOffset - 1]) {
                break;
            }
        }
        if (componentIndex == decoder->nc) {
            return 0;
        }
        config->indices[scanComponentIndex] = componentIndex;
        config->dcTables[scanComponentIndex] =
            (unsigned)payload[selectorOffset] >> (unsigned)JPEG_SAMPLING_NIBBLE_SHIFT;
        config->acTables[scanComponentIndex] =
            (unsigned)payload[selectorOffset] & (unsigned)JPEG_NIBBLE_MASK;
        if (config->dcTables[scanComponentIndex] > 3 || config->acTables[scanComponentIndex] > 3) {
            return 0;
        }
        for (unsigned priorIndex = 0; priorIndex < scanComponentIndex; priorIndex++) {
            if (config->indices[priorIndex] == config->indices[scanComponentIndex]) {
                return 0;
            }
        }
        decoder->c[componentIndex].dc = 0;
    }
    return 1;
}

/**
 * Check sequential or progressive spectral selection rules.
 * @param decoder JPEG frame mode.
 * @param config Scan component count and spectral range.
 * @return Nonzero when the spectral band is legal for this scan mode.
 * @author kai
 */
static int validScanBand(const Jpeg *decoder, const ScanConfig *config) {
    const BlockScan *band = &config->band;
    if (band->spectralStart > band->spectralEnd || band->spectralEnd >= JPEG_BLOCK_COEFFICIENTS ||
        band->previousApproximation > JPEG_MAX_PROGRESSIVE_APPROXIMATION ||
        band->currentApproximation > JPEG_MAX_PROGRESSIVE_APPROXIMATION) {
        return 0;
    }
    if (decoder->progressive) {
        return !((!band->spectralStart && band->spectralEnd) ||
                 (band->spectralStart && config->componentCount != 1) ||
                 (band->previousApproximation &&
                  band->previousApproximation != band->currentApproximation + 1));
    }
    return !band->spectralStart && band->spectralEnd == JPEG_LAST_COEFFICIENT &&
           !band->previousApproximation && !band->currentApproximation;
}

/**
 * Validate prior approximation levels and record this scan's coefficient level.
 * @param decoder JPEG component approximation state.
 * @param config Validated scan components and spectral range.
 * @return Nonzero when all selected coefficients follow valid progression.
 * @author kai
 */
static int recordScanProgression(Jpeg *decoder, const ScanConfig *config) {
    unsigned scanComponentIndex;
    for (scanComponentIndex = 0; scanComponentIndex < config->componentCount;
         scanComponentIndex++) {
        JComp *component = &decoder->c[config->indices[scanComponentIndex]];
        unsigned coefficientIndex;
        for (coefficientIndex = config->band.spectralStart;
             coefficientIndex <= config->band.spectralEnd; coefficientIndex++) {
            if ((!config->band.previousApproximation && component->last[coefficientIndex] != -1) ||
                (config->band.previousApproximation &&
                 component->last[coefficientIndex] != (int)config->band.previousApproximation)) {
                return 0;
            }
            component->last[coefficientIndex] = (int)config->band.currentApproximation;
        }
    }
    return 1;
}

/**
 * Validate and populate one JPEG scan's component selectors and spectral range.
 * @param decoder JPEG state receiving approximation progression.
 * @param payload Scan segment header.
 * @param length Bytes in the scan segment.
 * @param config Receives selected component and MCU metadata.
 * @return Nonzero when the scan header is valid.
 * @author kai
 */
static int prepareScan(Jpeg *decoder, const unsigned char *payload, size_t length,
                       ScanConfig *config) {
    if (!readScanComponents(decoder, payload, length, config)) {
        return 0;
    }
    config->band.spectralStart = payload[1 + (config->componentCount * 2)];
    config->band.spectralEnd = payload[2 + (config->componentCount * 2)];
    config->band.previousApproximation =
        (unsigned)payload[3 + (config->componentCount * 2)] >> (unsigned)JPEG_SAMPLING_NIBBLE_SHIFT;
    config->band.currentApproximation =
        (unsigned)payload[3 + (config->componentCount * 2)] & (unsigned)JPEG_NIBBLE_MASK;
    if (!validScanBand(decoder, config) || !recordScanProgression(decoder, config)) {
        return 0;
    }
    config->mcuColumns =
        config->componentCount == 1 ? decoder->c[config->indices[0]].actualX : decoder->mx;
    config->mcuRows =
        config->componentCount == 1 ? decoder->c[config->indices[0]].actualY : decoder->my;
    return 1;
}

/**
 * Reset scan predictors when a restart marker is due.
 * @param decoder Entropy decoder with restart interval state.
 * @param progress Current MCU index, restart marker, and EOB run state.
 * @return Nonzero when no restart is due or the expected marker is present.
 * @author kai
 */
static int resetScanInterval(Jpeg *decoder, ScanProgress *progress) {
    unsigned componentIndex;
    int marker;
    if (!decoder->restart || progress->unit != decoder->restart) {
        return 1;
    }
    decoder->bits = 0;
    decoder->count = 0;
    marker = nextMarker(decoder);
    if (marker != JPEG_RESTART_MARKER_BASE +
                      (int)(progress->restartIndex & (unsigned)JPEG_RESTART_SEQUENCE_MASK)) {
        return 0;
    }
    progress->restartIndex++;
    progress->unit = 0;
    progress->eob = 0;
    for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
        decoder->c[componentIndex].dc = 0;
    }
    return 1;
}

/**
 * Decode all component blocks contained in one MCU.
 * @param decoder JPEG entropy decoder.
 * @param config Validated scan component and spectral metadata.
 * @param position MCU coordinates.
 * @param eob Progressive end-of-band run state.
 * @return Nonzero when every block in the MCU decodes successfully.
 * @author kai
 */
static int decodeMcu(Jpeg *decoder, const ScanConfig *config, McuPosition position, unsigned *eob) {
    unsigned scanComponentIndex;
    for (scanComponentIndex = 0; scanComponentIndex < config->componentCount;
         scanComponentIndex++) {
        JComp *component = &decoder->c[config->indices[scanComponentIndex]];
        unsigned horizontalSampling = config->componentCount == 1 ? 1 : component->h;
        unsigned verticalSampling = config->componentCount == 1 ? 1 : component->v;
        unsigned blockY;
        unsigned blockX;
        for (blockY = 0; blockY < verticalSampling; blockY++) {
            for (blockX = 0; blockX < horizontalSampling; blockX++) {
                unsigned sampleBlockX =
                    config->componentCount == 1 ? position.x : (position.x * component->h) + blockX;
                unsigned sampleBlockY =
                    config->componentCount == 1 ? position.y : (position.y * component->v) + blockY;
                int *block =
                    component->coef + ((((size_t)sampleBlockY * component->bx) + sampleBlockX) *
                                       JPEG_BLOCK_COEFFICIENTS);
                BlockScan blockScan = config->band;
                blockScan.dcTable = config->dcTables[scanComponentIndex];
                blockScan.acTable = config->acTables[scanComponentIndex];
                if (!decodeBlock(decoder, component, block, blockScan, eob)) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

/**
 * Decode a JPEG scan, including restart markers and progressive coefficient updates.
 * @param decoder JPEG state updated by the scan.
 * @param payload Scan segment payload and entropy-coded bytes.
 * @param n Bytes available in the scan segment.
 * @return Nonzero when all scan units and progressive runs decode successfully.
 * @author kai
 */
static int scan(Jpeg *decoder, const unsigned char *payload, size_t n) {
    ScanConfig config = {0};
    ScanProgress progress = {0};
    unsigned mcuY;

    if (!prepareScan(decoder, payload, n, &config)) {
        return 0;
    }
    decoder->bits = 0;
    decoder->count = 0;
    for (mcuY = 0; mcuY < config.mcuRows; mcuY++) {
        unsigned mcuX;
        for (mcuX = 0; mcuX < config.mcuColumns; mcuX++) {
            if (!resetScanInterval(decoder, &progress) ||
                !decodeMcu(decoder, &config, (McuPosition){mcuX, mcuY}, &progress.eob)) {
                return 0;
            }
            progress.unit++;
        }
    }
    decoder->bits = 0;
    decoder->count = 0;
    return !decoder->bad && !progress.eob;
}

/*//////////////////////////////////////////////////
RECONSTRUCTION
//////////////////////////////////////////////////*/

/* SAMPLE LIMITS*/
/**
 * Round and clamp a component value to the 8-bit channel range.
 * @param value Floating-point channel value before 8-bit clamping.
 * @return Rounded channel value clamped to the inclusive 0–255 range.
 * @author kai
 */
static unsigned char clampByte(double value) {
    if (value <= 0) {
        return 0;
    }
    if (value >= JPEG_SAMPLE_MAX) {
        return JPEG_SAMPLE_MAX;
    }
    return (unsigned char)floor(value + JPEG_HALF_SAMPLE);
}

/**
 * Build the shared cosine basis for the 8 by 8 inverse transform.
 * @param basis Receives normalized frequency weights.
 * @author kai
 */
static void buildIdctBasis(double basis[JPEG_BLOCK_SIDE][JPEG_BLOCK_SIDE]) {
    unsigned pixelX;
    unsigned horizontalFrequency;
    for (pixelX = 0; pixelX < JPEG_BLOCK_SIDE; pixelX++) {
        for (horizontalFrequency = 0; horizontalFrequency < JPEG_BLOCK_SIDE;
             horizontalFrequency++) {
            basis[pixelX][horizontalFrequency] =
                (horizontalFrequency ? 1.0 : JPEG_IDCT_NORMALIZATION) *
                cos(((JPEG_TWO_SAMPLES * pixelX) + 1) * horizontalFrequency * JPEG_PI /
                    (JPEG_TWO_SAMPLES * JPEG_BLOCK_SIDE));
        }
    }
}

/**
 * Check whether a block has any AC coefficients.
 * @param coefficients Quantized coefficient block.
 * @return Nonzero when an AC coefficient is nonzero.
 * @author kai
 */
static int hasAcCoefficients(const int coefficients[JPEG_BLOCK_COEFFICIENTS]) {
    unsigned frequencyIndex;
    for (frequencyIndex = 1; frequencyIndex < JPEG_BLOCK_COEFFICIENTS; frequencyIndex++) {
        if (coefficients[frequencyIndex]) {
            return 1;
        }
    }
    return 0;
}

/**
 * Fill the reconstructed sample block for an AC-free DC coefficient.
 * @param component JPEG component holding the sample plane and quantizer.
 * @param blockX Horizontal block index.
 * @param blockY Vertical block index.
 * @param coefficients Quantized coefficient block.
 * @param quant Component quantization table.
 * @param stride Sample-plane row stride.
 * @author kai
 */
static void renderDcBlock(JComp *component, unsigned blockX, unsigned blockY,
                          const int coefficients[JPEG_BLOCK_COEFFICIENTS],
                          const unsigned quant[JPEG_BLOCK_COEFFICIENTS], unsigned stride) {
    unsigned pixelY;
    unsigned char value = clampByte(((double)coefficients[0] * quant[0] / JPEG_IDCT_DC_SCALE) +
                                    JPEG_TRANSFORM_CENTER);
    for (pixelY = 0; pixelY < JPEG_BLOCK_SIDE; pixelY++) {
        memset(component->samples + ((((size_t)blockY * JPEG_BLOCK_SIDE) + pixelY) * stride) +
                   ((size_t)blockX * JPEG_BLOCK_SIDE),
               value, JPEG_BLOCK_SIDE);
    }
}

/**
 * Apply the two inverse-transform passes for one JPEG block.
 * @param component JPEG component receiving reconstructed samples.
 * @param blockX Horizontal block index.
 * @param blockY Vertical block index.
 * @param coefficients Quantized coefficient block.
 * @param quant Component quantization table.
 * @param stride Sample-plane row stride.
 * @param basis Shared inverse-transform frequency weights.
 * @author kai
 */
static void renderAcBlock(JComp *component, unsigned blockX, unsigned blockY,
                          const int coefficients[JPEG_BLOCK_COEFFICIENTS],
                          const unsigned quant[JPEG_BLOCK_COEFFICIENTS], unsigned stride,
                          const double basis[JPEG_BLOCK_SIDE][JPEG_BLOCK_SIDE]) {
    double temporary[JPEG_BLOCK_SIDE][JPEG_BLOCK_SIDE];
    unsigned verticalFrequency;
    unsigned pixelX;
    unsigned pixelY;
    unsigned horizontalFrequency;
    for (verticalFrequency = 0; verticalFrequency < JPEG_BLOCK_SIDE; verticalFrequency++) {
        for (pixelX = 0; pixelX < JPEG_BLOCK_SIDE; pixelX++) {
            double sum = 0;
            for (horizontalFrequency = 0; horizontalFrequency < JPEG_BLOCK_SIDE;
                 horizontalFrequency++) {
                unsigned coefficientIndex =
                    (verticalFrequency * JPEG_BLOCK_SIDE) + horizontalFrequency;
                sum += basis[pixelX][horizontalFrequency] * coefficients[coefficientIndex] *
                       quant[coefficientIndex];
            }
            temporary[verticalFrequency][pixelX] = sum;
        }
    }
    for (pixelY = 0; pixelY < JPEG_BLOCK_SIDE; pixelY++) {
        for (pixelX = 0; pixelX < JPEG_BLOCK_SIDE; pixelX++) {
            double sum = 0;
            for (verticalFrequency = 0; verticalFrequency < JPEG_BLOCK_SIDE; verticalFrequency++) {
                sum += basis[pixelY][verticalFrequency] * temporary[verticalFrequency][pixelX];
            }
            component->samples[((((size_t)blockY * JPEG_BLOCK_SIDE) + pixelY) * stride) +
                               ((size_t)blockX * JPEG_BLOCK_SIDE) + pixelX] =
                clampByte((sum / JPEG_TRANSFORM_DIVISOR) + JPEG_TRANSFORM_CENTER);
        }
    }
}

/**
 * Reconstruct all blocks for one JPEG component.
 * @param decoder JPEG quantization tables.
 * @param component Component whose coefficient and sample planes are owned by the decoder.
 * @param basis Shared inverse-transform frequency weights.
 * @return Nonzero when the component has valid coefficients and samples are reconstructed.
 * @author kai
 */
static int renderComponent(Jpeg *decoder, JComp *component,
                           const double basis[JPEG_BLOCK_SIDE][JPEG_BLOCK_SIDE]) {
    unsigned blockX;
    unsigned blockY;
    unsigned stride = component->bx * JPEG_BLOCK_SIDE;
    const unsigned *quant = decoder->quant[component->q];
    if (component->last[0] < 0 || !quant[0]) {
        return 0;
    }
    component->samples = malloc((size_t)component->bx * component->by * JPEG_BLOCK_COEFFICIENTS);
    if (!component->samples) {
        return 0;
    }
    for (blockY = 0; blockY < component->by; blockY++) {
        for (blockX = 0; blockX < component->bx; blockX++) {
            int *coefficients = component->coef + ((((size_t)blockY * component->bx) + blockX) *
                                                   JPEG_BLOCK_COEFFICIENTS);
            if (hasAcCoefficients(coefficients)) {
                renderAcBlock(component, blockX, blockY, coefficients, quant, stride, basis);
            } else {
                renderDcBlock(component, blockX, blockY, coefficients, quant, stride);
            }
        }
    }
    return 1;
}

/* INVERSE TRANSFORM*/
/**
 * Bilinearly sample one component at the centre of an output pixel.
 * @param decoder JPEG geometry and maximum sampling factors.
 * @param component Component sample plane and sampling factors.
 * @param position Output pixel coordinates.
 * @return Interpolated component value.
 * @author kai
 */
static double sampleComponent(const Jpeg *decoder, const JComp *component, PixelPosition position) {
    double fractionX =
        ((position.x + JPEG_HALF_SAMPLE) * component->h / decoder->mh) - JPEG_HALF_SAMPLE;
    double fractionY =
        ((position.y + JPEG_HALF_SAMPLE) * component->v / decoder->mv) - JPEG_HALF_SAMPLE;
    unsigned sampleWidth = ((decoder->w * component->h) + decoder->mh - 1) / decoder->mh;
    unsigned sampleHeight = ((decoder->h * component->v) + decoder->mv - 1) / decoder->mv;
    unsigned sampleX0;
    unsigned sampleY0;
    unsigned sampleX1;
    unsigned sampleY1;
    unsigned stride = component->bx * JPEG_BLOCK_SIDE;
    double fractionalX;
    double fractionalY;
    if (fractionX < 0) {
        fractionX = 0;
    }
    if (fractionY < 0) {
        fractionY = 0;
    }
    if (fractionX > sampleWidth - 1) {
        fractionX = sampleWidth - 1;
    }
    if (fractionY > sampleHeight - 1) {
        fractionY = sampleHeight - 1;
    }
    sampleX0 = (unsigned)fractionX;
    sampleY0 = (unsigned)fractionY;
    sampleX1 = sampleX0 + 1 < sampleWidth ? sampleX0 + 1 : sampleX0;
    sampleY1 = sampleY0 + 1 < sampleHeight ? sampleY0 + 1 : sampleY0;
    fractionalX = fractionX - sampleX0;
    fractionalY = fractionY - sampleY0;
    return (((component->samples[((size_t)sampleY0 * stride) + sampleX0] * (1 - fractionalX)) +
             (component->samples[((size_t)sampleY0 * stride) + sampleX1] * fractionalX)) *
            (1 - fractionalY)) +
           (((component->samples[((size_t)sampleY1 * stride) + sampleX0] * (1 - fractionalX)) +
             (component->samples[((size_t)sampleY1 * stride) + sampleX1] * fractionalX)) *
            fractionalY);
}

/**
 * Convert sampled component values to one opaque RGBA output pixel.
 * @param decoder JPEG component planes and color transform metadata.
 * @param image Destination RGBA image.
 * @param position Output pixel coordinates.
 * @param direct Nonzero when component values are grayscale or direct RGB.
 * @author kai
 */
static void renderPixel(const Jpeg *decoder, RcImage *image, PixelPosition position, int direct) {
    double values[JPEG_CHANNEL_COUNT] = {
        JPEG_TRANSFORM_CENTER,
        JPEG_TRANSFORM_CENTER,
        JPEG_TRANSFORM_CENTER,
    };
    unsigned char *pixel =
        image->rgba + ((((size_t)position.y * decoder->w) + position.x) * JPEG_PIXEL_CHANNELS);
    unsigned componentIndex;
    for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
        values[componentIndex] = sampleComponent(decoder, &decoder->c[componentIndex], position);
    }
    if (decoder->nc == 1) {
        pixel[0] = pixel[1] = pixel[2] = clampByte(values[0]);
    } else if (direct) {
        for (componentIndex = 0; componentIndex < JPEG_CHANNEL_COUNT; componentIndex++) {
            pixel[componentIndex] = clampByte(values[componentIndex]);
        }
    } else {
        double blueDifference = values[1] - JPEG_TRANSFORM_CENTER;
        double redDifference = values[2] - JPEG_TRANSFORM_CENTER;
        pixel[0] = clampByte(values[0] + (JPEG_YCBCR_RED_SCALE * redDifference));
        pixel[1] = clampByte(values[0] - (JPEG_YCBCR_GREEN_BLUE_SCALE * blueDifference) -
                             (JPEG_YCBCR_GREEN_RED_SCALE * redDifference));
        pixel[2] = clampByte(values[0] + (JPEG_YCBCR_BLUE_SCALE * blueDifference));
    }
    pixel[JPEG_PIXEL_CHANNELS - 1] = JPEG_SAMPLE_MAX;
}

/**
 * Convert decoded JPEG component planes to straight-alpha RGBA8 pixels.
 * @param decoder Decoded component planes and sampling metadata.
 * @param image Receives the rendered RGBA8 image.
 * @return Nonzero when all component planes render into a complete image.
 * @author kai
 */
static int render(Jpeg *decoder, RcImage *image) {
    double basis[JPEG_BLOCK_SIDE][JPEG_BLOCK_SIDE];
    unsigned componentIndex;
    unsigned pixelX;
    unsigned pixelY;
    int direct = decoder->rgb || (decoder->nc == 3 && decoder->c[0].id == 'R' &&
                                  decoder->c[1].id == 'G' && decoder->c[2].id == 'B');
    buildIdctBasis(basis);
    for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
        if (!renderComponent(decoder, &decoder->c[componentIndex], basis)) {
            return 0;
        }
    }
    if (!decoder->w || !decoder->h) {
        return 0;
    }
    image->rgba = malloc((size_t)decoder->w * decoder->h * JPEG_PIXEL_CHANNELS);
    if (!image->rgba) {
        return 0;
    }
    image->width = decoder->w;
    image->height = decoder->h;
    for (pixelY = 0; pixelY < decoder->h; pixelY++) {
        for (pixelX = 0; pixelX < decoder->w; pixelX++) {
            renderPixel(decoder, image, (PixelPosition){pixelX, pixelY}, direct);
        }
    }
    return 1;
}

/* ORIENTATION*/
/**
 * Apply EXIF orientation by rearranging decoded pixels and dimensions.
 * @param image Owned image whose pixels and dimensions are transformed.
 * @param orientation EXIF orientation value from one through eight.
 * @return Nonzero when orientation is applied or already normal; zero on allocation failure.
 * @author kai
 */
static int orientImage(RcImage *image, unsigned orientation) {
    unsigned pixelX;
    unsigned pixelY;
    unsigned sourceWidth = image->width;
    unsigned sourceHeight = image->height;
    unsigned targetWidth =
        orientation >= JPEG_ORIENTATION_TRANSPOSE_START ? sourceHeight : sourceWidth;
    unsigned targetHeight =
        orientation >= JPEG_ORIENTATION_TRANSPOSE_START ? sourceWidth : sourceHeight;
    unsigned char *out;

    /* PREPARE ORIENTATION REMAPPING*/
    if (orientation == 1) {
        return 1;
    }
    out = malloc((size_t)sourceWidth * sourceHeight * 4);
    if (!out) {
        return 0;
    }

    /* MAP SOURCE PIXELS TO THE NEW GRID*/
    /* Transpose first; bits zero and one then reflect destination x and y respectively.*/
    for (pixelY = 0; pixelY < sourceHeight; pixelY++) {
        for (pixelX = 0; pixelX < sourceWidth; pixelX++) {
            static const unsigned char reflections[9] = {0, 0, 1, 3, 2, 0, 1, 3, 2};
            unsigned targetX = orientation >= JPEG_ORIENTATION_TRANSPOSE_START ? pixelY : pixelX;
            unsigned targetY = orientation >= JPEG_ORIENTATION_TRANSPOSE_START ? pixelX : pixelY;
            if ((unsigned)reflections[orientation] & 1U) {
                targetX = targetWidth - 1 - targetX;
            }
            if ((unsigned)reflections[orientation] & 2U) {
                targetY = targetHeight - 1 - targetY;
            }
            memcpy(out + ((((size_t)targetY * targetWidth) + targetX) * 4),
                   image->rgba + ((((size_t)pixelY * sourceWidth) + pixelX) * 4), 4);
        }
    }
    free(image->rgba);
    image->rgba = out;
    image->width = targetWidth;
    image->height = targetHeight;
    return 1;
}

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* JPEG INPUT*/
/**
 * Read all quantization tables in one marker payload.
 * @param decoder JPEG state receiving quantization values.
 * @param body Marker payload.
 * @param payloadLength Payload length in bytes.
 * @return Nonzero when every table entry is present and nonzero.
 * @author kai
 */
static int readQuantizationTables(Jpeg *decoder, const unsigned char *body, size_t payloadLength) {
    size_t payloadOffset = 0;
    while (payloadOffset < payloadLength) {
        unsigned spec = body[payloadOffset++];
        unsigned tableId = spec & (unsigned)JPEG_NIBBLE_MASK;
        unsigned wide = spec >> (unsigned)JPEG_SAMPLING_NIBBLE_SHIFT;
        unsigned coefficientIndex;
        if (tableId > 3 || wide > 1 ||
            (wide ? JPEG_QUANTIZATION_WIDE_BYTES : JPEG_QUANTIZATION_NARROW_BYTES) >
                payloadLength - payloadOffset) {
            return 0;
        }
        for (coefficientIndex = 0; coefficientIndex < JPEG_BLOCK_COEFFICIENTS; coefficientIndex++) {
            unsigned value = wide ? j16(body + payloadOffset + ((size_t)coefficientIndex * 2))
                                  : body[payloadOffset + coefficientIndex];
            if (!value) {
                return 0;
            }
            decoder->quant[tableId][zig[coefficientIndex]] = value;
        }
        payloadOffset += wide ? JPEG_QUANTIZATION_WIDE_BYTES : JPEG_QUANTIZATION_NARROW_BYTES;
    }
    return 1;
}

/**
 * Read canonical Huffman tables in one marker payload.
 * @param decoder JPEG state receiving Huffman tables.
 * @param body Marker payload.
 * @param payloadLength Payload length in bytes.
 * @return Nonzero when every canonical table is bounded and valid.
 * @author kai
 */
static int readHuffmanTables(Jpeg *decoder, const unsigned char *body, size_t payloadLength) {
    size_t payloadOffset = 0;
    while (payloadOffset < payloadLength) {
        unsigned spec = body[payloadOffset++];
        unsigned kind = spec >> (unsigned)JPEG_SAMPLING_NIBBLE_SHIFT;
        unsigned tableId = spec & (unsigned)JPEG_NIBBLE_MASK;
        unsigned symbolIndex;
        unsigned total = 0;
        int left = 1;
        JHuff *table;
        if (kind > 1 || tableId > 3 || payloadLength - payloadOffset < JPEG_HUFFMAN_COUNT_BYTES) {
            return 0;
        }
        table = &decoder->tables[kind][tableId];
        memset(table, 0, sizeof(*table));
        for (symbolIndex = 1; symbolIndex <= JPEG_HUFFMAN_MAX_CODE_LENGTH; symbolIndex++) {
            table->count[symbolIndex] = body[payloadOffset++];
            total += table->count[symbolIndex];
            left = (left * 2) - (int)table->count[symbolIndex];
            if (left < 0) {
                return 0;
            }
        }
        if (!total || total > JPEG_HUFFMAN_SYMBOL_CAPACITY ||
            total > payloadLength - payloadOffset) {
            return 0;
        }
        for (symbolIndex = 0; symbolIndex < total; symbolIndex++) {
            table->symbol[symbolIndex] = body[payloadOffset++];
        }
    }
    return 1;
}

/**
 * Process one non-standalone JPEG marker payload.
 * @param decoder JPEG state updated by the marker.
 * @param marker Marker code.
 * @param body Marker payload.
 * @param payloadLength Payload length in bytes.
 * @return Nonzero when the marker is supported and valid or safely ignored.
 * @author kai
 */
static int processMarker(Jpeg *decoder, int marker, const unsigned char *body,
                         size_t payloadLength) {
    if (marker == JPEG_MARKER_SOF0 || marker == JPEG_MARKER_SOF2) {
        decoder->progressive = marker == JPEG_MARKER_SOF2;
        return frame(decoder, body, payloadLength);
    }
    if (marker == JPEG_MARKER_DQT) {
        return readQuantizationTables(decoder, body, payloadLength);
    }
    if (marker == JPEG_MARKER_DHT) {
        return readHuffmanTables(decoder, body, payloadLength);
    }
    if (marker == JPEG_MARKER_DRI) {
        if (payloadLength != JPEG_RESTART_INTERVAL_BYTES) {
            return 0;
        }
        decoder->restart = j16(body);
        return 1;
    }
    if (marker == JPEG_MARKER_SOS) {
        return scan(decoder, body, payloadLength);
    }
    if (marker == JPEG_MARKER_APP1) {
        decoder->orientation = exifOrientation(body, payloadLength);
        return 1;
    }
    if (marker == JPEG_MARKER_APP14) {
        if (payloadLength >= JPEG_APP14_MINIMUM_BYTES &&
            !memcmp(body, "Adobe", JPEG_APP14_SIGNATURE_BYTES)) {
            if (body[JPEG_APP14_TRANSFORM_OFFSET] > 1) {
                return 0;
            }
            decoder->rgb = body[JPEG_APP14_TRANSFORM_OFFSET] == 0;
        }
        return 1;
    }
    return !(
        (marker >= JPEG_MARKER_SOF_RESERVED_FIRST && marker <= JPEG_MARKER_SOF_RESERVED_LAST) ||
        marker == JPEG_MARKER_DNL);
}

/**
 * Read and bounds-check the next length-delimited JPEG marker segment.
 * @param decoder JPEG stream state; its cursor advances past the segment.
 * @param fileLength Total JPEG file length.
 * @param segment Receives the marker and bounded payload.
 * @return One for a segment, zero for EOI, or minus one for malformed input.
 * @author kai
 */
static int readSegment(Jpeg *decoder, size_t fileLength, JpegSegment *segment) {
    unsigned length;
    int marker = nextMarker(decoder);
    if (marker == JPEG_MARKER_EOI) {
        return 0;
    }
    if (marker < 0 || marker == JPEG_MARKER_SOI || marker == 0 || marker == JPEG_MARKER_TEM ||
        (marker >= JPEG_MARKER_RESTART_FIRST && marker <= JPEG_MARKER_RESTART_LAST) ||
        decoder->at + JPEG_RESTART_INTERVAL_BYTES > fileLength) {
        return -1;
    }
    length = j16(decoder->p + decoder->at);
    if (length < JPEG_RESTART_INTERVAL_BYTES || length > fileLength - decoder->at) {
        return -1;
    }
    segment->marker = marker;
    segment->body = decoder->p + decoder->at + JPEG_RESTART_INTERVAL_BYTES;
    segment->payloadLength = length - JPEG_RESTART_INTERVAL_BYTES;
    decoder->at += length;
    return 1;
}

/**
 * Decode supported 8-bit Huffman JPEG data and optionally apply EXIF orientation.
 * @param data Complete JPEG file bytes.
 * @param n JPEG length in bytes.
 * @param image Receives decoded RGBA8 pixels.
 * @param orient Apply EXIF orientation for standalone images; use 0 for PDF image space.
 * @return 1 with owned RGBA pixels, or 0 without a retained allocation.
 * @author kai
 */
int rcJpeg(const unsigned char *data, size_t n, RcImage *image, int orient) {
    Jpeg *decoder = calloc(1, sizeof(*decoder));
    int success = 0;
    int ended = 0;
    unsigned componentIndex;

    /* PREPARE DECODER OWNERSHIP*/
    if (!decoder) {
        return 0;
    }
    memset(image, 0, sizeof(*image));
    decoder->p = data;
    decoder->n = n;
    decoder->at = 2;
    decoder->orientation = 1;
    if (n < 4 || data[0] != JPEG_SAMPLE_MAX || data[1] != JPEG_MARKER_SOI) {
        goto done;
    }

    /* WALK LENGTH-DELIMITED MARKERS*/
    /* Entropy scans consume their own bytes; other marker payloads stay within the declared
     * length.*/
    while (decoder->at < n) {
        JpegSegment segment;
        int segmentStatus = readSegment(decoder, n, &segment);
        if (segmentStatus < 0) {
            goto done;
        }
        if (!segmentStatus) {
            ended = 1;
            break;
        }
        if (!processMarker(decoder, segment.marker, segment.body, segment.payloadLength)) {
            goto done;
        }
    }

    /* RECONSTRUCT AND ORIENT PIXELS*/
    if (!ended || !render(decoder, image)) {
        goto done;
    }
    if (orient && !orientImage(image, decoder->orientation)) {
        goto done;
    }
    success = 1;
done:
    /* RELEASE COMPONENT WORKSPACE*/
    /* On failure, also discard any partially reconstructed destination pixels.*/
    for (componentIndex = 0; componentIndex < 3; componentIndex++) {
        free(decoder->c[componentIndex].coef);
        free(decoder->c[componentIndex].samples);
    }
    free(decoder);
    if (!success) {
        free(image->rgba);
        memset(image, 0, sizeof(*image));
    }
    return success;
}
