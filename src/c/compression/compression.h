#ifndef COMPRESSION_H
#define COMPRESSION_H

#include <stdlib.h>

/**
 * @owner: kaimacann
 * @file compression.h
 * @brief Compression API declarations.
 *
 * Data structures are opaque API types. Prototypes marked [API] are declared
 * below; their definitions belong in implementation files. Names marked ~ are
 * external placeholders, not declared or defined by this header. They were
 * included provisionally, pending contributions from other group members.
 *
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

typedef struct ParsedPdf ParsedPdf;
typedef struct PdfImageObject PdfImageObject;
typedef struct PdfImageXObjectList PdfImageXObjectList;
typedef struct PdfImageCandidate PdfImageCandidate;
typedef struct CompressionCodec CompressionCodec;
typedef struct DocumentDecodeResult DocumentDecodeResult;
typedef struct CompressionPolicy CompressionPolicy;
typedef struct DocumentCompressionResult DocumentCompressionResult;
typedef struct DecodedImage DecodedImage;
typedef struct ImageCompressionResult ImageCompressionResult;
typedef struct ImageClassification ImageClassification;
typedef struct EncodedImageCandidate EncodedImageCandidate;
typedef struct ImageFormat ImageFormat;
typedef struct ImageDecodeResult ImageDecodeResult;
typedef struct CompressionInput CompressionInput;
typedef struct CompressionAlgorithm CompressionAlgorithm;
typedef struct CodecOptions CodecOptions;
typedef struct LosslessCompressionResult LosslessCompressionResult;
typedef struct LossyProfile LossyProfile;
typedef struct LossyCompressionResult LossyCompressionResult;
typedef struct StrokeGeometry StrokeGeometry;
typedef struct SvgOptions SvgOptions;
typedef struct SvgSerializationResult SvgSerializationResult;
typedef struct DecompressionResult DecompressionResult;

/**
 * @owner: kaimacann
 * @brief compressFile: write a byte-level RLE encoding of a file.
 *
 * Each encoded run is stored as a one-byte count (1–255) followed by its byte
 * value. The encoding is lossless but can be larger than the source.
 *
 * @param file Path of the source file.
 * @param operation Type of compression (e.g. lossy, lossless, svg) of the file.
 * @return 1 on success, 0 on an I/O or argument error.
 */
int compressFile(const char *inputPath, const char *outputPath);

/**
 * @owner: kaimacann
 * @brief decompressFile: restore a file encoded by compressFile.
 *
 * Rejects zero-length runs and incomplete count/value records. Output is
 * streamed, so no full-file buffer is allocated.
 *
 * @param file Pointer to the RLE-encoded file.
 * @param operation The decompression operation specifics.
 * @return 1 on success, 0 on malformed input or an I/O/argument error.
 */
int decompressFile(const char *inputPath, const char *outputPath);

/**
 * @owner: kaimacann
 * @brief collectPdfImageXObjects: find eligible PDF image XObjects.
 *
 * Walk the object graph without decoding or changing streams; leave text,
 * vectors, page operators, and unrelated objects untouched.
 *
 * @param parsedPdf Parsed PDF object graph.
 * @return Object references with placement, mask, and metadata context.
 */
PdfImageXObjectList *collectPdfImageXObjects(const ParsedPdf *parsedPdf);

/**
 * @owner: kaimacann
 * @brief processPdfImageXObject: build a candidate for one PDF image XObject.
 *
 * Keep the source until accepted; preserve dimensions, color space, bit depth,
 * masks, decode parameters, and placement unless policy allows changes. Leave
 * page content, text, and vectors untouched.
 *
 * @param parsedPdf PDF containing the object.
 * @param imageObject Reference returned by collectPdfImageXObjects.
 * @return Replacement candidate and metadata, or a preserve-unchanged result.
 */
PdfImageCandidate *processPdfImageXObject(
    const ParsedPdf *parsedPdf,
    const PdfImageObject *imageObject);

/**
 * @owner: kaimacann
 * @brief decompressDocument: decode bytes with their matching codec.
 *
 * Reject malformed or unsupported input without partial output. This operates
 * on bytes and does not parse or rewrite document structure.
 *
 * @param encodedBytes Complete encoded document bytes.
 * @param codec Matching decoder and parameters; see
 *   [DEFLATE (RFC 1951)](https://www.rfc-editor.org/rfc/rfc1951).
 * @return Decoded bytes and status, or an error.
 */
DocumentDecodeResult *decompressDocument(
    const CompressionInput *encodedBytes,
    const CompressionCodec *codec);

/**
 * @owner: kaimacann
 * @brief compressDocument: produce a document candidate under policy.
 *
 * For PDFs, optimize eligible streams and image XObjects. Do not rasterize pages
 * or alter text, vectors, forms, annotations, or accessibility data. Keep the
 * original if processing fails or no valid smaller candidate is produced.
 *
 * @param documentBytes Complete source document.
 * @param policy Allowed codecs, limits, and explicit transformations.
 * @return Selected bytes, sizes, checksums, processing details, warnings, and status.
 */
DocumentCompressionResult *compressDocument(
    const CompressionInput *documentBytes,
    const CompressionPolicy *policy);

/**
 * @owner: kaimacann
 * @brief compressImage: produce and evaluate a raster-image candidate.
 *
 * Prefer reversible encoding. Allow changes to samples, dimensions, or appearance
 * only when loss is explicitly permitted; preserve alpha and relevant metadata,
 * and retain the source.
 *
 * @param decodedImage Samples, layout, and reconstruction metadata.
 * @param policy Allowed formats, mode, limits, and encoder settings. References:
 *   [PNG filters](https://www.w3.org/TR/png-3/) and
 *   [WebP coding techniques](https://developers.google.com/speed/webp/docs/compression).
 * @return Selected image, settings, sizes, checksums, warnings, and status; may
 *   select the source if no valid smaller candidate is produced.
 */
