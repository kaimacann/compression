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
static const unsigned char zig[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};
typedef struct {
    unsigned count[17], symbol[256];
} JHuff;
typedef struct {
    unsigned id, h, v, q, bx, by, actualX, actualY;
    int dc, last[64];
    int *coef;
    unsigned char *samples;
} JComp;
typedef struct {
    const unsigned char *p;
    size_t n, at;
    unsigned bits, count;
    int bad;
    unsigned w, h, nc, mh, mv, mx, my, restart, progressive, rgb, orientation;
    unsigned quant[4][64];
    JHuff tables[2][4];
    JComp c[3];
} Jpeg;

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
static unsigned j16(const unsigned char *bytes) { return ((unsigned)bytes[0] << 8) | bytes[1]; }
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
            if (byte == 255) {
                if (decoder->at >= decoder->n || decoder->p[decoder->at++] != 0) {
                    decoder->bad = 1;
                    return 0;
                }
            }
            decoder->bits = byte;
            decoder->count = 8;
        }
        value = (value << 1) | ((decoder->bits >> (--decoder->count)) & 1);
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
    for (codeLength = 1; codeLength <= 16; codeLength++) {
        code = (code << 1) | jbits(decoder, 1);
        if (decoder->bad) {
            return 0;
        }
        if (code >= first && code - first < table->count[codeLength]) {
            return (int)table->symbol[index + code - first];
        }
        index += table->count[codeLength];
        first = (first + table->count[codeLength]) << 1;
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
    if (n > 16) {
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
    if (decoder->at >= decoder->n || decoder->p[decoder->at++] != 255) {
        return -1;
    }
    do {
        if (decoder->at >= decoder->n) {
            return -1;
        }
        marker = decoder->p[decoder->at++];
    } while (marker == 255);
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
    return littleEndian ? ((unsigned)bytes[1] << 8) | bytes[0] : j16(bytes);
}
/**
 * Read a TIFF 32-bit value in the byte order declared by EXIF.
 * @param bytes Four TIFF bytes to interpret.
 * @param littleEndian Nonzero selects little-endian byte order.
 * @return The unsigned 32-bit TIFF value.
 * @author kai
 */
static unsigned long ex32(const unsigned char *bytes, int littleEndian) {
    return littleEndian ? (unsigned long)ex16(bytes, 1) | ((unsigned long)ex16(bytes + 2, 1) << 16)
                        : ((unsigned long)ex16(bytes, 0) << 16) | ex16(bytes + 2, 0);
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
    if (n < 14 || memcmp(payload, "Exif\0\0", 6) != 0) {
        return 1;
    }
    payload += 6;
    n -= 6;
    littleEndian = payload[0] == 'I' && payload[1] == 'I';
    if (!littleEndian && !(payload[0] == 'M' && payload[1] == 'M')) {
        return 1;
    }
    if (ex16(payload + 2, littleEndian) != 42) {
        return 1;
    }
    off = ex32(payload + 4, littleEndian);
    if (off > n - 2) {
        return 1;
    }
    count = ex16(payload + off, littleEndian);
    if (count > (n - off - 2) / 12) {
        return 1;
    }
    for (entryIndex = 0; entryIndex < count; entryIndex++) {
        const unsigned char *tag = payload + off + 2 + (entryIndex * 12);
        if (ex16(tag, littleEndian) == 274 && ex16(tag + 2, littleEndian) == 3 &&
            ex32(tag + 4, littleEndian) == 1) {
            unsigned orientation = ex16(tag + 8, littleEndian);
            return orientation >= 1 && orientation <= 8 ? orientation : 1;
        }
    }
    return 1;
}

/* COEFFICIENTS*/
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
    if (n < 6 || payload[0] != 8 || decoder->nc) {
        return 0;
    }
    decoder->h = j16(payload + 1);
    decoder->w = j16(payload + 3);
    decoder->nc = payload[5];
    if (!decoder->w || !decoder->h || (size_t)decoder->w > RC_PIXELS / decoder->h ||
        (decoder->nc != 1 && decoder->nc != 3) || n != 6 + decoder->nc * 3) {
        return 0;
    }

    /* REGISTER COMPONENT SAMPLING*/
    /* Sampling factors determine the MCU grid and each padded coefficient plane.*/
    for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
        JComp *component = &decoder->c[componentIndex];
        unsigned priorComponentIndex;
        component->id = payload[6 + (componentIndex * 3)];
        component->h = payload[7 + (componentIndex * 3)] >> 4;
        component->v = payload[7 + (componentIndex * 3)] & 15;
        component->q = payload[8 + (componentIndex * 3)];
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
        for (priorComponentIndex = 0; priorComponentIndex < 64; priorComponentIndex++) {
            component->last[priorComponentIndex] = -1;
        }
    }
    decoder->mx = (decoder->w + 8 * decoder->mh - 1) / (8 * decoder->mh);
    decoder->my = (decoder->h + 8 * decoder->mv - 1) / (8 * decoder->mv);
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
                (decoder->w * component->h + 8 * decoder->mh - 1) / (8 * decoder->mh);
            component->actualY =
                (decoder->h * component->v + 8 * decoder->mv - 1) / (8 * decoder->mv);
            count = (size_t)component->bx * component->by * 64;
            if (count > RC_LIMIT / sizeof(int) || total > RC_LIMIT - count * sizeof(int)) {
                return 0;
            }
            total += count * sizeof(int);
            blocks += component->h * component->v;
            component->coef = calloc(count, sizeof(int));
            if (!component->coef) {
                return 0;
            }
        }
        if (blocks > 10) {
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
 * Decode one JPEG block for sequential or progressive scans.
 * @param decoder Decoder state for the current scan.
 * @param component Component and predictor state for this block.
 * @param block Receives the 64 quantized coefficients.
 * @param dcTable DC Huffman table index.
 * @param acTable AC Huffman table index.
 * @param spectralStart First spectral coefficient in this scan.
 * @param spectralEnd Last spectral coefficient in this scan.
 * @param previousApproximation Previous approximation bit plane.
 * @param currentApproximation Current approximation bit plane.
 * @param eob Remaining end-of-band run for progressive AC data.
 * @return Nonzero when the block is decoded without invalid coefficients or truncated bits.
 * @author kai
 */
static int decodeBlock(Jpeg *decoder, JComp *component, int *block, unsigned dcTable,
                       unsigned acTable, unsigned spectralStart, unsigned spectralEnd,
                       unsigned previousApproximation, unsigned currentApproximation,
                       unsigned *eob) {
    unsigned spectralIndex = spectralStart;
    unsigned bit = 1U << currentApproximation;

    /* DECODE OR REFINE THE DC VALUE*/
    /* DC coefficients are differences from the previous block in the same component.*/
    if (!spectralStart) {
        if (!previousApproximation) {
            int category = jhuff(decoder, &decoder->tables[0][dcTable]);
            int delta;
            if (category < 0 || category > 11) {
                return 0;
            }
            delta = jreceive(decoder, (unsigned)category);
            if (component->dc > 1048576 || component->dc < -1048576) {
                return 0;
            }
            component->dc += delta;
            if (component->dc > (int)(1048576U / bit) || component->dc < -(int)(1048576U / bit)) {
                return 0;
            }
            block[0] = component->dc * (int)bit;
        } else if (jbits(decoder, 1)) {
            block[0] |= (int)bit;
        }
        spectralIndex = 1;
        if (!spectralEnd) {
            return !decoder->bad;
        }
    }

    /* DECODE INITIAL AC COEFFICIENTS*/
    /* An EOB run can skip the remaining spectral band across several blocks.*/
    if (!previousApproximation) {
        if (*eob) {
            (*eob)--;
            return 1;
        }
        while (spectralIndex <= spectralEnd && !decoder->bad) {
            int symbol = jhuff(decoder, &decoder->tables[1][acTable]);
            unsigned run = (unsigned)symbol >> 4;
            unsigned category = (unsigned)symbol & 15;
            if (!category) {
                if (run == 15) {
                    spectralIndex += 16;
                    if (spectralIndex > spectralEnd + 1) {
                        return 0;
                    }
                    continue;
                }
                if (decoder->progressive) {
                    *eob = (1U << run) + jbits(decoder, run) - 1;
                }
                return !decoder->bad;
            }
            if (category > 10 || spectralIndex + run > spectralEnd) {
                return 0;
            }
            spectralIndex += run;
            block[zig[spectralIndex++]] = jreceive(decoder, category) * (int)bit;
        }
        return !decoder->bad;
    }

    /* REFINE PROGRESSIVE AC COEFFICIENTS*/
    /* New coefficients consume a sign bit; existing nonzero values consume refinement bits.*/
    if (!*eob) {
        while (spectralIndex <= spectralEnd && !decoder->bad) {
            int symbol = jhuff(decoder, &decoder->tables[1][acTable]);
            int value = 0;
            unsigned run = (unsigned)symbol >> 4;
            unsigned category = (unsigned)symbol & 15;
            if (category) {
                if (category != 1) {
                    return 0;
                }
                value = jbits(decoder, 1) ? (int)bit : -(int)bit;
            } else if (run != 15) {
                *eob = (1U << run) + jbits(decoder, run);
                break;
            } else {
                run = 16;
            }

            /* COUNT ONLY ZERO COEFFICIENTS*/
            /* Nonzero coefficients still consume refinement bits but do not reduce the zero run.*/
            while (spectralIndex <= spectralEnd) {
                int *coefficient = &block[zig[spectralIndex]];
                if (*coefficient) {
                    if (!refineNonzero(decoder, coefficient, bit)) {
                        return 0;
                    }
                } else {
                    if (!run) {
                        if (value) {
                            *coefficient = value;
                            spectralIndex++;
                        }
                        break;
                    }
                    run--;
                }
                spectralIndex++;
            }
            if (run || (value && spectralIndex > spectralEnd + 1)) {
                return 0;
            }
        }
    }

    /* REFINE THE REMAINDER OF AN EOB BLOCK*/
    if (*eob) {
        for (; spectralIndex <= spectralEnd; spectralIndex++) {
            int *coefficient = &block[zig[spectralIndex]];
            if (*coefficient && !refineNonzero(decoder, coefficient, bit)) {
                return 0;
            }
        }
        (*eob)--;
    }
    return !decoder->bad;
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
    unsigned scanComponentCount;
    unsigned scanComponentIndex;
    unsigned indices[3];
    unsigned dcs[3];
    unsigned acs[3];
    unsigned spectralStart;
    unsigned spectralEnd;
    unsigned previousApproximation;
    unsigned currentApproximation;
    unsigned mcuColumns;
    unsigned mcuRows;
    unsigned mcuX;
    unsigned mcuY;
    unsigned unit = 0;
    unsigned eob = 0;
    unsigned rst = 0;

    /* VALIDATE SCAN COMPONENTS*/
    if (!decoder->nc || n < 4) {
        return 0;
    }
    scanComponentCount = payload[0];
    if (!scanComponentCount || scanComponentCount > decoder->nc ||
        n != 1 + scanComponentCount * 2 + 3) {
        return 0;
    }
    for (scanComponentIndex = 0; scanComponentIndex < scanComponentCount; scanComponentIndex++) {
        unsigned componentIndex;
        for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
            if (decoder->c[componentIndex].id == payload[1 + (scanComponentIndex * 2)]) {
                break;
            }
        }
        if (componentIndex == decoder->nc) {
            return 0;
        }
        indices[scanComponentIndex] = componentIndex;
        dcs[scanComponentIndex] = payload[2 + (scanComponentIndex * 2)] >> 4;
        acs[scanComponentIndex] = payload[2 + (scanComponentIndex * 2)] & 15;
        if (dcs[scanComponentIndex] > 3 || acs[scanComponentIndex] > 3) {
            return 0;
        }
        {
            unsigned componentIndex;
            for (componentIndex = 0; componentIndex < scanComponentIndex; componentIndex++) {
                if (indices[componentIndex] == componentIndex) {
                    return 0;
                }
            }
        }
        decoder->c[componentIndex].dc = 0;
    }

    /* VALIDATE SPECTRAL PROGRESSION*/
    /* Refinement scans must follow the previous approximation level for every selected
     * coefficient.*/
    spectralStart = payload[1 + (scanComponentCount * 2)];
    spectralEnd = payload[2 + (scanComponentCount * 2)];
    previousApproximation = payload[3 + (scanComponentCount * 2)] >> 4;
    currentApproximation = payload[3 + (scanComponentCount * 2)] & 15;
    if (spectralStart > spectralEnd || spectralEnd > 63 || previousApproximation > 13 ||
        currentApproximation > 13) {
        return 0;
    }
    if (decoder->progressive) {
        if ((!spectralStart && spectralEnd) || (spectralStart && scanComponentCount != 1) ||
            (previousApproximation && previousApproximation != currentApproximation + 1)) {
            return 0;
        }
    } else if (spectralStart || spectralEnd != 63 || previousApproximation ||
               currentApproximation) {
        return 0;
    }
    for (scanComponentIndex = 0; scanComponentIndex < scanComponentCount; scanComponentIndex++) {
        unsigned componentIndex;
        JComp *component = &decoder->c[indices[scanComponentIndex]];
        for (componentIndex = spectralStart; componentIndex <= spectralEnd; componentIndex++) {
            if ((!previousApproximation && component->last[componentIndex] != -1) ||
                (previousApproximation &&
                 component->last[componentIndex] != (int)previousApproximation)) {
                return 0;
            }
            component->last[componentIndex] = (int)currentApproximation;
        }
    }

    /* PREPARE THE MCU WALK*/
    /* Single-component scans use actual block bounds rather than interleaved padding.*/
    mcuColumns = scanComponentCount == 1 ? decoder->c[indices[0]].actualX : decoder->mx;
    mcuRows = scanComponentCount == 1 ? decoder->c[indices[0]].actualY : decoder->my;
    decoder->bits = 0;
    decoder->count = 0;

    /* Restart intervals delimit both DC predictors and progressive EOB runs.*/
    for (mcuY = 0; mcuY < mcuRows; mcuY++) {
        for (mcuX = 0; mcuX < mcuColumns; mcuX++) {
            /* RESET AT RESTART BOUNDARIES*/
            /* Restart marker numbering cycles modulo eight and resets predictors and EOB state.*/
            if (decoder->restart && unit == decoder->restart) {
                int marker;
                decoder->bits = 0;
                decoder->count = 0;
                marker = nextMarker(decoder);
                if (marker != 208 + (int)(rst & 7)) {
                    return 0;
                }
                rst++;
                unit = 0;
                eob = 0;
                for (scanComponentIndex = 0; scanComponentIndex < decoder->nc;
                     scanComponentIndex++) {
                    decoder->c[scanComponentIndex].dc = 0;
                }
            }

            /* DECODE THE MCU COMPONENT BLOCKS*/
            for (scanComponentIndex = 0; scanComponentIndex < scanComponentCount;
                 scanComponentIndex++) {
                JComp *component = &decoder->c[indices[scanComponentIndex]];
                unsigned blockY;
                unsigned blockX;
                unsigned horizontalSampling = scanComponentCount == 1 ? 1 : component->h;
                unsigned verticalSampling = scanComponentCount == 1 ? 1 : component->v;
                for (blockY = 0; blockY < verticalSampling; blockY++) {
                    for (blockX = 0; blockX < horizontalSampling; blockX++) {
                        unsigned blockX =
                            scanComponentCount == 1 ? mcuX : (mcuX * component->h) + blockX;
                        unsigned blockY =
                            scanComponentCount == 1 ? mcuY : (mcuY * component->v) + blockY;
                        int *block =
                            component->coef + (((size_t)blockY * component->bx + blockX) * 64);
                        if (!decodeBlock(decoder, component, block, dcs[scanComponentIndex],
                                         acs[scanComponentIndex], spectralStart, spectralEnd,
                                         previousApproximation, currentApproximation, &eob)) {
                            return 0;
                        }
                    }
                }
            }
            unit++;
        }
    }
    decoder->bits = 0;
    decoder->count = 0;
    return !decoder->bad && !eob;
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
    if (value >= 255) {
        return 255;
    }
    return (unsigned char)floor(value + 0.5);
}

