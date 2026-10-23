/**
 * @owner: kaimacann
 * @file compression.h
 * @brief Compression API declarations.
 *
 * Data structures are opaque API types. The declarations below are planned
 * entry points; their current definitions are stubs for the menu demo.
 *
 * @code
 * main() -> showMainMenu()
 * +-- compressFile(inputPath, outputPath)
 * |   +-- fileIO.readInputFile(inputPath)
 * |   +-- compress(input, policy)
 * |       +-- PDF/document: compressDocument(input, policy)
 * |       |   +-- PDF: parse -> collect image XObjects
 * |       |       +-- for each image: process -> losslessCompress
 * |       |       +-- replace each accepted image candidate [stub]
 * |       |       +-- serialize PDF [stub]
 * |       +-- image: decode -> compressImage(decodedImage, policy)
 * |           +-- raster mode: classified encoders / explicitly lossy encoder
 * |           +-- strokes mode: svgise
 * |   +-- fileIO.writeOutputFile(outputPath)
 * +-- decompressFile(inputPath, outputPath)
 *     +-- fileIO.readInputFile(inputPath)
 *     +-- decompressDocument / decompressImage / decompress
 *     +-- fileIO.writeOutputFile(outputPath)
 * @endcode
 */

#ifndef COMPRESSION_H
#define COMPRESSION_H

/*//////////////////////////////////////////////////
IMPORTS
//////////////////////////////////////////////////*/

#include <stdlib.h>

/*//////////////////////////////////////////////////
OPAQUE API TYPES
//////////////////////////////////////////////////*/

/* SHARED INPUT AND POLICY */
typedef struct CompressionInput CompressionInput;
typedef struct CompressionPolicy CompressionPolicy;
typedef struct CompressionAlgorithm CompressionAlgorithm;
typedef struct CompressionCodec CompressionCodec;
typedef struct CodecOptions CodecOptions;
typedef struct ImageFormat ImageFormat;
typedef struct LossyProfile LossyProfile;
typedef struct SvgOptions SvgOptions;

/* PDF OBJECTS */
typedef struct ParsedPdf ParsedPdf;
typedef struct PdfImageObject PdfImageObject;
typedef struct PdfImageXObjectList PdfImageXObjectList;
typedef struct PdfImageCandidate PdfImageCandidate;

/* DOCUMENT RESULTS */
typedef struct DocumentDecodeResult DocumentDecodeResult;
typedef struct DocumentCompressionResult DocumentCompressionResult;

/* IMAGE DATA AND RESULTS */
typedef struct DecodedImage DecodedImage;
typedef struct ImageCompressionResult ImageCompressionResult;
typedef struct ImageClassification ImageClassification;
typedef struct EncodedImageCandidate EncodedImageCandidate;
typedef struct ImageDecodeResult ImageDecodeResult;
typedef struct LosslessCompressionResult LosslessCompressionResult;
typedef struct LossyCompressionResult LossyCompressionResult;

/* VECTOR AND GENERAL DECOMPRESSION RESULTS */
typedef struct StrokeGeometry StrokeGeometry;
typedef struct SvgSerializationResult SvgSerializationResult;
typedef struct DecompressionResult DecompressionResult;

/*//////////////////////////////////////////////////
PUBLIC API
//////////////////////////////////////////////////*/

/* MENU-DRIVEN FILE FLOWS */
/**
 * Runs the file-level compression flow for a source and destination path.
 * The demo distinguishes PDF/document input from image input, then calls the
 * corresponding compression stubs. Encoding and output writing remain stubs.
 *
 * @param inputPath Path of the source file.
 * @param outputPath Path where encoded bytes should be written.
 * @return 0 while the demo implementation is a stub.
 */
int compressFile(const char *inputPath, const char *outputPath);

/**
 * Runs the file-level restoration flow for an encoded source and destination.
 *
 * The demo reads the source bytes, then calls decoder and output-writer stubs;
 * it does not decode or write the restored bytes.
 *
 * @param inputPath Path of the encoded file.
 * @param outputPath Path where restored bytes should be written.
 * @return 0 while the demo implementation is a stub.
 */
int decompressFile(const char *inputPath, const char *outputPath);

