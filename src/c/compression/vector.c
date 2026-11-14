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
#include <stdint.h>
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
    EDGE_DIRECTION_COUNT = 4,
    PATH_FORMAT_CAPACITY = 96,
    VECTOR_TEXT_BUFFER_CAPACITY = 256,
    MIN_PALETTE_COLORS = 2,
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
    ColorBin *histogram;
    ColorBin *bins;
    unsigned binCount;
} ColorBinCollection;
typedef struct {
    unsigned boxCount;
    unsigned chosenBox;
    unsigned channel;
    unsigned long population;
} ColorSplit;
typedef struct {
    RcBuffer svg, pdf;
} Paths;
typedef struct {
    const RcImage *image;
    const unsigned char *labels;
    const unsigned char (*palette)[COLOR_CHANNELS];
    unsigned int *visited;
    Paths *paths;
    unsigned paletteCount;
    size_t total;
    double tolerance;
} TraceContext;
typedef struct {
    int x;
    int y;
    unsigned direction;
    unsigned color;
} EdgeStart;
typedef struct {
    const RcImage *image;
    const unsigned char (*palette)[COLOR_CHANNELS];
    const Paths *paths;
    unsigned count;
    RcVector *out;
    char *text;
    size_t capacity;
} VectorOutputContext;
typedef struct {
    int x, y;
} Point;
typedef struct {
    Point position;
    unsigned direction;
    int color;
} EdgeQuery;
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
    return (((unsigned)pixel[0] >> (unsigned)COLOR_BUCKET_SHIFT)
            << (unsigned)(COLOR_BUCKET_SHIFT * 3)) |
           (((unsigned)pixel[1] >> (unsigned)COLOR_BUCKET_SHIFT)
            << (unsigned)(COLOR_BUCKET_SHIFT * 2)) |
           (((unsigned)pixel[2] >> (unsigned)COLOR_BUCKET_SHIFT) << (unsigned)COLOR_BUCKET_SHIFT) |
           ((unsigned)pixel[ALPHA_CHANNEL] >> (unsigned)COLOR_BUCKET_SHIFT);
}
/**
 * Order colour bins on the channel selected for the current median cut.
 * @param leftBin First colour bin.
 * @param rightBin Second colour bin.
 * @return Negative, zero, or positive according to selected channel and stable colour order.
 * @author kai
 */
static int compareBins(const ColorBin *leftBin, const ColorBin *rightBin) {
    int difference = (int)leftBin->value[sortChannel] - (int)rightBin->value[sortChannel];
    return difference ? difference : memcmp(leftBin->value, rightBin->value, COLOR_CHANNELS);
}

/**
 * Restore the max-heap property in one colour-bin range.
 * @param bins Heap values.
 * @param root Root index within the range.
 * @param count Number of values in the heap.
 * @author kai
 */
static void siftColorBins(ColorBin bins[], size_t root, size_t count) {
    while (root < count / 2) {
        size_t child = (root * 2) + 1;
        if (child + 1 < count && compareBins(&bins[child], &bins[child + 1]) < 0) {
            child++;
        }
        if (compareBins(&bins[root], &bins[child]) >= 0) {
            return;
        }
        {
            ColorBin temporary = bins[root];
            bins[root] = bins[child];
            bins[child] = temporary;
        }
        root = child;
    }
}

/**
 * Sort colour bins by their selected channel and stable colour tie-breakers.
 * @param bins Colour bins to sort in place.
 * @param count Number of bins.
 * @author kai
 */
static void sortColorBins(ColorBin bins[], size_t count) {
    size_t start = count / 2;
    size_t end = count;
    while (start > 0) {
        start--;
        siftColorBins(bins, start, count);
    }
    while (end > 1) {
        ColorBin temporary = bins[0];
        bins[0] = bins[end - 1];
        bins[end - 1] = temporary;
        end--;
        siftColorBins(bins, 0, end);
    }
}

/* MEDIAN CUT*/
/**
 * Build a population-weighted palette and assign each pixel to its nearest entry.
 * @param image Decoded RGBA image to quantize.
 * @param collection Receives the histogram, occupied buckets, and count.
 * @return Nonzero when palette labels are populated; zero on allocation failure.
 * @author kai
 */
