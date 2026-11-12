/**
 * @file jpeg.c Original baseline and progressive Huffman JPEG decoder.
 * @author kai
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* SHARED CONTRACT*/
#include "codec.h"

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
 * @param p Two JPEG bytes in big-endian order.
 * @return The unsigned 16-bit value represented by the bytes.
 * @author kai
 */
static unsigned j16(const unsigned char *p) { return ((unsigned)p[0] << 8) | p[1]; }
/**
 * Read entropy-coded JPEG bits and reject invalid byte stuffing.
 * @param j Decoder state holding the entropy stream and bit cache.
 * @param n Number of entropy bits to consume.
 * @return The next n bits, or zero while marking decoder state invalid on truncation.
 * @author kai
 */
static unsigned jbits(Jpeg *j, unsigned n) {
    unsigned v = 0;
    while (n--) {
        if (!j->count) {
            unsigned c;
            if (j->at >= j->n) {
                j->bad = 1;
                return 0;
            }
            c = j->p[j->at++];
            if (c == 255) {
                if (j->at >= j->n || j->p[j->at++] != 0) {
                    j->bad = 1;
                    return 0;
                }
            }
            j->bits = c;
            j->count = 8;
        }
        v = (v << 1) | ((j->bits >> (--j->count)) & 1);
    }
    return v;
}
/**
 * Decode one symbol from a JPEG canonical Huffman table.
 * @param j JPEG decoder state used to read the code bits.
 * @param h Canonical JPEG table for this symbol class.
 * @return Decoded symbol; an invalid code marks the decoder state bad.
 * @author kai
 */
static int jhuff(Jpeg *j, const JHuff *h) {
    unsigned code = 0, first = 0, index = 0, n;
    /* An unregistered table has zero counts and cannot match any code in this bounded walk.*/
    for (n = 1; n <= 16; n++) {
        code = (code << 1) | jbits(j, 1);
        if (j->bad)
            return 0;
        if (code >= first && code - first < h->count[n])
            return (int)h->symbol[index + code - first];
        index += h->count[n];
        first = (first + h->count[n]) << 1;
    }
    j->bad = 1;
    return 0;
}
/**
 * Read and sign-extend a JPEG magnitude value.
 * @param j Decoder state supplying the magnitude bits.
 * @param n Magnitude bit count from the Huffman symbol.
 * @return The JPEG sign-extended magnitude, or zero when the count is unsupported.
 * @author kai
 */
static int jreceive(Jpeg *j, unsigned n) {
    unsigned v;
    if (n > 16) {
        j->bad = 1;
        return 0;
    }
    v = jbits(j, n);
    return n && v < (1U << (n - 1)) ? (int)v - (int)((1U << n) - 1) : (int)v;
}
/**
 * Read the next JPEG marker, skipping marker fill bytes.
 * @param j Decoder state positioned at the next marker prefix.
 * @return Marker byte, or -1 for a missing or truncated marker.
 * @author kai
 */
static int nextMarker(Jpeg *j) {
    unsigned c;
    if (j->at >= j->n || j->p[j->at++] != 255)
        return -1;
    do {
        if (j->at >= j->n)
            return -1;
        c = j->p[j->at++];
    } while (c == 255);
    return (int)c;
}

/* METADATA*/
/**
 * Read a TIFF 16-bit value in the byte order declared by EXIF.
 * @param p Two TIFF bytes to interpret.
 * @param le Nonzero selects little-endian byte order.
 * @return The unsigned 16-bit TIFF value.
 * @author kai
 */
static unsigned ex16(const unsigned char *p, int le) {
    return le ? ((unsigned)p[1] << 8) | p[0] : j16(p);
}
/**
 * Read a TIFF 32-bit value in the byte order declared by EXIF.
 * @param p Four TIFF bytes to interpret.
 * @param le Nonzero selects little-endian byte order.
 * @return The unsigned 32-bit TIFF value.
 * @author kai
 */
static unsigned long ex32(const unsigned char *p, int le) {
    return le ? (unsigned long)ex16(p, 1) | ((unsigned long)ex16(p + 2, 1) << 16)
              : ((unsigned long)ex16(p, 0) << 16) | ex16(p + 2, 0);
}
/**
 * Read a valid EXIF orientation tag; default to normal orientation when absent or malformed.
 * @param p APP1 payload beginning with an EXIF header.
 * @param n Available payload length, used to bound TIFF offsets.
 * @return EXIF orientation from one through eight, defaulting to one when absent or invalid.
 * @author kai
 */
