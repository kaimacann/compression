/**
 * @file vector.c Adaptive palette quantization and connected-region vector paths.
 * @author kai
 */
/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* SHARED CONTRACT*/
#include "codec.h"
#include "media.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

/*//////////////////////////////////////////////////
CONFIG
//////////////////////////////////////////////////*/

/* PROCESSING STATE*/
enum {
    ALPHA_CHANNEL = 3,
    COLOR_CHANNELS = 4,
    COLOR_BUCKET_SHIFT = 4,
    COLOR_CHANNEL_MAX = 255,
    COLOR_KEY_COUNT = 65536,
    MAX_PALETTE_COLORS = 256,
    INITIAL_POINT_CAPACITY = 256,
    MIN_SIMPLIFY_POINTS = 5,
    PATH_FORMAT_CAPACITY = 96
};
static const unsigned ALPHA_DISTANCE_WEIGHT = 2;
typedef struct {
    unsigned long count, sum[COLOR_CHANNELS];
    unsigned char value[COLOR_CHANNELS], label;
} ColorBin;
typedef struct {
    unsigned begin, end;
} ColorBox;
typedef struct {
    RcBuffer svg, pdf;
} Paths;
typedef struct {
    int x, y;
} Point;
typedef struct {
    Point *p;
    size_t count, capacity;
} Points;
static unsigned sortChannel;
/* Pixel-corner directions run clockwise: east, south, west, north.*/
static const int stepX[COLOR_CHANNELS] = {1, 0, -1, 0}, stepY[COLOR_CHANNELS] = {0, 1, 0, -1};

/**
 * Append bounded formatted text, checking the formatter result before reading the buffer.
 * @param buffer Destination buffer that owns the appended bytes.
 * @param format Format string with values that fit the fixed output capacity.
 * @return Nonzero when formatted bytes are appended; zero on formatting, capacity, or allocation
 * failure.
 * @author kai
 */
/**
 * Copy a known number of contour points without invoking an unbounded byte-copy API.
 * @param destination Point array receiving the copied values.
 * @param source Point array supplying the values.
 * @param count Number of points to copy.
 * @author kai
 */
/*//////////////////////////////////////////////////
PALETTE
//////////////////////////////////////////////////*/

/* LABELS*/
/**
 * Map an RGBA pixel to a 16-bit histogram bucket; transparent pixels share bucket zero.
 * @param pixel One RGBA pixel to quantize into a histogram bucket.
 * @return 16-bit histogram key; fully transparent pixels map to zero.
 * @author kai
 */
static unsigned colorKey(const unsigned char *pixel) {
    if (!pixel[ALPHA_CHANNEL]) {
        return 0;
    }
    return ((unsigned)(pixel[0] >> COLOR_BUCKET_SHIFT) << 12) |
           ((unsigned)(pixel[1] >> COLOR_BUCKET_SHIFT) << 8) |
           ((unsigned)(pixel[2] >> COLOR_BUCKET_SHIFT) << COLOR_BUCKET_SHIFT) |
           (pixel[ALPHA_CHANNEL] >> COLOR_BUCKET_SHIFT);
}
/**
 * Order colour bins on the channel selected for the current median cut.
 * @param left First ColorBin supplied to qsort.
 * @param right Second ColorBin supplied to qsort.
 * @return Negative, zero, or positive according to selected channel and stable colour order.
 * @author kai
 */
static int compareBins(const void *left,
                       const void *right) { // NOLINT(bugprone-easily-swappable-parameters) -- qsort
                                            // requires two identically typed element pointers.
    const ColorBin *leftBin = left;
    const ColorBin *rightBin = right;
    int difference = (int)leftBin->value[sortChannel] - rightBin->value[sortChannel];
    return difference ? difference : memcmp(leftBin->value, rightBin->value, COLOR_CHANNELS);
}

/* MEDIAN CUT*/
/**
 * Build a population-weighted palette and assign each pixel to its nearest entry.
 * @param image Decoded RGBA image to quantize.
 * @param histogramOutput Receives the populated histogram.
 * @param binsOutput Receives compact entries for occupied histogram buckets.
 * @param binCountOutput Receives the number of occupied buckets.
 * @return Nonzero when palette labels are populated; zero on allocation failure.
 * @author kai
 */