ImageCompressionResult *compressImage(
    const DecodedImage *decodedImage,
    const CompressionPolicy *policy);

/**
 * @owner: kaimacann
 * @brief encodeClassifiedColorData: losslessly encode classified colour data.
 *
 * Preserve every sample and alpha value. Unsupported or uncertain classifications
 * should let the caller retain the source.
 *
 * @param image Decoded samples, layout, and metadata.
 * @param classification Validated colour-data classification and encoding details; see
 *   [classification policy](../../content/docs/plan/0-overview.md#8-image-processing-policy)
 *   and [PNG indexed-colour encoding](https://www.w3.org/TR/png-3/).
 * @return Encoded candidate and status; candidate size selection belongs to caller.
 */
EncodedImageCandidate *encodeClassifiedColorData(
    const DecodedImage *image,
    const ImageClassification *classification);

/**
 * @owner: kaimacann
 * @brief encodeClassifiedBackground: losslessly encode a repeated background.
 *
 * Preserve exact samples, dimensions, channel layout, and alpha. Do not flatten
 * transparency or approximate colours.
 *
 * @param image Decoded samples, layout, and metadata.
 * @param classification Validated classification; see the
 *   [classification policy](../../content/docs/plan/0-overview.md#8-image-processing-policy).
 *   [PackBits RLE](https://download.osgeo.org/libtiff/doc/TIFF6.pdf) is a reference, not a required encoding.
 * @return Encoded candidate and status; candidate size selection belongs to caller.
 */
EncodedImageCandidate *encodeClassifiedBackground(
    const DecodedImage *image,
    const ImageClassification *classification);

/**
 * @owner: kaimacann
 * @brief decompressImage: decode image bytes with the matching format and codec.
 *
 * Reject malformed, unsupported, or inconsistent input without partial pixels.
 * Lossy decoding returns only the encoded approximation.
 *
 * @param encodedBytes Complete encoded image.
 * @param format Image format identifier.
 * @param codec Decoder and parameters matching that format.
 * @return Decoded samples, layout, metadata, and status, or an error.
 */
ImageDecodeResult *decompressImage(
    const CompressionInput *encodedBytes,
    const ImageFormat *format,
    const CompressionCodec *codec);

/**
 * @owner: kaimacann
 * @brief losslessCompress: build a reversible candidate from bytes or samples.
 *
 * Input is already read, format-detected, and classified. PDF input is one eligible extracted stream; PDF parsing and rewriting stay external.
 * The candidate may be larger, so the caller validates and selects it.
 *
 * @param input Source bytes or samples and length.
 * @param algorithm Selected lossless technique (references, not requirements):
 *   [PackBits RLE (TIFF 6.0 §9)](https://download.osgeo.org/libtiff/doc/TIFF6.pdf),
 *   [DEFLATE: LZ77 + Huffman (RFC 1951)](https://www.rfc-editor.org/rfc/rfc1951),
 *   [PNG filters + DEFLATE](https://www.w3.org/TR/png-3/),
 *   [CCITT Group 4 (ITU-T T.6)](https://www.itu.int/rec/T-REC-T.6/en).
 * @param options Codec settings and output limits.
 * @return Candidate bytes, length, algorithm, and status. Private codec helpers need separate documentation only if exposed publicly.
 */
LosslessCompressionResult *losslessCompress(
    const CompressionInput *input,
    const CompressionAlgorithm *algorithm,
    const CodecOptions *options);

/**
 * @owner: kaimacann
 * @brief lossyCompress: build an explicitly lossy raster candidate.
 *
 * Never alter PDF text, vectors, or structure; never use as an implicit fallback.
 * The external processor owns consent, source retention, previews, metadata, and candidate selection.
 *
 * @param imageSamples Decoded raster samples and layout.
 * @param targetFormat Consumer-supported image format; technique reference:
 *   [WebP coding techniques](https://developers.google.com/speed/webp/docs/compression).
 * @param profile Approved quality/dimension limits and encoder settings.
 * @return Lossy candidate bytes, applied settings, and status.
 */
LossyCompressionResult *lossyCompress(
    const DecodedImage *imageSamples,
    const ImageFormat *targetFormat,
    const LossyProfile *profile);

/**
 * @owner: kaimacann
 * @brief svgise: serialize validated stroke geometry as deterministic SVG.
 *
 * Does not read image files, classify pixels, sanitize source markup, or simplify geometry.
 * Approximation needs an explicit tolerance and review; retain the source.
 *
 * @param strokeGeometry Validated strokes and coordinates in the
 *   [SVG 2 format](https://www.w3.org/TR/SVG2/).
 * @param options SVG units, precision, and serialization settings.
 * @return SVG bytes and status; invalid geometry returns an error.
 */
SvgSerializationResult *svgise(
    const StrokeGeometry *strokeGeometry,
    const SvgOptions *options);

/**
 * @owner: kaimacann
 * @brief decompress: decode a byte/sample stream using its matching codec.
 *
 * Reject malformed data and output over the size limit. Lossless codecs restore exact input;
 * lossy codecs return only their approximation. File access and format parsing stay external.
 *
 * @param compressedInput Encoded bytes and length.
 * @param algorithm Matching decoder; see the lossless technique references above.
 * @param maxOutputBytes Maximum decoded size.
 * @return Restored bytes/samples and status, or an error without partial output.
 */
DecompressionResult *decompress(
    const CompressionInput *compressedInput,
    const CompressionAlgorithm *algorithm,
    size_t maxOutputBytes);

#endif /* COMPRESSION_H */
