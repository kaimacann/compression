#include <stdio.h>

/**
 * @code
 * ~processAssetUpload(input, policy)
 * +-- ~detectInputOutput(metadata, operation) // kind of asset, kind of compression
 * +-- compressFile(file, operation)
 * |   +-- document/PDF: [API] compressDocument(documentBytes, policy)
 * |   |   +-- [API] collectPdfImageXObjects(parsedPdf)
 * |   |   +-- [API] processPdfImageXObject(parsedPdf, imageObject)
 * |   |   +-- [API] losslessCompress(input, algorithm, options)
 * |   +-- raster: [API] compressImage(decodedImage, policy)
 * |   |   +-- [API] encodeClassifiedColorData(image, classification)
 * |   |   +-- [API] encodeClassifiedBackground(image, classification)
 * |   |   +-- [API] lossyCompress(imageSamples, targetFormat, profile) [explicit]
 * |   |   +-- strokes: [API] svgise(strokeGeometry, options)
 * +-- decompressFile(file, operation)
 * |   +-- [API] decompressDocument(encodedBytes, codec)
 * |   +-- [API] decompressImage(encodedBytes, format, codec)
 * @endcode
 */

/**
 * Accepts a pdf, for each image in pdfs run image compression
 *
 * @param file pdf containing images
 */
int compressFile(file) {
    // INTRODUCTORY MESSAGES
    printf("Settings:");

    // PROCESS
    image[] images = collectPdfImageXObjects(file);
    image[] compressedImages;
    // lossless
    for (image in images) {
        losslessCompress(image, algorithm, options);
    }
    // + lossy
    if (rasterise) {
        for (image in images) {
            rasterise(image, algorithm, options);
        }
    }

    processPdfImageXObject(files, compressedImages);

    // SAFE RETURN
    return 0;
}

int raster(image) {
    // INTRODUCTORY MESSAGES
    printf("Settings:\n");
    printf("Colour Compression 1 (black or white) - 10 (existing fidelity): \n");
    printf("Fidelity Compression 1 (black or white) - 10 (existing fidelity): ");

    // PROCESS
    encodeClassifiedColorData(image, classification);
    lossyCompress(imageSamples, targetFormat, profile)[explicit];

    // SAFE RETURN
    return 0;
}

int decompressFile(file) {
    // INTRODUCTORY MESSAGES
    printf("Settings:\n");

    // PROCESS
    if (file.type == "Document")
        decompressDocument(encodedBytes, codec);
    else
        decompressImage(encodedBytes, format, codec);

    // SAFE RETURN
    return 0;
}

// HELPERS
int losslessCompress() {}
int rasterise() {}
int processPdfImageXObject() {}