static int collectColorBins(const RcImage *image, ColorBin **histogramOutput, ColorBin **binsOutput,
                            unsigned *binCountOutput) {
    ColorBin *histogram = calloc(COLOR_KEY_COUNT, sizeof(*histogram));
    ColorBin *bins;
    unsigned binCount = 0;
    unsigned bucketIndex;
    unsigned channelIndex;
    size_t pixelIndex;
    size_t pixelCount = (size_t)image->width * image->height;
    if (!histogram) {
        return 0;
    }
    for (pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
        const unsigned char *pixel = image->rgba + (pixelIndex * COLOR_CHANNELS);
        ColorBin *bin = &histogram[colorKey(pixel)];
        bin->count++;
        for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
            bin->sum[channelIndex] += pixel[ALPHA_CHANNEL] ? pixel[channelIndex] : 0;
        }
    }
    for (bucketIndex = 0; bucketIndex < COLOR_KEY_COUNT; bucketIndex++) {
        if (histogram[bucketIndex].count) {
            binCount++;
        }
    }
    /* Median cuts operate on occupied buckets rather than the complete colour key space. */
    bins = malloc(binCount * sizeof(*bins));
    if (!bins) {
        free(histogram);
        return 0;
    }
    binCount = 0;
    for (bucketIndex = 0; bucketIndex < COLOR_KEY_COUNT; bucketIndex++) {
        if (histogram[bucketIndex].count) {
            for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
                histogram[bucketIndex].value[channelIndex] =
                    (unsigned char)(histogram[bucketIndex].sum[channelIndex] /
                                    histogram[bucketIndex].count);
            }
            bins[binCount++] = histogram[bucketIndex];
        }
    }
    *histogramOutput = histogram;
    *binsOutput = bins;
    *binCountOutput = binCount;
    return 1;
}

/**
 * Measure the strongest population-weighted channel range in a colour box.
 * @param bins Occupied colour buckets being examined.
 * @param box Range of bins to score.
 * @param channelOutput Receives the strongest channel index.
 * @param populationOutput Receives the total pixel count in the box.
 * @return Population-weighted range score, or zero when the box cannot be split.
 * @author kai
 */
static unsigned long scoreColorBox(const ColorBin *bins, ColorBox box, unsigned *channelOutput,
                                   unsigned long *populationOutput) {
    unsigned minimum[COLOR_CHANNELS] = {COLOR_CHANNEL_MAX, COLOR_CHANNEL_MAX, COLOR_CHANNEL_MAX,
                                        COLOR_CHANNEL_MAX};
    unsigned maximum[COLOR_CHANNELS] = {0};
    unsigned channelIndex;
    unsigned binIndex;
    unsigned long population = 0;
    unsigned long bestScore = 0;
    if (box.end - box.begin < 2) {
        return 0;
    }
    for (binIndex = box.begin; binIndex < box.end; binIndex++) {
        population += bins[binIndex].count;
        for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
            if (bins[binIndex].value[channelIndex] < minimum[channelIndex]) {
                minimum[channelIndex] = bins[binIndex].value[channelIndex];
            }
            if (bins[binIndex].value[channelIndex] > maximum[channelIndex]) {
                maximum[channelIndex] = bins[binIndex].value[channelIndex];
            }
        }
    }
    for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
        unsigned long score = (maximum[channelIndex] - minimum[channelIndex]) *
                              (unsigned long)sqrt((double)population);
        if (score > bestScore) {
            bestScore = score;
            *channelOutput = channelIndex;
        }
    }
    *populationOutput = population;
    return bestScore;
}

/**
 * Select the box with the largest population-weighted channel range.
 * @param bins Occupied colour buckets to split.
 * @param boxes Current palette partition ranges.
 * @param boxCount Number of current boxes.
 * @param channelOutput Receives the channel to sort on.
 * @param populationOutput Receives the selected box population.
 * @return Selected box index or MAX_PALETTE_COLORS when no box can be split.
 * @author kai
 */