static int collectColorBins(const RcImage *image, ColorBinCollection *collection) {
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
    collection->histogram = histogram;
    collection->bins = bins;
    collection->binCount = binCount;
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
    unsigned minimum[COLOR_CHANNELS] = {
        COLOR_CHANNEL_MAX,
        COLOR_CHANNEL_MAX,
        COLOR_CHANNEL_MAX,
        COLOR_CHANNEL_MAX,
    };
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
 * @param splitOptions Selected range, current box count, sort channel, and population.
 * @author kai
 */
static void splitColorBox(ColorBin *bins, ColorBox *boxes, ColorSplit splitOptions) {
    ColorBox box = boxes[splitOptions.chosenBox];
    unsigned split = box.begin + 1;
    unsigned binIndex;
    unsigned long populationAtSplit = 0;
    sortChannel = splitOptions.channel;
    sortColorBins(bins + box.begin, box.end - box.begin);
    for (binIndex = box.begin; binIndex + 1 < box.end; binIndex++) {
        populationAtSplit += bins[binIndex].count;
        split = binIndex + 1;
        if (populationAtSplit >= splitOptions.population / 2) {
            break;
        }
    }
    boxes[splitOptions.chosenBox].end = split;
    boxes[splitOptions.boxCount] = (ColorBox){split, box.end};
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
        if (!population) {
            memset(palette[boxIndex], 0, COLOR_CHANNELS);
            continue;
        }
        for (channelIndex = 0; channelIndex < COLOR_CHANNELS; channelIndex++) {
            palette[boxIndex][channelIndex] =
                (unsigned char)((sums[channelIndex] + (population / 2)) / population);
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
    ColorBinCollection collection = {0};
    ColorBox boxes[MAX_PALETTE_COLORS];
    unsigned binCount = 0;
    unsigned paletteCount = 1;
    unsigned char *pixelLabels = NULL;
    unsigned channel = 0;
    unsigned chosenBox;
    unsigned long population = 0;
    size_t pixelIndex;
    size_t pixelCount;
    int succeeded = 0;
    if (!image || !image->rgba || !image->width || !image->height || !requested || !palette ||
        !used || !labels) {
        return 0;
    }
    pixelCount = (size_t)image->width * image->height;
    *labels = NULL;
    if (!collectColorBins(image, &collection)) {
        return 0;
    }
    histogram = collection.histogram;
    bins = collection.bins;
    binCount = collection.binCount;
    pixelLabels = calloc(pixelCount, sizeof(*pixelLabels));
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
        splitColorBox(bins, boxes, (ColorSplit){paletteCount, chosenBox, channel, population});
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
 * @param pointList Contour point array to extend.
 * @param xPosition Horizontal position.
 * @param yPosition Vertical position.
 * @return Nonzero when the vertex is appended; zero when bounded allocation fails.
 * @author kai
 */
static int pointAdd(Points *pointList, int xPosition, int yPosition) {
    if (pointList->count == pointList->capacity) {
        size_t newCapacity = pointList->capacity ? pointList->capacity * 2 : INITIAL_POINT_CAPACITY;
        Point *newPoints;
        if (newCapacity > RC_LIMIT / sizeof(Point)) {
            return 0;
        }
        newPoints = realloc(pointList->p, newCapacity * sizeof(*newPoints));
        if (!newPoints) {
            return 0;
        }
        pointList->p = newPoints;
        pointList->capacity = newCapacity;
    }
    pointList->p[pointList->count++] = (Point){xPosition, yPosition};
    return 1;
}
/**
 * Return a palette label at a pixel coordinate, or the transparent/outside label.
 * @param labels Row-major palette index for each pixel.
 * @param imageWidth Image width used for row addressing.
 * @param imageHeight Image height used to identify outside coordinates.
 * @param xPosition Horizontal position.
 * @param yPosition Vertical position.
 * @return Palette index at the coordinate, or -1 outside the image.
 * @author kai
 */
static int labelAt(const unsigned char *labels, unsigned imageWidth, unsigned imageHeight,
                   int xPosition, int yPosition) {
    return !labels || xPosition < 0 || yPosition < 0 || (unsigned)xPosition >= imageWidth ||
                   (unsigned)yPosition >= imageHeight
               ? -1
               : labels[((size_t)yPosition * imageWidth) + (unsigned)xPosition];
}
/**
 * Check whether a directed pixel edge borders the requested palette region.
 * @param labels Row-major palette labels.
 * @param imageWidth Image width.
 * @param imageHeight Image height.
 * @param query Pixel corner, clockwise edge direction, and palette index to trace.
 * @param cell Receives the adjacent pixel index for visited-edge tracking.
 * @return Nonzero when this edge borders the requested region and cell is set.
 * @author kai
 */
static int edge(const unsigned char *labels, unsigned imageWidth, unsigned imageHeight,
                EdgeQuery query, size_t *cell) {
    static const int rightX[4] = {0, -1, -1, 0};
    static const int rightY[4] = {0, 0, -1, -1};
    static const int leftX[4] = {0, 0, -1, -1};
    static const int leftY[4] = {-1, 0, 0, -1};
    int rightXPosition;
    int rightYPosition;
    int leftXPosition;
    int leftYPosition;
    if (query.direction >= EDGE_DIRECTION_COUNT || !cell) {
        return 0;
    }
    rightXPosition = query.position.x + rightX[query.direction];
    rightYPosition = query.position.y + rightY[query.direction];
    leftXPosition = query.position.x + leftX[query.direction];
    leftYPosition = query.position.y + leftY[query.direction];
    if (rightXPosition < 0 || rightYPosition < 0 || (unsigned)rightXPosition >= imageWidth ||
        (unsigned)rightYPosition >= imageHeight) {
        return 0;
    }
    if (labelAt(labels, imageWidth, imageHeight, rightXPosition, rightYPosition) != query.color ||
        labelAt(labels, imageWidth, imageHeight, leftXPosition, leftYPosition) == query.color) {
        return 0;
    }
    *cell = ((size_t)rightYPosition * imageWidth) + (unsigned)rightXPosition;
    return 1;
}

/* SIMPLIFICATION*/
/**
 * Measure squared distance from a point to a line segment for contour simplification.
 * @param point Point whose distance is measured.
 * @param startPoint First endpoint of the segment.
 * @param endPoint Second endpoint of the segment.
 * @return Squared distance from point to the nearest point on the segment.
 * @author kai
 */
static double distanceSquared(Point point, Point startPoint, Point endPoint) {
    double deltaX = endPoint.x - startPoint.x;
    double deltaY = endPoint.y - startPoint.y;
    double projection = 0;
    double errorX;
    double errorY;
    if (deltaX || deltaY) {
        projection = (((point.x - startPoint.x) * deltaX) + ((point.y - startPoint.y) * deltaY)) /
                     ((deltaX * deltaX) + (deltaY * deltaY));
    }
    projection = fmax(0, fmin(1, projection));
    errorX = point.x - startPoint.x - (projection * deltaX);
    errorY = point.y - startPoint.y - (projection * deltaY);
    return (errorX * errorX) + (errorY * errorY);
}

/** Count distinct palette labels around one pixel corner.
 * @param labels Row-major palette labels.
 * @param imageWidth Image width for label lookup.
 * @param imageHeight Image height for label lookup.
 * @param vertex Corner whose four neighbouring pixel labels are examined.
 * @return Number of distinct labels around the corner.
 * @author kai
 */
static int distinctCornerLabels(const unsigned char *labels, unsigned imageWidth,
                                unsigned imageHeight, Point vertex) {
    int values[COLOR_CHANNELS];
    int distinct = 0;
    unsigned colorIndex;
    values[0] = labelAt(labels, imageWidth, imageHeight, vertex.x - 1, vertex.y - 1);
    values[1] = labelAt(labels, imageWidth, imageHeight, vertex.x, vertex.y - 1);
    values[2] = labelAt(labels, imageWidth, imageHeight, vertex.x - 1, vertex.y);
    values[3] = labelAt(labels, imageWidth, imageHeight, vertex.x, vertex.y);
    for (colorIndex = 0; colorIndex < COLOR_CHANNELS; colorIndex++) {
        unsigned previousIndex;
        for (previousIndex = 0; previousIndex < colorIndex; previousIndex++) {
            if (values[previousIndex] == values[colorIndex]) {
                break;
            }
        }
        distinct += (previousIndex == colorIndex);
    }
    return distinct;
}

/** Compare points in stable x-then-y order.
 * @param point Candidate point.
 * @param other Point to compare against.
 * @return Nonzero when point sorts before other.
 * @author kai
 */
static int pointBefore(Point point, Point other) {
    return point.x < other.x || (point.x == other.x && point.y < other.y);
}

/** Rotate a contour to its stable shared-boundary anchor and initialise keep flags.
 * @param contour Contour to rotate in place.
 * @param labels Palette labels used to detect shared junctions.
 * @param imageWidth Image width for label lookup.
 * @param imageHeight Image height for label lookup.
 * @param keep Receives locked-junction flags.
 * @param anchorFlags Scratch storage for per-vertex junction flags.
 * @param scratch Scratch point storage.
 * @return Number of original contour vertices.
 * @author kai
 */
static size_t rotateContourToAnchor(Points *contour, const unsigned char *labels,
                                    unsigned imageWidth, unsigned imageHeight, unsigned char *keep,
                                    size_t *anchorFlags, Point *scratch) {
    size_t vertexCount = contour->count;
    size_t anchor = 0;
    size_t junction = vertexCount;
    size_t vertexIndex;
    for (vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
        Point vertex = contour->p[vertexIndex];
        int distinct = distinctCornerLabels(labels, imageWidth, imageHeight, vertex);
        if (pointBefore(vertex, contour->p[anchor])) {
            anchor = vertexIndex;
        }
        if (distinct >= 3 &&
            (junction == vertexCount || pointBefore(vertex, contour->p[junction]))) {
            junction = vertexIndex;
        }
        anchorFlags[vertexIndex] = (size_t)(distinct >= 3);
    }
    if (junction != vertexCount) {
        anchor = junction;
    }
    for (vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
        size_t original = (anchor + vertexIndex) % vertexCount;
        scratch[vertexIndex] = contour->p[original];
        keep[vertexIndex] = (unsigned char)anchorFlags[original];
    }
    memcpy(contour->p, scratch, vertexCount * sizeof(*scratch));
    return vertexCount;
}

/** Resolve equal-distance simplification ties by point coordinates.
 * @param contour Contour containing candidate vertices.
 * @param candidate Candidate vertex index.
 * @param chosen Previously selected vertex index.
 * @return Nonzero when candidate sorts before chosen.
 * @author kai
 */
static int splitPointBefore(const Points *contour, size_t candidate, size_t chosen) {
    return pointBefore(contour->p[candidate], contour->p[chosen]);
}

/** Simplify each interval between preserved contour anchors.
 * @param contour Closed contour with its first point appended at the end.
 * @param stack Work stack of interval endpoint pairs.
 * @param top Current number of stack entries.
 * @param keep Receives selected-vertex flags.
 * @param tolerance Maximum point-to-segment displacement.
 * @author kai
 */
static void simplifySegments(Points *contour, size_t *stack, size_t *top, unsigned char *keep,
                             double tolerance) {
    while (*top) {
        size_t end = stack[--(*top)];
        size_t begin = stack[--(*top)];
        size_t chosen = 0;
        double best = tolerance * tolerance;
        size_t vertexIndex;
        for (vertexIndex = begin + 1; vertexIndex < end; vertexIndex++) {
            double distance =
                distanceSquared(contour->p[vertexIndex], contour->p[begin], contour->p[end]);
            if (distance > best ||
                (chosen && distance == best && splitPointBefore(contour, vertexIndex, chosen))) {
                best = distance;
                chosen = vertexIndex;
            }
        }
        if (chosen) {
            keep[chosen] = 1;
            stack[(*top)++] = begin;
            stack[(*top)++] = chosen;
            stack[(*top)++] = chosen;
            stack[(*top)++] = end;
        }
    }
}

/** Commit simplified vertices only when the contour remains nondegenerate and keeps its winding.
 * @param contour Contour being simplified.
 * @param keep Selected-vertex flags.
 * @param scratch Scratch storage for the candidate contour.
 * @param vertexCount Number of vertices in the original contour.
 * @return Nonzero when the simplified contour is retained.
 * @author kai
 */
static int retainSameWinding(Points *contour, const unsigned char *keep, Point *scratch,
                             size_t vertexCount) {
    size_t count = 0;
    size_t vertexIndex;
    double before = 0;
    double after = 0;
    for (vertexIndex = 0; vertexIndex < vertexCount; vertexIndex++) {
        before +=
            ((double)contour->p[vertexIndex].x * contour->p[(vertexIndex + 1) % vertexCount].y) -
            ((double)contour->p[(vertexIndex + 1) % vertexCount].x * contour->p[vertexIndex].y);
        if (keep[vertexIndex]) {
            scratch[count++] = contour->p[vertexIndex];
        }
    }
    for (vertexIndex = 0; vertexIndex < count; vertexIndex++) {
        after += ((double)scratch[vertexIndex].x * scratch[(vertexIndex + 1) % count].y) -
                 ((double)scratch[(vertexIndex + 1) % count].x * scratch[vertexIndex].y);
    }
    if (count < 3 || before * after <= 0) {
        contour->count = vertexCount;
        return 0;
    }
    memcpy(contour->p, scratch, count * sizeof(*scratch));
    contour->count = count;
    return 1;
}
/**
 * Simplify a closed contour while retaining winding, shared colour anchors, and three vertices.
 * @param contour Closed contour whose vertices may be removed.
 * @param tolerance Contour simplification tolerance.
 * @param labels Neighbouring palette labels used to preserve shared anchors.
 * @param imageWidth Image width for label lookups.
 * @param imageHeight Image height for label lookups.
 * @return Nonzero when simplification completes; zero if scratch allocation fails.
 * @author kai
 */
static int simplify(Points *contour, double tolerance, const unsigned char *labels,
                    unsigned imageWidth, unsigned imageHeight) {
    unsigned char *keep;
    size_t *stack;
    size_t top = 0;
    size_t vertexCount = contour->count;
    Point *scratch;
    int success = 0;

    /* PREPARE CONTOUR SIMPLIFICATION*/
    if (vertexCount < MIN_SIMPLIFY_POINTS || tolerance <= 0) {
        return 1;
    }
    keep = calloc(vertexCount + 1, 1);
    stack = malloc((vertexCount + 1) * 2 * sizeof(*stack));
    scratch = malloc(vertexCount * sizeof(*scratch));
    if (!keep || !stack || !scratch) {
        goto done;
    }
    /* Shared boundaries must choose identical anchors in either direction. */
    rotateContourToAnchor(contour, labels, imageWidth, imageHeight, keep, stack, scratch);

    /* SEED LOCKED SEGMENTS*/
    /* Retain the closing vertex and split work only between fixed junctions.*/
    if (!pointAdd(contour, contour->p[0].x, contour->p[0].y)) {
        goto done;
    }
    keep[0] = keep[vertexCount] = 1;
    {
        size_t begin = 0;
        size_t anchorIndex;
        for (anchorIndex = 1; anchorIndex <= vertexCount; anchorIndex++) {
            if (keep[anchorIndex]) {
                stack[top++] = begin;
                stack[top++] = anchorIndex;
                begin = anchorIndex;
            }
        }
    }

    simplifySegments(contour, stack, &top, keep, tolerance);
    retainSameWinding(contour, keep, scratch, vertexCount);
    success = 1;
done:
    /* RELEASE SIMPLIFICATION WORKSPACE*/
    /* One scratch allocation serves both rotation and the candidate contour.*/
    free(keep);
    free(stack);
    free(scratch);
    return success;
}

/* PATH SERIALIZATION*/
/**
 * Append one closed contour to the corresponding SVG and PDF path buffers.
 * @param svg SVG path buffer receiving the contour.
 * @param pdf PDF path buffer receiving equivalent geometry.
 * @param contour Simplified closed contour vertices.
 * @return Nonzero when both path buffers accept the contour; zero if an append fails.
 * @author kai
 */
static int emitContour(RcBuffer *svg, RcBuffer *pdf, const Points *contour) {
    size_t vertexIndex;
    char text[PATH_FORMAT_CAPACITY];
    int length;

    /* OPEN THE CLOSED PATH*/
    if (contour->count < 3) {
        return 1;
    }
    length = snprintf(text, sizeof(text), "M%d %d", contour->p[0].x, contour->p[0].y);
    if (!rcAppend(svg, text, (size_t)length)) {
        return 0;
    }
    length = snprintf(text, sizeof(text), "%d %d m\n", contour->p[0].x, contour->p[0].y);
    if (!rcAppend(pdf, text, (size_t)length)) {
        return 0;
    }

    /* SERIALIZE SHARED VERTICES*/
    /* SVG uses compact relative moves; PDF receives equivalent absolute line endpoints.*/
    for (vertexIndex = 1; vertexIndex < contour->count; vertexIndex++) {
        int deltaX = contour->p[vertexIndex].x - contour->p[vertexIndex - 1].x;
        int deltaY = contour->p[vertexIndex].y - contour->p[vertexIndex - 1].y;
        length = snprintf(text, sizeof(text),
                          deltaX == 0   ? "v%d"
                          : deltaY == 0 ? "h%d"
                                        : "l%d %d",
                          deltaX == 0 ? deltaY : deltaX, deltaY);
        if (!rcAppend(svg, text, (size_t)length)) {
            return 0;
        }
        length = snprintf(text, sizeof(text), "%d %d l\n", contour->p[vertexIndex].x,
                          contour->p[vertexIndex].y);
        if (!rcAppend(pdf, text, (size_t)length)) {
            return 0;
        }
    }
    return rcAppend(svg, "z", 1) && rcAppend(pdf, "h\n", 2);
}

/**
 * Close, simplify, and serialize one completed contour.
 * @param context Image, label, palette, and output workspace.
 * @param start Region colour for the contour.
 * @param points Completed contour vertices.
 * @return Nonzero when simplification and output serialization succeed.
 * @author kai
 */
static int storeTraceContour(const TraceContext *context, EdgeStart start, Points *points) {
    if (points->count > 1 && points->p[points->count - 1].x == points->p[0].x &&
        points->p[points->count - 1].y == points->p[0].y) {
        points->count--;
    }
    return simplify(points, context->tolerance, context->labels, context->image->width,
                    context->image->height) &&
           emitContour(&context->paths[start.color].svg, &context->paths[start.color].pdf, points);
}

/**
 * Trace, simplify, and serialize one boundary contour.
 * @param context Image, label, palette, and visited-edge workspace.
 * @param start Region colour and directed contour start.
 * @param points Reusable contour point storage.
 * @return Nonzero when the start is not a boundary or its contour is serialized successfully.
 * @author kai
 */
static int traceContour(const TraceContext *context, EdgeStart start, Points *points) {
    int currentX = start.x;
    int currentY = start.y;
    unsigned direction = start.direction;
    size_t cell;
    size_t steps = 0;
    if (!edge(context->labels, context->image->width, context->image->height,
              (EdgeQuery){{start.x, start.y}, start.direction, (int)start.color}, &cell)) {
        return 1;
    }
    if (cell >= context->total) {
        return 0;
    }
    if ((context->visited[cell] & (1U << start.direction)) != 0) {
        return 1;
    }
    points->count = 0;
    if (!pointAdd(points, start.x, start.y)) {
        return 0;
    }
    do {
        static const unsigned turns[EDGE_DIRECTION_COUNT] = {1, 0, 3, 2};
        unsigned next = 0;
        unsigned turnIndex;
        size_t nextCell = 0;
        if (cell >= context->total || (context->visited[cell] & (1U << direction)) != 0) {
            return 0;
        }
        context->visited[cell] |= 1U << direction;
        currentX += stepX[direction];
        currentY += stepY[direction];
        for (turnIndex = 0; turnIndex < EDGE_DIRECTION_COUNT; turnIndex++) {
            next = (direction + turns[turnIndex]) % EDGE_DIRECTION_COUNT;
            if (edge(context->labels, context->image->width, context->image->height,
                     (EdgeQuery){{currentX, currentY}, next, (int)start.color}, &nextCell)) {
                if (nextCell >= context->total) {
                    return 0;
                }
                break;
            }
        }
        if (turnIndex == EDGE_DIRECTION_COUNT || ++steps > context->total * EDGE_DIRECTION_COUNT) {
            return 0;
        }
        if (next != direction && !pointAdd(points, currentX, currentY)) {
            return 0;
        }
        direction = next;
        cell = nextCell;
    } while (currentX != start.x || currentY != start.y || direction != start.direction);

    return storeTraceContour(context, start, points);
}

/**
 * Trace every visible palette region into per-colour contour paths.
 * @param context Image, label, palette, and visited-edge workspace.
 * @param points Reusable contour point storage.
 * @return Nonzero when all visible boundaries are serialized.
 * @author kai
 */
static int traceVectorContours(const TraceContext *context, Points *points) {
    unsigned pixelX;
    unsigned pixelY;
    for (pixelY = 0; pixelY < context->image->height; pixelY++) {
        for (pixelX = 0; pixelX < context->image->width; pixelX++) {
            unsigned color = context->labels[((size_t)pixelY * context->image->width) + pixelX];
            unsigned direction;
            if (color >= context->paletteCount) {
                return 0;
            }
            if (!context->palette[color][ALPHA_CHANNEL]) {
                continue;
            }
            for (direction = 0; direction < EDGE_DIRECTION_COUNT; direction++) {
                EdgeStart start = {
                    (int)pixelX + (direction == 1 || direction == 2),
                    (int)pixelY + (direction >= 2),
                    direction,
                    color,
                };
                if (!traceContour(context, start, points)) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

/**
 * Serialize the SVG/PDF headers and all visible palette layers.
 * @param context Image dimensions, palette paths, and output buffers.
 * @return Nonzero when both output formats accept all content.
 * @author kai
 */
static int writeVectorDocument(VectorOutputContext *context) {
    int textLength;
    unsigned paletteIndex;
    textLength = snprintf(context->text, context->capacity,
                          "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%u\" height=\"%u\" "
                          "viewBox=\"0 0 %u %u\">\n",
                          context->image->width, context->image->height, context->image->width,
                          context->image->height);
    if (!rcAppend(&context->out->svg, context->text, (size_t)textLength)) {
        return 0;
    }
    textLength = snprintf(context->text, context->capacity, "q\n%.12g 0 0 %.12g 0 1 cm\n",
                          1.0 / context->image->width, -1.0 / context->image->height);
    if (!rcAppend(&context->out->pdf, context->text, (size_t)textLength) ||
        !rcAppend(&context->out->resources, "/ExtGState <<\n", sizeof("/ExtGState <<\n") - 1)) {
        return 0;
    }

    for (paletteIndex = 0; paletteIndex < context->count; paletteIndex++) {
        if (!context->paths[paletteIndex].svg.size) {
            continue;
        }
        textLength = snprintf(context->text, context->capacity,
                              "<path fill=\"#%02x%02x%02x\" fill-opacity=\"%.6f\" "
                              "fill-rule=\"evenodd\" directionIndex=\"",
                              context->palette[paletteIndex][0], context->palette[paletteIndex][1],
                              context->palette[paletteIndex][2],
                              context->palette[paletteIndex][3] / (double)COLOR_CHANNEL_MAX);
        if (!rcAppend(&context->out->svg, context->text, (size_t)textLength) ||
            !rcAppend(&context->out->svg, context->paths[paletteIndex].svg.data,
                      context->paths[paletteIndex].svg.size) ||
            !rcAppend(&context->out->svg, "\"/>\n", 4)) {
            return 0;
        }

        textLength =
            snprintf(context->text, context->capacity, "%.6f %.6f %.6f rg /A%u gs\n",
                     context->palette[paletteIndex][0] / (double)COLOR_CHANNEL_MAX,
                     context->palette[paletteIndex][1] / (double)COLOR_CHANNEL_MAX,
                     context->palette[paletteIndex][2] / (double)COLOR_CHANNEL_MAX, paletteIndex);
        if (!rcAppend(&context->out->pdf, context->text, (size_t)textLength) ||
            !rcAppend(&context->out->pdf, context->paths[paletteIndex].pdf.data,
                      context->paths[paletteIndex].pdf.size) ||
            !rcAppend(&context->out->pdf, "f*\n", 3)) {
            return 0;
        }
        textLength =
            snprintf(context->text, context->capacity, "/A%u << /Type /ExtGState /ca %.6f >>\n",
                     paletteIndex, context->palette[paletteIndex][3] / (double)COLOR_CHANNEL_MAX);
        if (!rcAppend(&context->out->resources, context->text, (size_t)textLength)) {
            return 0;
        }
    }
    return 1;
}

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* BUFFER OWNERSHIP*/
/**
 * Release SVG and PDF path buffers owned by a vector result.
 * @param vector Vector result whose owned buffers are released and cleared.
 * @author kai
 */
void rcFreeVector(RcVector *vector) {
    free(vector->svg.data);
    free(vector->pdf.data);
    free(vector->resources.data);
    memset(vector, 0, sizeof(*vector));
}

/* REGION OUTPUT*/
/**
 * Quantize RGBA pixels and trace connected palette regions into shared SVG/PDF paths.
 * @param image Decoded RGBA source image.
 * @param options Palette size and maximum contour displacement.
 * @param out Receives SVG/PDF geometry and palette metadata.
 * @return 1 with owned output buffers; 0 after releasing incomplete geometry.
 * @author kai
 */
int rcVectorize(const RcImage *image, RcVectorOptions options, RcVector *out) {
    unsigned char palette[MAX_PALETTE_COLORS][COLOR_CHANNELS];
    unsigned char *labels = NULL;
    unsigned int *visited = NULL;
    Paths *paths = NULL;
    Points points = {0};
    TraceContext context;
    VectorOutputContext outputContext;
    unsigned count = 0;
    unsigned paletteIndex;
    size_t total;
    int success = 0;
    char text[VECTOR_TEXT_BUFFER_CAPACITY];

    /* PREPARE REGION WORKSPACE*/
    /* A visited bit belongs to one directed side of one pixel, not to the whole region.*/
    if (!image || !out || !image->rgba || !image->width || !image->height) {
        return 0;
    }
    total = (size_t)image->width * image->height;
    if (total > SIZE_MAX / EDGE_DIRECTION_COUNT) {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    if (options.colors < MIN_PALETTE_COLORS || options.colors > MAX_PALETTE_COLORS ||
        !paletteImage(image, options.colors, palette, &count, &labels)) {
        return 0;
    }
    /* Reserve a bounded guard band for analyzer paths; runtime checks still reject invalid cells.
     */
    visited = calloc(total + EDGE_DIRECTION_COUNT, sizeof(*visited));
    paths = calloc(count, sizeof(*paths));
    if (!visited || !paths) {
        goto done;
    }
    context =
        (TraceContext){image, labels, palette, visited, paths, count, total, options.tolerance};
    if (!traceVectorContours(&context, &points)) {
        goto done;
    }

    outputContext = (VectorOutputContext){image, palette, paths, count, out, text, sizeof(text)};
    if (!writeVectorDocument(&outputContext) || !rcAppend(&out->resources, ">>\n", 3) ||
        !rcAppend(&out->svg, "</svg>\n", sizeof("</svg>\n") - 1) ||
        !rcAppend(&out->pdf, "Q\n", sizeof("Q\n") - 1)) {
        goto done;
    }
    out->colors = count;
    success = 1;
done:
    /* RELEASE TEMPORARY CONTOURS*/
    /* Only complete output buffers remain owned by the caller on success.*/
    if (paths) {
        for (paletteIndex = 0; paletteIndex < count; paletteIndex++) {
            free(paths[paletteIndex].svg.data);
            free(paths[paletteIndex].pdf.data);
        }
    }
    free(paths);
    free(labels);
    free(visited);
    free(points.p);
    if (!success) {
        rcFreeVector(out);
    }
    return success;
}