static unsigned exifOrientation(const unsigned char *p, size_t n) {
    unsigned long off;
    unsigned count, i;
    int le;
    if (n < 14 || memcmp(p, "Exif\0\0", 6))
        return 1;
    p += 6;
    n -= 6;
    le = p[0] == 'I' && p[1] == 'I';
    if (!le && !(p[0] == 'M' && p[1] == 'M'))
        return 1;
    if (ex16(p + 2, le) != 42)
        return 1;
    off = ex32(p + 4, le);
    if (off > n - 2)
        return 1;
    count = ex16(p + off, le);
    if (count > (n - off - 2) / 12)
        return 1;
    for (i = 0; i < count; i++) {
        const unsigned char *tag = p + off + 2 + i * 12;
        if (ex16(tag, le) == 274 && ex16(tag + 2, le) == 3 && ex32(tag + 4, le) == 1) {
            unsigned v = ex16(tag + 8, le);
            return v >= 1 && v <= 8 ? v : 1;
        }
    }
    return 1;
}

/* COEFFICIENTS*/
/**
 * Parse JPEG frame dimensions, sampling factors, and quantization table references.
 * @param j JPEG state receiving frame metadata.
 * @param p Frame segment payload.
 * @param n Payload length from the marker.
 * @return Nonzero when frame geometry and bounded coefficient planes are valid.
 * @author kai
 */
static int frame(Jpeg *j, const unsigned char *p, size_t n) {
    unsigned i;

    /* VALIDATE FRAME GEOMETRY*/
    if (n < 6 || p[0] != 8 || j->nc)
        return 0;
    j->h = j16(p + 1);
    j->w = j16(p + 3);
    j->nc = p[5];
    if (!j->w || !j->h || (size_t)j->w > RC_PIXELS / j->h || (j->nc != 1 && j->nc != 3) ||
        n != 6 + j->nc * 3)
        return 0;

    /* REGISTER COMPONENT SAMPLING*/
    /* Sampling factors determine the MCU grid and each padded coefficient plane.*/
    for (i = 0; i < j->nc; i++) {
        JComp *c = &j->c[i];
        unsigned k;
        c->id = p[6 + i * 3];
        c->h = p[7 + i * 3] >> 4;
        c->v = p[7 + i * 3] & 15;
        c->q = p[8 + i * 3];
        if (!c->h || !c->v || c->h > 4 || c->v > 4 || c->q > 3)
            return 0;
        for (k = 0; k < i; k++)
            if (j->c[k].id == c->id)
                return 0;
        if (c->h > j->mh)
            j->mh = c->h;
        if (c->v > j->mv)
            j->mv = c->v;
        for (k = 0; k < 64; k++)
            c->last[k] = -1;
    }
    j->mx = (j->w + 8 * j->mh - 1) / (8 * j->mh);
    j->my = (j->h + 8 * j->mv - 1) / (8 * j->mv);
    {
        /* ALLOCATE BOUNDED COEFFICIENT PLANES*/
        /* Track the combined allocation so component padding cannot bypass the memory limit.*/
        size_t total = 0;
        unsigned blocks = 0;
        for (i = 0; i < j->nc; i++) {
            JComp *c = &j->c[i];
            size_t count;
            c->bx = j->mx * c->h;
            c->by = j->my * c->v;
            c->actualX = (j->w * c->h + 8 * j->mh - 1) / (8 * j->mh);
            c->actualY = (j->h * c->v + 8 * j->mv - 1) / (8 * j->mv);
            count = (size_t)c->bx * c->by * 64;
            if (count > RC_LIMIT / sizeof(int) || total > RC_LIMIT - count * sizeof(int))
                return 0;
            total += count * sizeof(int);
            blocks += c->h * c->v;
            c->coef = calloc(count, sizeof(int));
            if (!c->coef)
                return 0;
        }
        if (blocks > 10)
            return 0;
    }
    return 1;
}
/**
 * Apply one progressive AC refinement bit to an already nonzero coefficient.
 * @param j Progressive JPEG decoder state.
 * @param c Component whose coefficient block is being refined.
 * @param bit Refinement bit to apply at the current spectral position.
 * @return Nonzero when the refinement bits are valid.
 * @author kai
 */