static unsigned chooseColorBox(const ColorBin *bins, const ColorBox *boxes, unsigned boxCount,
                               unsigned *channelOutput, unsigned long *populationOutput) {
    unsigned chosenBox = MAX_PALETTE_COLORS;
    unsigned boxIndex;
    unsigned long bestScore = 0;
    for (boxIndex = 0; boxIndex < boxCount; boxIndex++) {
        unsigned candidateChannel = 0;
        unsigned long population = 0;
        unsigned long score = scoreColorBox(bins, boxes[boxIndex], &candidateChannel, &population);
        if (score > bestScore) {
            bestScore = score;
            chosenBox = boxIndex;
            *channelOutput = candidateChannel;
            *populationOutput = population;
        }
    }
    return chosenBox;
}

/**
 * Split one bin range near its population-weighted median.
 * @param bins Colour buckets that are sorted and divided in place.
 * @param boxes Current ranges to update.
 * @param boxCount Number of boxes before appending the new range.
 * @param chosenBox Range index selected for splitting.
 * @param channel Channel used to sort the selected range.
 * @param population Total pixel population in the selected range.
 * @author kai
 */
static void splitColorBox(ColorBin *bins, ColorBox *boxes, unsigned boxCount, unsigned chosenBox,
                          unsigned channel, unsigned long population) {
    ColorBox box = boxes[chosenBox];
    unsigned split = box.begin + 1;
    unsigned binIndex;
    unsigned long populationAtSplit = 0;
    sortChannel = channel;
    qsort(bins + box.begin, box.end - box.begin, sizeof(*bins), compareBins);
    for (binIndex = box.begin; binIndex + 1 < box.end; binIndex++) {
        populationAtSplit += bins[binIndex].count;
        split = binIndex + 1;
        if (populationAtSplit >= population / 2) {
            break;
        }
    }
    boxes[chosenBox].end = split;
    boxes[boxCount] = (ColorBox){split, box.end};
}

/**
 * Average original sample sums into each final palette colour.
 * @param bins Occupied colour buckets partitioned by median cut.
 * @param boxes Final bin ranges.
 * @param boxCount Number of palette colours.
 * @param palette Receives averaged RGBA entries.
 * @author kai
 */
static void averagePalette(const ColorBin *bins, const ColorBox *boxes, unsigned boxCount,
                           unsigned char palette[MAX_PALETTE_COLORS][COLOR_CHANNELS]) {
    unsigned boxIndex;
    for (boxIndex = 0; boxIndex < boxCount; boxIndex++) {
        unsigned long population = 0;
        unsigned long sums[COLOR_CHANNELS] = {0};
        unsigned binIndex;
        unsigned channelIndex;
        for (binIndex = boxes[boxIndex].begin; binIndex < boxes[boxIndex].end; binIndex++) {
            population += bins[binIndex].count;
            for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
                sums[channelIndex] += bins[binIndex].sum[channelIndex];
            }
        }
        for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
            palette[boxIndex][channelIndex] =
                (unsigned char)((sums[channelIndex] + population / 2) / population);
        }
    }
}

/**
 * Assign every occupied histogram bucket to its nearest averaged palette entry.
 * @param histogram Colour histogram receiving palette labels.
 * @param palette Averaged RGBA entries.
 * @param paletteCount Number of populated entries.
 * @author kai
 */
static void assignPaletteLabels(ColorBin *histogram,
                                unsigned char palette[MAX_PALETTE_COLORS][COLOR_CHANNELS],
                                unsigned paletteCount) {
    unsigned bucketIndex;
    for (bucketIndex = 0; bucketIndex < COLOR_KEY_COUNT; bucketIndex++) {
        unsigned colorIndex;
        unsigned long bestDistance = (unsigned long)-1;
        if (!histogram[bucketIndex].count) {
            continue;
        }
        for (colorIndex = 0; colorIndex < paletteCount; colorIndex++) {
            unsigned long distance = 0;
            unsigned channelIndex;
            for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
                long difference = (long)histogram[bucketIndex].value[channelIndex] -
                                  palette[colorIndex][channelIndex];
                unsigned weight = channelIndex == ALPHA_CHANNEL ? ALPHA_DISTANCE_WEIGHT : 1;
                distance += (unsigned long)(difference * difference) * weight;
            }
            if (distance < bestDistance) {
                bestDistance = distance;
                histogram[bucketIndex].label = (unsigned char)colorIndex;
            }
        }
    }
}