/* INVERSE TRANSFORM*/
/**
 * Convert decoded JPEG component planes to straight-alpha RGBA8 pixels.
 * @param decoder Decoded component planes and sampling metadata.
 * @param image Receives the rendered RGBA8 image.
 * @return Nonzero when all component planes render into a complete image.
 * @author kai
 */
static int render(Jpeg *decoder, RcImage *image) {
    double basis[8][8];
    unsigned componentIndex;
    unsigned pixelX;
    unsigned pixelY;
    unsigned horizontalFrequency;
    unsigned verticalFrequency;
    int direct = decoder->rgb || (decoder->nc == 3 && decoder->c[0].id == 'R' &&
                                  decoder->c[1].id == 'G' && decoder->c[2].id == 'B');

    /* BUILD THE INVERSE-TRANSFORM BASIS*/
    for (pixelX = 0; pixelX < 8; pixelX++) {
        for (horizontalFrequency = 0; horizontalFrequency < 8; horizontalFrequency++) {
            basis[pixelX][horizontalFrequency] =
                (horizontalFrequency ? 1.0 : 0.7071067811865475) *
                cos((2.0 * pixelX + 1) * horizontalFrequency * 3.14159265358979323846 / 16.0);
        }
    }

    /* Reconstruct separate component planes before centred chroma sampling.*/
    for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
        JComp *component = &decoder->c[componentIndex];
        unsigned blockX;
        unsigned blockY;
        unsigned stride = component->bx * 8;
        /* DC progression and a nonzero first quantizer prove that both inputs were registered.*/
        if (component->last[0] < 0 || !decoder->quant[component->q][0]) {
            return 0;
        }
        component->samples = malloc((size_t)component->bx * component->by * 64);
        if (!component->samples) {
            return 0;
        }

        /* RECONSTRUCT COMPONENT BLOCKS*/
        for (blockY = 0; blockY < component->by; blockY++) {
            for (blockX = 0; blockX < component->bx; blockX++) {
                int *coeff = component->coef + (((size_t)blockY * component->bx + blockX) * 64);
                double tmp[8][8];
                int hasAcCoefficients = 0;
                for (horizontalFrequency = 1; horizontalFrequency < 64; horizontalFrequency++) {
                    if (coeff[horizontalFrequency]) {
                        hasAcCoefficients = 1;
                        break;
                    }
                }

                /* HANDLE CONSTANT BLOCKS*/
                /* A DC-only block has one value throughout, avoiding the two full transform
                 * passes.*/
                if (!hasAcCoefficients) {
                    unsigned char val =
                        clampByte(((double)coeff[0] * decoder->quant[component->q][0] / 8.0) + 128);
                    for (pixelY = 0; pixelY < 8; pixelY++) {
                        memset(component->samples + (((size_t)blockY * 8 + pixelY) * stride) +
                                   (blockX * 8),
                               val, 8);
                    }
                    continue;
                }

                /* APPLY THE TWO TRANSFORM PASSES*/
                /* Dequantize horizontally, then reconstruct rows vertically with the shared
                 * cosine*/
                /* basis.*/
                for (verticalFrequency = 0; verticalFrequency < 8; verticalFrequency++) {
                    for (pixelX = 0; pixelX < 8; pixelX++) {
                        double sum = 0;
                        for (horizontalFrequency = 0; horizontalFrequency < 8;
                             horizontalFrequency++) {
                            sum += basis[pixelX][horizontalFrequency] *
                                   coeff[(verticalFrequency * 8) + horizontalFrequency] *
                                   decoder->quant[component->q]
                                                 [(verticalFrequency * 8) + horizontalFrequency];
                        }
                        tmp[verticalFrequency][pixelX] = sum;
                    }
                }
                for (pixelY = 0; pixelY < 8; pixelY++) {
                    for (pixelX = 0; pixelX < 8; pixelX++) {
                        double sum = 0;
                        for (verticalFrequency = 0; verticalFrequency < 8; verticalFrequency++) {
                            sum +=
                                basis[pixelY][verticalFrequency] * tmp[verticalFrequency][pixelX];
                        }
                        component->samples[(((size_t)blockY * 8 + pixelY) * stride) + (blockX * 8) +
                                           pixelX] = clampByte((sum / 4) + 128);
                    }
                }
            }
        }
    }

    /* Convert reconstructed samples only after every component is available.*/
    image->rgba = malloc((size_t)decoder->w * decoder->h * 4);
    if (!image->rgba) {
        return 0;
    }
    image->width = decoder->w;
    image->height = decoder->h;
    for (pixelY = 0; pixelY < decoder->h; pixelY++) {
        for (pixelX = 0; pixelX < decoder->w; pixelX++) {
            double values[3] = {128, 128, 128};
            unsigned char *pixel = image->rgba + (((size_t)pixelY * decoder->w + pixelX) * 4);

            /* UPSAMPLE COMPONENTS AT PIXEL CENTRES*/
            /* Clamp neighbours at the visible plane edges instead of sampling padded image
             * blocks.*/
            for (componentIndex = 0; componentIndex < decoder->nc; componentIndex++) {
                JComp *component = &decoder->c[componentIndex];
                double fractionX = ((pixelX + 0.5) * component->h / decoder->mh) - 0.5;
                double fractionY = ((pixelY + 0.5) * component->v / decoder->mv) - 0.5;
                unsigned sampleWidth = (decoder->w * component->h + decoder->mh - 1) / decoder->mh;
                unsigned sampleHeight = (decoder->h * component->v + decoder->mv - 1) / decoder->mv;
                unsigned sampleX0;
                unsigned sampleY0;
                unsigned sampleX1;
                unsigned sampleY1;
                unsigned stride = component->bx * 8;
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
                values[componentIndex] =
                    (component->samples[((size_t)sampleY0 * stride) + sampleX0] *
                         (1 - fractionalX) +
                     component->samples[((size_t)sampleY0 * stride) + sampleX1] * fractionalX) *
                        (1 - fractionalY) +
                    (component->samples[((size_t)sampleY1 * stride) + sampleX0] *
                         (1 - fractionalX) +
                     component->samples[((size_t)sampleY1 * stride) + sampleX1] * fractionalX) *
                        fractionalY;
            }

            /* CONVERT COMPONENT VALUES TO RGB*/
            /* Grayscale and explicit RGB bypass the YCbCr colour transform.*/
            if (decoder->nc == 1) {
                pixel[0] = pixel[1] = pixel[2] = clampByte(values[0]);
            } else if (direct) {
                for (componentIndex = 0; componentIndex < 3; componentIndex++) {
                    pixel[componentIndex] = clampByte(values[componentIndex]);
                }
            } else {
                double blueDifference = values[1] - 128;
                double redDifference = values[2] - 128;
                pixel[0] = clampByte(values[0] + (1.402 * redDifference));
                pixel[1] =
                    clampByte(values[0] - (0.344136 * blueDifference) - (0.714136 * redDifference));
                pixel[2] = clampByte(values[0] + (1.772 * blueDifference));
            }
            pixel[3] = 255;
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
    unsigned targetWidth = orientation >= 5 ? sourceHeight : sourceWidth;
    unsigned targetHeight = orientation >= 5 ? sourceWidth : sourceHeight;
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
            unsigned targetX = orientation >= 5 ? pixelY : pixelX;
            unsigned targetY = orientation >= 5 ? pixelX : pixelY;
            if (reflections[orientation] & 1) {
                targetX = targetWidth - 1 - targetX;
            }
            if (reflections[orientation] & 2) {
                targetY = targetHeight - 1 - targetY;
            }
            memcpy(out + (((size_t)targetY * targetWidth + targetX) * 4),
                   image->rgba + (((size_t)pixelY * sourceWidth + pixelX) * 4), 4);
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
    if (n < 4 || data[0] != 255 || data[1] != 216) {
        goto done;
    }

    /* WALK LENGTH-DELIMITED MARKERS*/
    /* Entropy scans consume their own bytes; other marker payloads stay within the declared
     * length.*/
    while (decoder->at < n) {
        int marker = nextMarker(decoder);
        unsigned length;
        const unsigned char *body;
        size_t payload;
        if (marker == 217) {
            ended = 1;
            break;
        }
        if (marker < 0 || marker == 216 || marker == 0 || marker == 1 ||
            (marker >= 208 && marker <= 215)) {
            goto done;
        }
        if (decoder->at + 2 > n) {
            goto done;
        }
        length = j16(data + decoder->at);
        if (length < 2 || length > n - decoder->at) {
            goto done;
        }
        body = data + decoder->at + 2;
        payload = length - 2;
        decoder->at += length;

        /* DISPATCH THE MARKER PAYLOAD*/
        if (marker == 192 || marker == 194) {
            decoder->progressive = marker == 194;
            if (!frame(decoder, body, payload)) {
                goto done;
            }
        } else if (marker == 219) {
            size_t payloadOffset = 0;
            while (payloadOffset < payload) {
                /* READ QUANTIZATION TABLES*/
                /* JPEG transmits values in zigzag order; the transform consumes natural
                 * coefficient*/
                /* order.*/
                unsigned spec = body[payloadOffset++];
                unsigned tableId = spec & 15;
                unsigned wide = spec >> 4;
                unsigned coefficientIndex;
                if (tableId > 3 || wide > 1 || (wide ? 128 : 64) > payload - payloadOffset) {
                    goto done;
                }
                for (coefficientIndex = 0; coefficientIndex < 64; coefficientIndex++) {
                    unsigned value = wide ? j16(body + payloadOffset + (coefficientIndex * 2))
                                          : body[payloadOffset + coefficientIndex];
                    if (!value) {
                        goto done;
                    }
                    decoder->quant[tableId][zig[coefficientIndex]] = value;
                }
                payloadOffset += wide ? 128 : 64;
            }
        } else if (marker == 196) {
            size_t payloadOffset = 0;
            while (payloadOffset < payload) {
                /* READ HUFFMAN TABLES*/
                /* Check the canonical code space before accepting the transmitted symbol list.*/
                unsigned spec = body[payloadOffset++];
                unsigned kind = spec >> 4;
                unsigned tableId = spec & 15;
                unsigned symbolIndex;
                unsigned total = 0;
                int left = 1;
                JHuff *table;
                if (kind > 1 || tableId > 3 || payload - payloadOffset < 16) {
                    goto done;
                }
                table = &decoder->tables[kind][tableId];
                memset(table, 0, sizeof(*table));
                for (symbolIndex = 1; symbolIndex <= 16; symbolIndex++) {
                    table->count[symbolIndex] = body[payloadOffset++];
                    total += table->count[symbolIndex];
                    left = left * 2 - (int)table->count[symbolIndex];
                    if (left < 0) {
                        goto done;
                    }
                }
                if (!total || total > 256 || total > payload - payloadOffset) {
                    goto done;
                }
                for (symbolIndex = 0; symbolIndex < total; symbolIndex++) {
                    table->symbol[symbolIndex] = body[payloadOffset++];
                }
            }
        } else if (marker == 221) {
            /* READ RESTART INTERVALS*/
            /* This count determines when entropy predictors reset within later scans.*/
            if (payload != 2) {
                goto done;
            }
            decoder->restart = j16(body);
        } else if (marker == 218) {
            if (!scan(decoder, body, payload)) {
                goto done;
            }
        } else if (marker == 225) {
            decoder->orientation = exifOrientation(body, payload);
        } else if (marker == 238 && payload >= 12 && !memcmp(body, "Adobe", 5)) {
            if (body[11] > 1) {
                goto done;
            }
            decoder->rgb = body[11] == 0;
        } else if ((marker >= 192 && marker <= 207) || marker == 220) {
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