static int refineNonzero(Jpeg *j, int *c, unsigned bit) {
    if (jbits(j, 1) && ((unsigned)abs(*c) & bit) == 0)
        *c += *c > 0 ? (int)bit : -(int)bit;
    return !j->bad;
}
/**
 * Decode one JPEG block for sequential or progressive scans.
 * @param j Decoder state for the current scan.
 * @param c Component and predictor state for this block.
 * @param block Receives the 64 quantized coefficients.
 * @param dc DC Huffman table index.
 * @param ac AC Huffman table index.
 * @param ss First spectral coefficient in this scan.
 * @param se Last spectral coefficient in this scan.
 * @param ah Previous approximation bit plane.
 * @param al Current approximation bit plane.
 * @param eob Remaining end-of-band run for progressive AC data.
 * @return Nonzero when the block is decoded without invalid coefficients or truncated bits.
 * @author kai
 */
static int decodeBlock(Jpeg *j, JComp *c, int *block, unsigned dc, unsigned ac, unsigned ss,
                       unsigned se, unsigned ah, unsigned al, unsigned *eob) {
    unsigned k = ss, bit = 1U << al;

    /* DECODE OR REFINE THE DC VALUE*/
    /* DC coefficients are differences from the previous block in the same component.*/
    if (!ss) {
        if (!ah) {
            int s = jhuff(j, &j->tables[0][dc]);
            int delta;
            if (s < 0 || s > 11)
                return 0;
            delta = jreceive(j, (unsigned)s);
            if (c->dc > 1048576 || c->dc < -1048576)
                return 0;
            c->dc += delta;
            if (c->dc > (int)(1048576U / bit) || c->dc < -(int)(1048576U / bit))
                return 0;
            block[0] = c->dc * (int)bit;
        } else if (jbits(j, 1))
            block[0] |= (int)bit;
        k = 1;
        if (!se)
            return !j->bad;
    }

    /* DECODE INITIAL AC COEFFICIENTS*/
    /* An EOB run can skip the remaining spectral band across several blocks.*/
    if (!ah) {
        if (*eob) {
            (*eob)--;
            return 1;
        }
        while (k <= se && !j->bad) {
            int symbol = jhuff(j, &j->tables[1][ac]);
            unsigned run = (unsigned)symbol >> 4, s = (unsigned)symbol & 15;
            if (!s) {
                if (run == 15) {
                    k += 16;
                    if (k > se + 1)
                        return 0;
                    continue;
                }
                if (j->progressive) {
                    *eob = (1U << run) + jbits(j, run) - 1;
                }
                return !j->bad;
            }
            if (s > 10 || k + run > se)
                return 0;
            k += run;
            block[zig[k++]] = jreceive(j, s) * (int)bit;
        }
        return !j->bad;
    }

    /* REFINE PROGRESSIVE AC COEFFICIENTS*/
    /* New coefficients consume a sign bit; existing nonzero values consume refinement bits.*/
    if (!*eob) {
        while (k <= se && !j->bad) {
            int symbol = jhuff(j, &j->tables[1][ac]), value = 0;
            unsigned run = (unsigned)symbol >> 4, s = (unsigned)symbol & 15;
            if (s) {
                if (s != 1)
                    return 0;
                value = jbits(j, 1) ? (int)bit : -(int)bit;
            } else if (run != 15) {
                *eob = (1U << run) + jbits(j, run);
                break;
            } else
                run = 16;

            /* COUNT ONLY ZERO COEFFICIENTS*/
            /* Nonzero coefficients still consume refinement bits but do not reduce the zero run.*/
            while (k <= se) {
                int *v = &block[zig[k]];
                if (*v) {
                    if (!refineNonzero(j, v, bit))
                        return 0;
                } else {
                    if (!run) {
                        if (value) {
                            *v = value;
                            k++;
                        }
                        break;
                    }
                    run--;
                }
                k++;
            }
            if (run || (value && k > se + 1))
                return 0;
        }
    }

    /* REFINE THE REMAINDER OF AN EOB BLOCK*/
    if (*eob) {
        for (; k <= se; k++) {
            int *v = &block[zig[k]];
            if (*v && !refineNonzero(j, v, bit))
                return 0;
        }
        (*eob)--;
    }
    return !j->bad;
}
/**
 * Decode a JPEG scan, including restart markers and progressive coefficient updates.
 * @param j JPEG state updated by the scan.
 * @param p Scan segment payload and entropy-coded bytes.
 * @param n Bytes available in the scan segment.
 * @return Nonzero when all scan units and progressive runs decode successfully.
 * @author kai
 */