/**
 * Build a population-weighted palette and assign each pixel to its nearest entry.
 * @param image Decoded RGBA image to quantize.
 * @param requested Maximum number of palette colours.
 * @param palette Receives palette RGBA entries.
 * @param used Receives the number of populated palette entries.
 * @param labels Receives one palette index per source pixel.
 * @return Nonzero when palette labels are populated; zero on allocation failure.
 * @author kai
 */
static int paletteImage(const RcImage *image, unsigned requested,
                        unsigned char palette[MAX_PALETTE_COLORS][COLOR_CHANNELS], unsigned *used,
                        unsigned char **labels) {
    ColorBin *histogram = NULL;
    ColorBin *bins = NULL;
    ColorBox boxes[MAX_PALETTE_COLORS];
    unsigned binCount = 0;
    unsigned paletteCount = 1;
    unsigned char *pixelLabels = NULL;
    unsigned channel = 0;
    unsigned chosenBox;
    unsigned long population = 0;
    size_t pixelIndex;
    size_t pixelCount = (size_t)image->width * image->height;
    int succeeded = 0;
    *labels = NULL;
    if (!collectColorBins(image, &histogram, &bins, &binCount)) {
        return 0;
    }
    pixelLabels = malloc(pixelCount);
    if (!pixelLabels) {
        goto done;
    }
    boxes[0] = (ColorBox){0, binCount};
    /* Split populated ranges by the strongest population-weighted colour channel. */
    while (paletteCount < requested) {
        chosenBox = chooseColorBox(bins, boxes, paletteCount, &channel, &population);
        if (chosenBox == MAX_PALETTE_COLORS) {
            break;
        }
        splitColorBox(bins, boxes, paletteCount, chosenBox, channel, population);
        paletteCount++;
    }
    averagePalette(bins, boxes, paletteCount, palette);
    assignPaletteLabels(histogram, palette, paletteCount);
    for (pixelIndex = 0; pixelIndex < pixelCount; pixelIndex++) {
        pixelLabels[pixelIndex] =
            histogram[colorKey(image->rgba + (pixelIndex * COLOR_CHANNELS))].label;
    }
    *used = paletteCount;
    *labels = pixelLabels;
    succeeded = 1;
done:
    free(histogram);
    free(bins);
    if (!succeeded) {
        free(pixelLabels);
    }
    return succeeded;
}

/*//////////////////////////////////////////////////
CONTOURS
//////////////////////////////////////////////////*/

/* EDGE GRAPH*/
/**
 * Append a contour vertex, growing the point list as needed.
 * @param p Contour point array to extend.
 * @param x Horizontal position.
 * @param y Vertical position.
 * @return Nonzero when the vertex is appended; zero when bounded allocation fails.
 * @author kai
 */
static int pointAdd(Points *p, int x, int y) {
    if (p->count == p->capacity) {
        size_t cap = p->capacity ? p->capacity * 2 : 256;
        Point *q;
        if (cap > RC_LIMIT / sizeof(Point)) {
            return 0;
        }
        q = realloc(p->p, cap * sizeof(*q));
        if (!q) {
            return 0;
        }
        p->p = q;
        p->capacity = cap;
    }
    p->p[p->count++] = (Point){x, y};
    return 1;
}
/**
 * Return a palette label at a pixel coordinate, or the transparent/outside label.
 * @param labels Row-major palette index for each pixel.
 * @param w Image width used for row addressing.
 * @param h Image height used to identify outside coordinates.
 * @param x Horizontal position.
 * @param y Vertical position.
 * @return Palette index at the coordinate, or -1 outside the image.
 * @author kai
 */
static int labelAt(const unsigned char *labels, unsigned w, unsigned h, int x, int y) {
    return x < 0 || y < 0 || (unsigned)x >= w || (unsigned)y >= h
               ? -1
               : labels[((size_t)y * w) + (unsigned)x];
}
/**
 * Check whether a directed pixel edge borders the requested palette region.
 * @param labels Row-major palette labels.
 * @param w Image width.
 * @param h Image height.
 * @param x Horizontal position.
 * @param y Vertical position.
 * @param d Clockwise edge direction: east, south, west, or north.
 * @param color Palette index whose boundary is being traced.
 * @param cell Receives the adjacent pixel index for visited-edge tracking.
 * @return Nonzero when this edge borders the requested region and cell is set.
 * @author kai
 */
