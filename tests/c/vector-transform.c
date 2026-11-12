#include "../../src/c/compression/media.h"

#include <stdio.h>

/**
 * Verify a small RGBA image produces repeatable SVG and PDF vector data.
 * @return Nonzero when both outputs are complete and byte-for-byte deterministic.
 */
static int vectorizationIsDeterministic(void) {
    unsigned char pixels[] = {
        255, 0, 0, 255, 255, 0, 0, 255, 0, 0, 255, 255, 0, 0, 255, 255,
    };
    RcImage image = {2, 2, pixels};
    RcVector first = {0}, second = {0};
    int success = rcVectorize(&image, 2, 0, &first) && rcVectorize(&image, 2, 0, &second) &&
                  first.colors == 2 && second.colors == 2 && first.svg.size > 0 &&
                  first.pdf.size > 0 && first.resources.size > 0 &&
                  first.svg.size == second.svg.size && first.pdf.size == second.pdf.size &&
                  first.resources.size == second.resources.size &&
                  !memcmp(first.svg.data, second.svg.data, first.svg.size) &&
                  !memcmp(first.pdf.data, second.pdf.data, first.pdf.size) &&
                  !memcmp(first.resources.data, second.resources.data, first.resources.size);

    rcFreeVector(&first);
    rcFreeVector(&second);
    return success;
}

/**
 * Run the deterministic in-memory vector transform check.
 * @return Zero on repeatable vector output, or one after a failed assertion.
 */
int main(void) {
    if (!vectorizationIsDeterministic()) {
        fputs("SVG and PDF vector output was empty or nondeterministic.\n", stderr);
        return 1;
    }
    return 0;
}