static int scan(Jpeg *j, const unsigned char *p, size_t n) {
    unsigned ns, i, indices[3], dcs[3], acs[3], ss, se, ah, al, mx, my, x, y, unit = 0, eob = 0,
                                                                              rst = 0;

    /* VALIDATE SCAN COMPONENTS*/
    if (!j->nc || n < 4)
        return 0;
    ns = p[0];
    if (!ns || ns > j->nc || n != 1 + ns * 2 + 3)
        return 0;
    for (i = 0; i < ns; i++) {
        unsigned c;
        for (c = 0; c < j->nc; c++)
            if (j->c[c].id == p[1 + i * 2])
                break;
        if (c == j->nc)
            return 0;
        indices[i] = c;
        dcs[i] = p[2 + i * 2] >> 4;
        acs[i] = p[2 + i * 2] & 15;
        if (dcs[i] > 3 || acs[i] > 3)
            return 0;
        {
            unsigned k;
            for (k = 0; k < i; k++)
                if (indices[k] == c)
                    return 0;
        }
        j->c[c].dc = 0;
    }

    /* VALIDATE SPECTRAL PROGRESSION*/
    /* Refinement scans must follow the previous approximation level for every selected
     * coefficient.*/
    ss = p[1 + ns * 2];
    se = p[2 + ns * 2];
    ah = p[3 + ns * 2] >> 4;
    al = p[3 + ns * 2] & 15;
    if (ss > se || se > 63 || ah > 13 || al > 13)
        return 0;
    if (j->progressive) {
        if ((!ss && se) || (ss && ns != 1) || (ah && ah != al + 1))
            return 0;
    } else if (ss || se != 63 || ah || al)
        return 0;
    for (i = 0; i < ns; i++) {
        unsigned k;
        JComp *c = &j->c[indices[i]];
        for (k = ss; k <= se; k++) {
            if ((!ah && c->last[k] != -1) || (ah && c->last[k] != (int)ah))
                return 0;
            c->last[k] = (int)al;
        }
    }

    /* PREPARE THE MCU WALK*/
    /* Single-component scans use actual block bounds rather than interleaved padding.*/
    mx = ns == 1 ? j->c[indices[0]].actualX : j->mx;
    my = ns == 1 ? j->c[indices[0]].actualY : j->my;
    j->bits = 0;
    j->count = 0;

    /* Restart intervals delimit both DC predictors and progressive EOB runs.*/
    for (y = 0; y < my; y++)
        for (x = 0; x < mx; x++) {
            /* RESET AT RESTART BOUNDARIES*/
            /* Restart marker numbering cycles modulo eight and resets predictors and EOB state.*/
            if (j->restart && unit == j->restart) {
                int marker;
                j->bits = 0;
                j->count = 0;
                marker = nextMarker(j);
                if (marker != 208 + (int)(rst & 7))
                    return 0;
                rst++;
                unit = 0;
                eob = 0;
                for (i = 0; i < j->nc; i++)
                    j->c[i].dc = 0;
            }

            /* DECODE THE MCU COMPONENT BLOCKS*/
            for (i = 0; i < ns; i++) {
                JComp *c = &j->c[indices[i]];
                unsigned yy, xx, h = ns == 1 ? 1 : c->h, v = ns == 1 ? 1 : c->v;
                for (yy = 0; yy < v; yy++)
                    for (xx = 0; xx < h; xx++) {
                        unsigned bx = ns == 1 ? x : x * c->h + xx, by = ns == 1 ? y : y * c->v + yy;
                        int *block = c->coef + ((size_t)by * c->bx + bx) * 64;
                        if (!decodeBlock(j, c, block, dcs[i], acs[i], ss, se, ah, al, &eob))
                            return 0;
                    }
            }
            unit++;
        }
    j->bits = 0;
    j->count = 0;
    return !j->bad && !eob;
}