static int edge(const unsigned char *labels, unsigned w, unsigned h, int x, int y, unsigned d,
                int color, size_t *cell) {
    static const int rightX[4] = {0, -1, -1, 0};
    static const int rightY[4] = {0, 0, -1, -1};
    static const int leftX[4] = {0, 0, -1, -1};
    static const int leftY[4] = {-1, 0, 0, -1};
    int rx = x + rightX[d];
    int ry = y + rightY[d];
    int lx = x + leftX[d];
    int ly = y + leftY[d];
    if (labelAt(labels, w, h, rx, ry) != color || labelAt(labels, w, h, lx, ly) == color) {
        return 0;
    }
    *cell = (size_t)ry * w + (unsigned)rx;
    return 1;
}

/* SIMPLIFICATION*/
/**
 * Measure squared distance from a point to a line segment for contour simplification.
 * @param p Point whose distance is measured.
 * @param a First endpoint of the segment.
 * @param b Second endpoint of the segment.
 * @return Squared distance from p to the nearest point on segment ab.
 * @author kai
 */
static double distanceSquared(Point p, Point a, Point b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double t = 0;
    double ex;
    double ey;
    if (dx || dy) {
        t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy);
    }
    t = fmax(0, fmin(1, t));
    ex = p.x - a.x - t * dx;
    ey = p.y - a.y - t * dy;
    return (ex * ex) + (ey * ey);
}
/**
 * Simplify a closed contour while retaining winding, shared colour anchors, and three vertices.
 * @param p Closed contour whose vertices may be removed.
 * @param tolerance Contour simplification tolerance.
 * @param labels Neighbouring palette labels used to preserve shared anchors.
 * @param w Image width for label lookups.
 * @param h Image height for label lookups.
 * @return Nonzero when simplification completes; zero if scratch allocation fails.
 * @author kai
 */
