#ifndef COMPRESSION_MEDIA_H
#define COMPRESSION_MEDIA_H

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

// SHARED CONTRACT
#include "codec.h"

/*//////////////////////////////////////////////////
TYPES
//////////////////////////////////////////////////*/

// PROCESSING POLICY
/** Explicit mode, palette, contour tolerance, and derived-output policy. */
typedef struct {
    int vectorMode, force; // Lossy consent and explicit derived-file replacement.
    unsigned colors;
    double tolerance;
    const char *imagesDirectory; // Existing directory; NULL keeps sidecars beside the PDF.
} MediaOptions;
typedef struct {
    RcBuffer svg, pdf, resources;
    unsigned colors;
} RcVector;

/*//////////////////////////////////////////////////
API
//////////////////////////////////////////////////*/

// PROCESSING OPERATIONS
/** Trace adaptive colour regions to SVG and PDF paths. @author kai */
int rcVectorize(const RcImage *image, unsigned colors, double tolerance, RcVector *vector);
/** Release all vector output buffers. @author kai */
void rcFreeVector(RcVector *vector);
/** Process media and commit the selected representation.
 * @param options Explicit mode, palette, tolerance, and output replacement policy.
 * @return 1 after all outputs commit; 0 after validation or transaction failure.
 * @author kai
 */
int processMedia(const char *input, const char *output, const MediaOptions *options);
/** Build a candidate without editing source bytes.
 * @param candidate Receives owned rebuilt bytes; empty means retain the source unchanged.
 * @param images Receives owned SVG buffers; the caller releases each buffer and the array.
 * @return 1 for processed or preserved PDFs; 0 for malformed input or resource failure.
 * @author kai
 */
int processPdf(const unsigned char *data, size_t length, const MediaOptions *options,
               RcBuffer *candidate, RcBuffer **images, size_t *imageCount, unsigned *unsupported);
#endif