/*//////////////////////////////////////////////////
RECONSTRUCTION
//////////////////////////////////////////////////*/

/* SAMPLE LIMITS*/
/**
 * Round and clamp a component value to the 8-bit channel range.
 * @param v Floating-point channel value before 8-bit clamping.
 * @return Rounded channel value clamped to the inclusive 0–255 range.
 * @author kai
 */
static unsigned char clampByte(double v) {
    if (v <= 0)
        return 0;
    if (v >= 255)
        return 255;
    return (unsigned char)floor(v + 0.5);
}

/* INVERSE TRANSFORM*/
/**
 * Convert decoded JPEG component planes to straight-alpha RGBA8 pixels.
 * @param j Decoded component planes and sampling metadata.
 * @param im Receives the rendered RGBA8 image.
 * @return Nonzero when all component planes render into a complete image.
 * @author kai
 */
static int render(Jpeg *j, RcImage *im) {
    double basis[8][8];
    unsigned c, x, y, u, v;
    int direct =
        j->rgb || (j->nc == 3 && j->c[0].id == 'R' && j->c[1].id == 'G' && j->c[2].id == 'B');

    /* BUILD THE INVERSE-TRANSFORM BASIS*/
    for (x = 0; x < 8; x++)
        for (u = 0; u < 8; u++)
            basis[x][u] = (u ? 1.0 : 0.7071067811865475) *
                          cos((2.0 * x + 1) * u * 3.14159265358979323846 / 16.0);

    /* Reconstruct separate component planes before centred chroma sampling.*/
    for (c = 0; c < j->nc; c++) {
        JComp *co = &j->c[c];
        unsigned bx, by, stride = co->bx * 8;
        /* DC progression and a nonzero first quantizer prove that both inputs were registered.*/
        if (co->last[0] < 0 || !j->quant[co->q][0])
            return 0;
        co->samples = malloc((size_t)co->bx * co->by * 64);
        if (!co->samples)
            return 0;

        /* RECONSTRUCT COMPONENT BLOCKS*/
        for (by = 0; by < co->by; by++)
            for (bx = 0; bx < co->bx; bx++) {
                int *coeff = co->coef + ((size_t)by * co->bx + bx) * 64;
                double tmp[8][8];
                int ac = 0;
                for (u = 1; u < 64; u++)
                    if (coeff[u]) {
                        ac = 1;
                        break;
                    }

                /* HANDLE CONSTANT BLOCKS*/
                /* A DC-only block has one value throughout, avoiding the two full transform
                 * passes.*/
                if (!ac) {
                    unsigned char val =
                        clampByte((double)coeff[0] * j->quant[co->q][0] / 8.0 + 128);
                    for (y = 0; y < 8; y++)
                        memset(co->samples + ((size_t)by * 8 + y) * stride + bx * 8, val, 8);
                    continue;
                }

                /* APPLY THE TWO TRANSFORM PASSES*/
                /* Dequantize horizontally, then reconstruct rows vertically with the shared
                 * cosine*/
                /* basis.*/
                for (v = 0; v < 8; v++)
                    for (x = 0; x < 8; x++) {
                        double sum = 0;
                        for (u = 0; u < 8; u++)
                            sum += basis[x][u] * coeff[v * 8 + u] * j->quant[co->q][v * 8 + u];
                        tmp[v][x] = sum;
                    }
                for (y = 0; y < 8; y++)
                    for (x = 0; x < 8; x++) {
                        double sum = 0;
                        for (v = 0; v < 8; v++)
                            sum += basis[y][v] * tmp[v][x];
                        co->samples[((size_t)by * 8 + y) * stride + bx * 8 + x] =
                            clampByte(sum / 4 + 128);
                    }
            }
    }

    /* Convert reconstructed samples only after every component is available.*/
    im->rgba = malloc((size_t)j->w * j->h * 4);
    if (!im->rgba)
        return 0;
    im->width = j->w;
    im->height = j->h;
    for (y = 0; y < j->h; y++)
        for (x = 0; x < j->w; x++) {
            double values[3] = {128, 128, 128};
            unsigned char *pixel = im->rgba + ((size_t)y * j->w + x) * 4;

            /* UPSAMPLE COMPONENTS AT PIXEL CENTRES*/
            /* Clamp neighbours at the visible plane edges instead of sampling padded image
             * blocks.*/
            for (c = 0; c < j->nc; c++) {
                JComp *co = &j->c[c];
                double fx = ((x + 0.5) * co->h / j->mh) - 0.5,
                       fy = ((y + 0.5) * co->v / j->mv) - 0.5;
                unsigned sw = (j->w * co->h + j->mh - 1) / j->mh,
                         sh = (j->h * co->v + j->mv - 1) / j->mv, x0, y0, x1, y1,
                         stride = co->bx * 8;
                double ax, ay;
                if (fx < 0)
                    fx = 0;
                if (fy < 0)
                    fy = 0;
                if (fx > sw - 1)
                    fx = sw - 1;
                if (fy > sh - 1)
                    fy = sh - 1;
                x0 = (unsigned)fx;
                y0 = (unsigned)fy;
                x1 = x0 + 1 < sw ? x0 + 1 : x0;
                y1 = y0 + 1 < sh ? y0 + 1 : y0;
                ax = fx - x0;
                ay = fy - y0;
                values[c] = (co->samples[(size_t)y0 * stride + x0] * (1 - ax) +
                             co->samples[(size_t)y0 * stride + x1] * ax) *
                                (1 - ay) +
                            (co->samples[(size_t)y1 * stride + x0] * (1 - ax) +
                             co->samples[(size_t)y1 * stride + x1] * ax) *
                                ay;
            }

            /* CONVERT COMPONENT VALUES TO RGB*/
            /* Grayscale and explicit RGB bypass the YCbCr colour transform.*/
            if (j->nc == 1)
                pixel[0] = pixel[1] = pixel[2] = clampByte(values[0]);
            else if (direct)
                for (c = 0; c < 3; c++)
                    pixel[c] = clampByte(values[c]);
            else {
                double cb = values[1] - 128, cr = values[2] - 128;
                pixel[0] = clampByte(values[0] + 1.402 * cr);
                pixel[1] = clampByte(values[0] - 0.344136 * cb - 0.714136 * cr);
                pixel[2] = clampByte(values[0] + 1.772 * cb);
            }
            pixel[3] = 255;
        }
    return 1;
}