static int simplify(Points *p, double tolerance, const unsigned char *labels, unsigned w,
                    unsigned h) {
    unsigned char *keep;
    size_t *stack;
    size_t top = 0;
    size_t n = p->count;
    size_t i;
    Point *scratch;
    int ok = 0;

    /* PREPARE CONTOUR SIMPLIFICATION*/
    if (n < 5 || tolerance <= 0) {
        return 1;
    }
    keep = calloc(n + 1, 1);
    stack = malloc((n + 1) * 2 * sizeof(*stack));
    scratch = malloc(n * sizeof(*scratch));
    if (!keep || !stack || !scratch) {
        goto done;
    }
    /* Shared boundaries must choose identical anchors in either direction. */
    {
        /* LOCK SHARED JUNCTIONS*/
        /* Three or more surrounding labels make this vertex a boundary that neither neighbour may*/
        /* move.*/
        size_t anchor = 0;
        size_t junction = n;
        for (i = 0; i < n; i++) {
            int values[4];
            int distinct = 0;
            int q;
            int j;
            int x = p->p[i].x;
            int y = p->p[i].y;
            values[0] = labelAt(labels, w, h, x - 1, y - 1);
            values[1] = labelAt(labels, w, h, x, y - 1);
            values[2] = labelAt(labels, w, h, x - 1, y);
            values[3] = labelAt(labels, w, h, x, y);
            for (q = 0; q < 4; q++) {
                for (j = 0; j < q; j++) {
                    if (values[j] == values[q]) {
                        break;
                    }
                }
                if (j == q) {
                    distinct++;
                }
            }
            if (x < p->p[anchor].x || (x == p->p[anchor].x && y < p->p[anchor].y)) {
                anchor = i;
            }
            if (distinct >= 3 && (junction == n || x < p->p[junction].x ||
                                  (x == p->p[junction].x && y < p->p[junction].y))) {
                junction = i;
            }
            stack[i] = (size_t)(distinct >= 3);
        }

        /* ROTATE TO A SHARED ANCHOR*/
        /* Lexicographic anchors make forward and reverse traversals choose the same segments.*/
        if (junction != n) {
            anchor = junction;
        }
        for (i = 0; i < n; i++) {
            size_t original = (anchor + i) % n;
            scratch[i] = p->p[original];
            keep[i] = (unsigned char)stack[original];
        }
        memcpy(p->p, scratch, n * sizeof(*scratch));
    }

    /* SEED LOCKED SEGMENTS*/
    /* Retain the closing vertex and split work only between fixed junctions.*/
    if (!pointAdd(p, p->p[0].x, p->p[0].y)) {
        goto done;
    }
    keep[0] = keep[n] = 1;
    {
        size_t begin = 0;
        for (i = 1; i <= n; i++) {
            if (keep[i]) {
                stack[top++] = begin;
                stack[top++] = i;
                begin = i;
            }
        }
    }

    /* SIMPLIFY BETWEEN ANCHORS*/
    /* The explicit stack avoids recursion on detailed image boundaries.*/
    while (top) {
        size_t end = stack[--top];
        size_t begin = stack[--top];
        size_t chosen = 0;
        double best = tolerance * tolerance;
        for (i = begin + 1; i < end; i++) {
            double d = distanceSquared(p->p[i], p->p[begin], p->p[end]);

            /* Break equal-distance ties by coordinates so reversed shared edges simplify*/
            /* identically.*/
            if (d > best || (chosen && d == best &&
                             (p->p[i].x < p->p[chosen].x ||
                              (p->p[i].x == p->p[chosen].x && p->p[i].y < p->p[chosen].y)))) {
                best = d;
                chosen = i;
            }
        }
        if (chosen) {
            keep[chosen] = 1;
            stack[top++] = begin;
            stack[top++] = chosen;
            stack[top++] = chosen;
            stack[top++] = end;
        }
    }
    {
        /* VALIDATE WINDING BEFORE COMMIT*/
        /* Keep the original contour if simplification would collapse it or reverse its
         * orientation.*/
        size_t count = 0;
        double before = 0;
        double after = 0;
        for (i = 0; i < n; i++) {
            before +=
                (double)p->p[i].x * p->p[(i + 1) % n].y - (double)p->p[(i + 1) % n].x * p->p[i].y;
            if (keep[i]) {
                scratch[count++] = p->p[i];
            }
        }
        for (i = 0; i < count; i++) {
            after += (double)scratch[i].x * scratch[(i + 1) % count].y -
                     (double)scratch[(i + 1) % count].x * scratch[i].y;
        }
        if (count >= 3 && before * after > 0) {
            memcpy(p->p, scratch, count * sizeof(*scratch));
            p->count = count;
        } else {
            p->count = n;
        }
    }
    ok = 1;
done:
    /* RELEASE SIMPLIFICATION WORKSPACE*/
    /* One scratch allocation serves both rotation and the candidate contour.*/
    free(keep);
    free(stack);
    free(scratch);
    return ok;
}

/* PATH SERIALIZATION*/
/**
 * Append one closed contour to the corresponding SVG and PDF path buffers.
 * @param svg SVG path buffer receiving the contour.
 * @param pdf PDF path buffer receiving equivalent geometry.
 * @param p Simplified closed contour vertices.
 * @return Nonzero when both path buffers accept the contour; zero if an append fails.
 * @author kai
 */
static int emitContour(RcBuffer *svg, RcBuffer *pdf, const Points *p) {
    size_t i;
    char text[96];
    int length;

    /* OPEN THE CLOSED PATH*/
    if (p->count < 3) {
        return 1;
    }
    length = snprintf(text, sizeof(text), "M%d %d", p->p[0].x, p->p[0].y);
    if (!rcAppend(svg, text, (size_t)length)) {
        return 0;
    }
    length = snprintf(text, sizeof(text), "%d %d m\n", p->p[0].x, p->p[0].y);
    if (!rcAppend(pdf, text, (size_t)length)) {
        return 0;
    }

    /* SERIALIZE SHARED VERTICES*/
    /* SVG uses compact relative moves; PDF receives equivalent absolute line endpoints.*/
    for (i = 1; i < p->count; i++) {
        int dx = p->p[i].x - p->p[i - 1].x;
        int dy = p->p[i].y - p->p[i - 1].y;
        length = snprintf(text, sizeof(text),
                          dx == 0   ? "v%d"
                          : dy == 0 ? "h%d"
                                    : "l%d %d",
                          dx == 0 ? dy : dx, dy);
        if (!rcAppend(svg, text, (size_t)length)) {
            return 0;
        }
        length = snprintf(text, sizeof(text), "%d %d l\n", p->p[i].x, p->p[i].y);
        if (!rcAppend(pdf, text, (size_t)length)) {
            return 0;
        }
    }
    return rcAppend(svg, "z", 1) && rcAppend(pdf, "h\n", 2);
}

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* BUFFER OWNERSHIP*/
/**
 * Release SVG and PDF path buffers owned by a vector result.
 * @param v Vector result whose owned buffers are released and cleared.
 * @author kai
 */
