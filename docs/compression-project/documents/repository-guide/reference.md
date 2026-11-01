---
title: API and limits
description: Public C entry points, format boundaries, repository map, and common errors.
---

## Public C API

`src/c/compression/compression.h` exports two path-based operations:

<Code language="c">{`int compressFile(const char *inputPath, const char *outputPath);
int decompressFile(const char *inputPath, const char *outputPath);`}</Code>

Both return `1` when processing and writing succeed, or `0` for invalid input, an unsupported compressed format, or an I/O error. A valid PDF copied unchanged still counts as successful compression.

## Support boundaries

<table>
  <thead><tr><th>Area</th><th>Current behaviour</th></tr></thead>
  <tbody>
    <tr><td>General files</td><td>Reversible <code>CRL1</code> byte container using run-length encoding or stored bytes.</td></tr>
    <tr><td>PDF images</td><td>Run-length processing for eligible image XObjects in a restricted PDF structure.</td></tr>
    <tr><td>Unsupported PDF structures</td><td>Encrypted, signed (<code>/ByteRange</code>), incremental, linearised, xref-stream, and object-stream PDFs are not supported.</td></tr>
    <tr><td>Image processing</td><td>No pixel decoding, raster recompression, lossy processing, or SVG conversion.</td></tr>
    <tr><td>Encryption</td><td>Not implemented by the current compression demo.</td></tr>
    <tr><td>Input size</td><td>The complete file is held in memory while it is processed.</td></tr>
  </tbody>
</table>

## Troubleshooting

<table>
  <thead><tr><th>Message or result</th><th>What to check</th></tr></thead>
  <tbody>
    <tr><td>No smaller eligible PDF image stream was found</td><td>The PDF was copied unchanged and has no marker for restoration.</td></tr>
    <tr><td>Input is not a valid CRL1 container</td><td>Choose a container created by this program or check whether its bytes were damaged.</td></tr>
    <tr><td>PDF has no supported RLE image streams</td><td>The input may be an unchanged copy or a PDF without this program's marker.</td></tr>
    <tr><td>The menu reports failure</td><td>Check the paths, destination directory, input format, and available memory.</td></tr>
  </tbody>
</table>

## Repository map

| Path                                | Role                                                                    |
| ----------------------------------- | ----------------------------------------------------------------------- |
| `src/c/main/`                       | Interactive menu and path input.                                        |
| `src/c/compression/`                | Public file operations, CRL1 coding, and PDF rewriting.                 |
| `src/c/fileIO/fileInput/`           | Whole-file reads and extension detection.                               |
| `src/c/fileIO/fileOutput/`          | Binary output writes.                                                   |
| `src/c/encryption/`, `src/c/login/` | Source areas outside the standalone `compression-demo` Makefile target. |
| `samples/`                          | Text, image, and PDF inputs for manual workflows.                       |
| `docs/compression-project/`         | Assessment pages and this repository guide.                             |

The compression demo is built from the menu, file input, file output, and compression sources listed in the root `Makefile`.
