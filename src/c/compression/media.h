#ifndef COMPRESSION_MEDIA_H
#define COMPRESSION_MEDIA_H

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

/* SHARED CONTRACT*/
#include "codec.h"

/*//////////////////////////////////////////////////
TYPES
//////////////////////////////////////////////////*/

/* PROCESSING POLICY*/
/**
 * Explicit media mode and output policy.
 * @author kai
 */
typedef struct {
    /**
     * Enable lossy raster vectorization.
     * @author kai
     */
    int vectorMode;
    /**
     * Allow replacing existing media outputs.
     * @author kai
     */
    int force;
    /**
     * Maximum palette size.
     * @author kai
     */
    unsigned colors;
    /**
     * Contour simplification tolerance in pixels.
     * @author kai
     */
    double tolerance;
    /**
     * Existing sidecar directory, or NULL for beside the PDF.
     * @author kai
     */
    const char *imagesDirectory;
} MediaOptions;
/**
 * Shared vector geometry for SVG and PDF serialization.
 * @author kai
 */
typedef struct {
    /**
     * SVG image.
     * @author kai
     */
    RcBuffer svg;
    /**
     * Equivalent PDF path content.
     * @author kai
     */
    RcBuffer pdf;
    /**
     * PDF resources required by the path content.
     * @author kai
     */
    RcBuffer resources;
    /**
     * Palette entries used.
     * @author kai
     */
    unsigned colors;
} RcVector;
/** Palette and contour settings used by vector conversion. */
typedef struct {
    unsigned colors;  /**< Palette size, from two through 256. */
    double tolerance; /**< Maximum contour simplification distance. */
} RcVectorOptions;

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

/* PROCESSING OPERATIONS*/
/** Trace adaptive colour regions to SVG and PDF paths.
 * @param image Decoded RGBA source.
 * @param options Palette size and contour simplification distance.
 * @param out Receives owned SVG, PDF, and resource buffers.
 * @return Nonzero on success.
 * @author kai
 */
int rcVectorize(const RcImage *image, RcVectorOptions options, RcVector *out);
/** Release all buffers owned by a vector result.
 * @param vector Result to clear.
 * @author kai
 */
void rcFreeVector(RcVector *vector);
/** Process media and commit the selected representation.
 * @param input PNG, JPEG, or PDF source, detected by signature.
 * @param output Destination for the processed representation.
 * @param options Explicit mode, palette, tolerance, and replacement policy.
 * @return 1 after all outputs commit; 0 after validation or transaction failure.
 * @author kai
 */
int processMedia(const char *input, const char *output, const MediaOptions *options);
/** Build a candidate without editing source bytes.
 * @param data Source PDF bytes.
 * @param length Source length.
 * @param options Explicit vectorization and output policy.
 * @param candidate Receives owned rebuilt bytes; empty means retain the source unchanged.
 * @param images Receives owned SVG buffers.
 * @param imageCount Receives the number of SVG buffers.
 * @param unsupported Receives the count of retained unsupported objects.
 * @return 1 for processed or preserved PDFs; 0 for malformed input or resource failure.
 * @author kai
 */
int processPdf(const unsigned char *data, size_t length, const MediaOptions *options,
               RcBuffer *candidate, RcBuffer **images, size_t *imageCount, unsigned *unsupported);
#endif