void rcFreeVector(RcVector *v) {
    free(v->svg.data);
    free(v->pdf.data);
    free(v->resources.data);
    memset(v, 0, sizeof(*v));
}

/* REGION OUTPUT*/
/**
 * Quantize RGBA pixels and trace connected palette regions into shared SVG/PDF paths.
 * @param im Decoded RGBA source image.
 * @param colors Palette size, limited to 2 through 256.
 * @param tolerance Maximum contour displacement in pixels.
 * @param out Receives SVG/PDF geometry and palette metadata.
 * @return 1 with owned output buffers; 0 after releasing incomplete geometry.
 * @author kai
 */
int rcVectorize(const RcImage *im, unsigned colors, double tolerance, RcVector *out) {
    unsigned char palette[256][4];
    unsigned char *labels = NULL;
    unsigned char *visited = NULL;
    Paths *paths = NULL;
    Points points = {0};
    unsigned count = 0;
    unsigned x;
    unsigned y;
    unsigned k;
    size_t total = (size_t)im->width * im->height;
    int ok = 0;
    char text[256];
    int n;

    /* PREPARE REGION WORKSPACE*/
    /* A visited bit belongs to one directed side of one pixel, not to the whole region.*/
    memset(out, 0, sizeof(*out));
    if (colors < 2 || colors > 256 || !paletteImage(im, colors, palette, &count, &labels)) {
        return 0;
    }
    visited = calloc(total, 1);
    paths = calloc(count, sizeof(*paths));
    if (!visited || !paths) {
        goto done;
    }

    /* DISCOVER REGION BOUNDARIES*/
    /* Walk each boundary with its region on the right; transparent labels are skipped.*/
    for (y = 0; y < im->height; y++) {
        for (x = 0; x < im->width; x++) {
            unsigned color = labels[(size_t)y * im->width + x];
            unsigned d;

            /* Transparent regions remain empty rather than being flattened onto a background.*/
            if (!palette[color][3]) {
                continue;
            }
            for (d = 0; d < 4; d++) {
                int sx = (int)x + (d == 1 || d == 2);
                int sy = (int)y + (d >= 2);
                int cx = sx;
                int cy = sy;
                unsigned direction = d;
                size_t cell;
                size_t steps = 0;
                points.count = 0;

                /* START AN UNVISITED CONTOUR*/
                /* Start only on an unvisited side separating different labels.*/
                if (!edge(labels, im->width, im->height, sx, sy, d, (int)color, &cell) ||
                    (visited[cell] & (1U << d))) {
                    continue;
                }
                if (!pointAdd(&points, sx, sy)) {
                    goto done;
                }
                do {
                    static const unsigned turns[4] = {1, 0, 3, 2};
                    unsigned next;
                    unsigned i;
                    size_t nextCell;

                    /* ADVANCE THE BOUNDARY*/
                    /* Reject a repeated directed edge before moving to its endpoint.*/
                    if (visited[cell] & (1U << direction)) {
                        goto done;
                    }

                    /* Direction order is east, south, west, north.*/
                    visited[cell] |= (unsigned char)(1U << direction);
                    cx += stepX[direction];
                    cy += stepY[direction];

                    /* CHOOSE THE NEXT EDGE*/
                    /* Right-first turns keep diagonal contacts in separate contours.*/
                    for (i = 0; i < 4; i++) {
                        next = (direction + turns[i]) % 4;
                        if (edge(labels, im->width, im->height, cx, cy, next, (int)color,
                                 &nextCell)) {
                            break;
                        }
                    }

                    /* BOUND THE WALK*/
                    /* A contour cannot visit more than four sides per pixel.*/
                    if (i == 4 || ++steps > total * 4) {
                        goto done;
                    }

                    /* RECORD CORNERS*/
                    /* Record turns only; straight runs need no intermediate vertices.*/
                    if (next != direction && !pointAdd(&points, cx, cy)) {
                        goto done;
                    }
                    direction = next;
                    cell = nextCell;
                } while (cx != sx || cy != sy || direction != d);

                /* CLOSE AND STORE THE CONTOUR*/
                /* Drop the duplicate endpoint; share simplified vertices across formats.*/
                if (points.count > 1 && points.p[points.count - 1].x == points.p[0].x &&
                    points.p[points.count - 1].y == points.p[0].y) {
                    points.count--;
                }
                if (!simplify(&points, tolerance, labels, im->width, im->height) ||
                    !emitContour(&paths[color].svg, &paths[color].pdf, &points)) {
                    goto done;
                }
            }
        }
    }

    /* BEGIN VECTOR DOCUMENTS*/
    /* SVG keeps pixel coordinates; the PDF transform maps the same paths into unit image space.*/
    n = snprintf(text, sizeof(text),
                 "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" viewBox=\"0 "
                 "0 %u %u\">\n",
                 im->width, im->height, im->width, im->height);
    if (!rcAppend(&out->svg, text, (size_t)n)) {
        goto done;
    }
    n = snprintf(text, sizeof(text), "q\n%.12g 0 0 %.12g 0 1 cm\n", 1.0 / im->width,
                 -1.0 / im->height);
    if (!rcAppend(&out->pdf, text, (size_t)n) ||
        !rcAppend(&out->resources, "/ExtGState <<\n", 14)) {
        goto done;
    }

    /* PAINT PALETTE REGIONS*/
    for (k = 0; k < count; k++) {
        if (!paths[k].svg.size) {
            continue;
        }

        /* SVG COLOUR AND GEOMETRY*/
        /* Even-odd filling preserves holes independently of winding.*/
        n = snprintf(
            text, sizeof(text),
            "<path fill=\"#%02x%02x%02x\" fill-opacity=\"%.6f\" fill-rule=\"evenodd\" d=\"",
            palette[k][0], palette[k][1], palette[k][2], palette[k][3] / 255.0);
        if (!rcAppend(&out->svg, text, (size_t)n) ||
            !rcAppend(&out->svg, paths[k].svg.data, paths[k].svg.size) ||
            !rcAppend(&out->svg, "\"/>\n", 4)) {
            goto done;
        }

        /* PDF COLOUR AND OPACITY*/
        /* Each colour uses its own graphics state so transparency survives conversion.*/
        n = snprintf(text, sizeof(text), "%.6f %.6f %.6f rg /A%u gs\n", palette[k][0] / 255.0,
                     palette[k][1] / 255.0, palette[k][2] / 255.0, k);
        if (!rcAppend(&out->pdf, text, (size_t)n) ||
            !rcAppend(&out->pdf, paths[k].pdf.data, paths[k].pdf.size) ||
            !rcAppend(&out->pdf, "f*\n", 3)) {
            goto done;
        }
        n = snprintf(text, sizeof(text), "/A%u << /Type /ExtGState /ca %.6f >>\n", k,
                     palette[k][3] / 255.0);
        if (!rcAppend(&out->resources, text, (size_t)n)) {
            goto done;
        }
    }

    /* FINALISE OUTPUT DOCUMENTS*/
    if (!rcAppend(&out->resources, ">>\n", 3) || !rcAppend(&out->svg, "</svg>\n", 7) ||
        !rcAppend(&out->pdf, "Q\n", 2)) {
        goto done;
    }
    out->colors = count;
    ok = 1;
done:
    /* RELEASE TEMPORARY CONTOURS*/
    /* Only complete output buffers remain owned by the caller on success.*/
    if (paths) {
        for (k = 0; k < count; k++) {
            free(paths[k].svg.data);
            free(paths[k].pdf.data);
        }
    }
    free(paths);
    free(labels);
    free(visited);
    free(points.p);
    if (!ok) {
        rcFreeVector(out);
    }
    return ok;
}
