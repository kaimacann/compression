---
title: Compression
description: A format- and content-aware plan for compressing image assets and PDF documents.
# Status: Research plan for review, not an implementation commitment. The official assessment brief and tutor decisions take precedence.
---

## 1. Design

Choose compression from the detected format **and contents**, not the filename or client MIME type. Distinguish, for example, screenshots from photos and mixed PDFs from single images.

- Default to lossless; change pixels, appearance, text, or behavior only by explicit lossy choice.
- Keep the original immutable; store output separately and return both sizes and checksums.
- Select a candidate only if valid and smaller; otherwise keep the original.
- Treat PDFs as structured documents: optimize eligible objects/images, not whole-page bitmaps.
- Preserve relevant color profiles, alpha, orientation, animation, searchable text, accessibility structure, forms, annotations, attachments, and signatures.
- Record detected format/content class, algorithm and mode, encoder/settings, output checksum, warnings, and result.

## 2. Images

### 2.1 Lossy

Common inputs: JPEG, high-bit-depth TIFF, PNG, WebP, AVIF.

- **Default:** Pass JPEG/WebP/AVIF through unless a supported transform avoids pixel decode/re-encode. JPEG coefficient transforms can sometimes rotate, crop, or rearrange, but generally cannot reduce quality or resize; handle metadata separately ([libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/doc/usage.txt)).
- **Opt-in lossy:** Offer JPEG, lossy WebP, or AVIF with an explicit quality/size policy; retain the source. Both formats support lossy/lossless modes and alpha; AVIF also supports HDR and wide gamut ([WebP](https://developers.google.com/speed/webp), [AVIF](https://aomedia.org/specifications/avif/)).
- Downsample only to a selected maximum dimension or delivery profile; report dimensions. Never infer display/print resolution from DPI metadata.
- Preserve ICC profiles and orientation. Metadata removal is a separate privacy choice. JPEG re-encoding, quality/chroma changes, WebP/AVIF conversion, and resizing are lossy.

Use lossless PNG: reversible row filters plus DEFLATE, with exact indexed color where possible ([PNG spec](https://www.w3.org/TR/png-3/), [RFC 2083](https://www.rfc-editor.org/rfc/rfc2083/)). Try an exact palette including transparency; otherwise retain color depth and use adaptive filters. Avoid JPEG/lossy WebP/AVIF, which create visible ringing and block artifacts around text and edges. Verify pixels and alpha exactly.

| Format      | Lossless-first policy                                                                                                                         | Optional lossy operation                                                |
| ----------- | --------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| JPEG        | Preserve; allow validated coefficient transforms or non-image metadata edits.                                                                 | Re-encode/resize to a named target.                                     |
| WebP / AVIF | Preserve if efficient or unsupported; retain alpha, animation, color, and sequence timing.                                                    | Re-encode only for an explicit target and compatible consumer.          |
| GIF         | Palette/LZW is already compressed; retain frame order, delays, disposal, transparency.                                                        | Convert only if full animation is retained and destination is selected. |
| TIFF        | Inspect each page, bit depth, and compression; test lossless DEFLATE/LZW on uncompressed or losslessly encoded pixels; retain tags/structure. | JPEG-in-TIFF/downsampling requires opt-in.                              |
| BMP         | Encode uncompressed input samples in a suitable lossless format; retain dimensions, channel values, alpha semantics.                          | Quantization or lossy output requires opt-in.                           |

General byte compression rarely helps GIF; conversions must respect animation extensions ([GIF89a](https://giflib.sourceforge.net/gifstandard/GIF89a.html)). TIFF encodings vary by page ([Adobe overview](https://helpx.adobe.com/photoshop/desktop/save-and-export/export-files-to-different-formats/file-compression-in-photoshop.html)).

### 2.2 Lossless

- Raster: exact indexed PNG, else true-color PNG or lossless WebP (alpha supported; PNG is a broad fallback; [WebP](https://developers.google.com/speed/webp), [PNG](https://www.w3.org/TR/png-3/)). Palette approximation is lossy even in PNG/GIF; require explicit visual-loss mode and report color-count changes.
- Retain alpha; flatten only onto a selected background.
- SVG: remove only inert whitespace/comments and redundant syntax with an SVG-aware parser. Gzip/DEFLATE (SVGZ) needs matching serving metadata ([minimization](https://www.w3.org/TR/SVG11/minimize.html), [server conformance](https://www.w3.org/TR/SVG11/conform.html)). Path simplification/raster-to-vector conversion can alter appearance; require tolerance and preview, else preserve.

### 2.3 Monochrome and grayscale images

Reduce bit depth or index only if decoded samples stay identical. Apply photo policy to grayscale photos; never auto-binarize. Thresholding, dithering, and reducing gray levels require explicit choice and visual comparison.

## 3. PDFs

Walk the object graph; optimize eligible streams/images in place, never rasterize every page. PDF filters (Flate, CCITT fax, JBIG2, DCT/JPEG, JPX/JPEG 2000) serve different data and require decoding plus deliberate policy to interchange ([PDF 32000-1](https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf)).

### 3.1 Structure and content streams

Flate/DEFLATE uncompressed streams; recompress Flate only if decoded bytes stay exact and output shrinks. Use object streams only if PDF version/readers support them; these and stream recompression shrink structure without image resampling ([qpdf](https://qpdf.readthedocs.io/en/latest/cli.html)). Preserve page operators, text extraction, reading order, links, and vectors.

### 3.2 Embedded photographs and color images

Inspect each image XObject's filter and dimensions:

- **DCT/JPEG:** Usually pass through. Re-encoding/downsampling needs an explicit PDF profile; qpdf may replace non-JPEG with JPEG and introduce artifacts ([qpdf](https://qpdf.readthedocs.io/en/latest/cli.html)).
- **Flate pixels:** Recompress without changing decoded samples; retain color space, bit depth, masks, and decode parameters.
- **JPX/JPEG 2000:** Preserve already-compressed input.
- **Oversized scans:** Downsample only to a selected effective-resolution target; retain the source PDF.

### 3.3 Scanned pages and monochrome content

- Bitonal text/line scans: lossless CCITT Group 4 or exact Flate; preserve existing CCITT.
- JBIG2: preserve existing streams; future encoders must be lossless because lossy symbol substitution can change characters. Never default to lossy JBIG2.
- Gray/color scans: retain Flate/JPX losslessly when possible. JPEG/JPX re-encoding, thresholding, or downsampling needs an explicit lossy profile. CCITT is typically one-bit; JBIG2 may be exact or approximate ([PDF 32000-1](https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/PDF32000_2008.pdf)).

### 3.4 Text, fonts, vectors, and document features

Keep text/vector paths as objects; compress streams losslessly. Preserve fonts, subset character maps, and `ToUnicode`; subsetting requires a PDF library aware of usage, forms, and text mapping. Retain tags/accessibility tree, forms/appearances, annotations, bookmarks, attachments, color profiles, and metadata unless instructed otherwise. Detect encryption/signatures first; never guess passwords. Rewriting can invalidate signatures without changing appearance: report this and retain the source ([qpdf](https://qpdf.readthedocs.io/en/latest/cli.html)).

### 3.5 Explicit lossy PDF profiles

If approved, offer named **web**, **screen**, or **print** profiles with documented dimensions/effective DPI and quality. Apply only to raster XObjects; state changes, retain source, and preview. Text/vector PDFs may shrink little; scans may gain more from downsampling.