/* PDF OBJECT PROCESSING */
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
PdfImageCandidate *processPdfImageXObject(const ParsedPdf *parsedPdf,
                                          const PdfImageObject *imageObject);

/* DOCUMENT PROCESSING */
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
DocumentDecodeResult *decompressDocument(const CompressionInput *encodedBytes,
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
DocumentCompressionResult *compressDocument(const CompressionInput *documentBytes,
                                            const CompressionPolicy *policy);

/* RASTER IMAGE PROCESSING */
/**
 * @owner: kaimacann
 * @brief compressImage: produce and evaluate an image candidate by policy mode.
 *
 * Raster and strokes are image-processing modes. Prefer reversible raster
 * encoding; allow changes to samples, dimensions, or appearance only when loss
 * is explicitly permitted. Preserve alpha and relevant metadata, and retain the source.
 *
 * @param decodedImage Samples, layout, and reconstruction metadata.
 * @param policy Allowed formats, mode, limits, and encoder settings. References:
 *   [PNG filters](https://www.w3.org/TR/png-3/) and
 *   [WebP coding techniques](https://developers.google.com/speed/webp/docs/compression).
 * @return Selected image, settings, sizes, checksums, warnings, and status; may
 *   select the source if no valid smaller candidate is produced.
 */
ImageCompressionResult *compressImage(const DecodedImage *decodedImage,
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
EncodedImageCandidate *encodeClassifiedColorData(const DecodedImage *image,
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
 *   [PackBits RLE](https://download.osgeo.org/libtiff/doc/TIFF6.pdf) is a reference, not a required
 * encoding.
 * @return Encoded candidate and status; candidate size selection belongs to caller.
 */
EncodedImageCandidate *encodeClassifiedBackground(const DecodedImage *image,
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
ImageDecodeResult *decompressImage(const CompressionInput *encodedBytes, const ImageFormat *format,
                                   const CompressionCodec *codec);

/* BYTE AND VECTOR CODECS */
/**
 * @owner: kaimacann
 * @brief losslessCompress: build a reversible candidate from bytes or samples.
 *
 * Input is already read, format-detected, and classified. For PDF input, it is one extracted
 * image stream prepared by document processing. The caller validates the candidate and updates
 * the PDF structure. The candidate may be larger than the source.
 *
 * @param input Source bytes or samples and length.
 * @param algorithm Selected lossless technique (references, not requirements):
 *   [PackBits RLE (TIFF 6.0 §9)](https://download.osgeo.org/libtiff/doc/TIFF6.pdf),
 *   [DEFLATE: LZ77 + Huffman (RFC 1951)](https://www.rfc-editor.org/rfc/rfc1951),
 *   [PNG filters + DEFLATE](https://www.w3.org/TR/png-3/),
 *   [CCITT Group 4 (ITU-T T.6)](https://www.itu.int/rec/T-REC-T.6/en).
 * @param options Codec settings and output limits.
 * @return Candidate bytes, length, algorithm, and status. Private codec helpers need separate
 * documentation only if exposed publicly.
 */
LosslessCompressionResult *losslessCompress(const CompressionInput *input,
                                            const CompressionAlgorithm *algorithm,
                                            const CodecOptions *options);

/**
 * @owner: kaimacann
 * @brief lossyCompress: build an explicitly lossy raster candidate.
 *
 * Never alter PDF text, vectors, or structure; never use as an implicit fallback.
 * The external processor owns consent, source retention, previews, metadata, and candidate
 * selection.
 *
 * @param imageSamples Decoded raster samples and layout.
 * @param targetFormat Consumer-supported image format; technique reference:
 *   [WebP coding techniques](https://developers.google.com/speed/webp/docs/compression).
 * @param profile Approved quality/dimension limits and encoder settings.
 * @return Lossy candidate bytes, applied settings, and status.
 */
LossyCompressionResult *lossyCompress(const DecodedImage *imageSamples,
                                      const ImageFormat *targetFormat, const LossyProfile *profile);

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
SvgSerializationResult *svgise(const StrokeGeometry *strokeGeometry, const SvgOptions *options);

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
DecompressionResult *decompress(const CompressionInput *compressedInput,
                                const CompressionAlgorithm *algorithm, size_t maxOutputBytes);

#endif /* COMPRESSION_H */