/* ORIENTATION*/
/**
 * Apply EXIF orientation by rearranging decoded pixels and dimensions.
 * @param im Owned image whose pixels and dimensions are transformed.
 * @param orientation EXIF orientation value from one through eight.
 * @return Nonzero when orientation is applied or already normal; zero on allocation failure.
 * @author kai
 */
static int orientImage(RcImage *im, unsigned orientation) {
    unsigned x, y, w = im->width, h = im->height, nw = orientation >= 5 ? h : w,
                   nh = orientation >= 5 ? w : h;
    unsigned char *out;

    /* PREPARE ORIENTATION REMAPPING*/
    if (orientation == 1)
        return 1;
    out = malloc((size_t)w * h * 4);
    if (!out)
        return 0;

    /* MAP SOURCE PIXELS TO THE NEW GRID*/
    /* Transpose first; bits zero and one then reflect destination x and y respectively.*/
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            static const unsigned char reflections[9] = {0, 0, 1, 3, 2, 0, 1, 3, 2};
            unsigned xx = orientation >= 5 ? y : x, yy = orientation >= 5 ? x : y;
            if (reflections[orientation] & 1)
                xx = nw - 1 - xx;
            if (reflections[orientation] & 2)
                yy = nh - 1 - yy;
            memcpy(out + ((size_t)yy * nw + xx) * 4, im->rgba + ((size_t)y * w + x) * 4, 4);
        }
    free(im->rgba);
    im->rgba = out;
    im->width = nw;
    im->height = nh;
    return 1;
}

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* JPEG INPUT*/
/**
 * Decode supported 8-bit Huffman JPEG data and optionally apply EXIF orientation.
 * @param p Complete JPEG file bytes.
 * @param n JPEG length in bytes.
 * @param im Receives decoded RGBA8 pixels.
 * @param orient Apply EXIF orientation for standalone images; use 0 for PDF image space.
 * @return 1 with owned RGBA pixels, or 0 without a retained allocation.
 * @author kai
 */
int rcJpeg(const unsigned char *p, size_t n, RcImage *im, int orient) {
    Jpeg *j = calloc(1, sizeof(*j));
    int ok = 0, ended = 0;
    unsigned c;

    /* PREPARE DECODER OWNERSHIP*/
    if (!j)
        return 0;
    memset(im, 0, sizeof(*im));
    j->p = p;
    j->n = n;
    j->at = 2;
    j->orientation = 1;
    if (n < 4 || p[0] != 255 || p[1] != 216)
        goto done;

    /* WALK LENGTH-DELIMITED MARKERS*/
    /* Entropy scans consume their own bytes; other marker payloads stay within the declared
     * length.*/
    while (j->at < n) {
        int marker = nextMarker(j);
        unsigned length;
        const unsigned char *body;
        size_t payload;
        if (marker == 217) {
            ended = 1;
            break;
        }
        if (marker < 0 || marker == 216 || marker == 0 || marker == 1 ||
            (marker >= 208 && marker <= 215))
            goto done;
        if (j->at + 2 > n)
            goto done;
        length = j16(p + j->at);
        if (length < 2 || length > n - j->at)
            goto done;
        body = p + j->at + 2;
        payload = length - 2;
        j->at += length;

        /* DISPATCH THE MARKER PAYLOAD*/
        if (marker == 192 || marker == 194) {
            j->progressive = marker == 194;
            if (!frame(j, body, payload))
                goto done;
        } else if (marker == 219) {
            size_t at = 0;
            while (at < payload) {
                /* READ QUANTIZATION TABLES*/
                /* JPEG transmits values in zigzag order; the transform consumes natural
                 * coefficient*/
                /* order.*/
                unsigned spec = body[at++], q = spec & 15, wide = spec >> 4, k;
                if (q > 3 || wide > 1 || (wide ? 128 : 64) > payload - at)
                    goto done;
                for (k = 0; k < 64; k++) {
                    unsigned value = wide ? j16(body + at + k * 2) : body[at + k];
                    if (!value)
                        goto done;
                    j->quant[q][zig[k]] = value;
                }
                at += wide ? 128 : 64;
            }
        } else if (marker == 196) {
            size_t at = 0;
            while (at < payload) {
                /* READ HUFFMAN TABLES*/
                /* Check the canonical code space before accepting the transmitted symbol list.*/
                unsigned spec = body[at++], kind = spec >> 4, id = spec & 15, k, total = 0;
                int left = 1;
                JHuff *h;
                if (kind > 1 || id > 3 || payload - at < 16)
                    goto done;
                h = &j->tables[kind][id];
                memset(h, 0, sizeof(*h));
                for (k = 1; k <= 16; k++) {
                    h->count[k] = body[at++];
                    total += h->count[k];
                    left = left * 2 - (int)h->count[k];
                    if (left < 0)
                        goto done;
                }
                if (!total || total > 256 || total > payload - at)
                    goto done;
                for (k = 0; k < total; k++)
                    h->symbol[k] = body[at++];
            }
        } else if (marker == 221) {
            /* READ RESTART INTERVALS*/
            /* This count determines when entropy predictors reset within later scans.*/
            if (payload != 2)
                goto done;
            j->restart = j16(body);
        } else if (marker == 218) {
            if (!scan(j, body, payload))
                goto done;
        } else if (marker == 225) {
            j->orientation = exifOrientation(body, payload);
        } else if (marker == 238 && payload >= 12 && !memcmp(body, "Adobe", 5)) {
            if (body[11] > 1)
                goto done;
            j->rgb = body[11] == 0;
        } else if ((marker >= 192 && marker <= 207) || marker == 220)
            goto done;
    }

    /* RECONSTRUCT AND ORIENT PIXELS*/
    if (!ended || !render(j, im))
        goto done;
    if (orient && !orientImage(im, j->orientation))
        goto done;
    ok = 1;
done:
    /* RELEASE COMPONENT WORKSPACE*/
    /* On failure, also discard any partially reconstructed destination pixels.*/
    for (c = 0; c < 3; c++) {
        free(j->c[c].coef);
        free(j->c[c].samples);
    }
    free(j);
    if (!ok) {
        free(im->rgba);
        memset(im, 0, sizeof(*im));
    }
    return ok;
}
