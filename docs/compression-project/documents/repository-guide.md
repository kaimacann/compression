---
title: Repository guide
description: "Build, run, and understand the current C compression demo."
---

This guide describes the code in this repository as it works today. The API proposal and compression plan are design documents; they describe ideas beyond the current demo.

## What is in the repository

The repository has two parts:

- A standalone C11 program that compresses or restores one file at a time.
- A documentation site with the assessment brief, submission guidance, project proposal, compression plan, and this implementation guide.

The C program supports a reversible byte container for non-PDF files and conservative run-length compression for eligible PDF image streams. It does not decode image pixels, perform lossy compression, encrypt files, or convert strokes to SVG.

## Requirements

To build and run the C demo, install a C11 compiler available as `cc` and `make`.

To run or build the documentation site, install Node.js and pnpm 10.30.3, the version declared by the root `package.json`.

## Build and launch the demo

From the repository root:

```sh
make
./compression-demo
```

`make run` builds the executable if needed and opens the same interactive menu. The menu accepts `1` to compress a file, `2` to decompress one, and `3` to exit. For each operation, enter an input path and a separate output path. The program rejects identical path strings and prints whether the operation completed. Choose an output path that names a different file to preserve the source.

The program has no command-line flags. It reads the entire input file into memory, so available memory limits the size of files it can process.

## Round-trip a text sample

The menu can also be driven from a POSIX shell. This example compresses the repeated-text fixture, restores it, and checks that the bytes match:

```sh
set -eu
make
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

printf '1\n%s\n%s/repeated.crl\n3\n' samples/repeated.txt "$tmpdir" | ./compression-demo
printf '2\n%s/repeated.crl\n%s/recovered.txt\n3\n' "$tmpdir" "$tmpdir" | ./compression-demo
cmp samples/repeated.txt "$tmpdir/recovered.txt"
```

`cmp` is silent when the restored file is byte-for-byte identical. The output extension is `.crl` so the decompressor selects the container path.

## Round-trip the PDF fixture

The fixture contains an eligible repeated grayscale image stream. Keep the compressed PDF extension as `.pdf` so the decompressor selects PDF restoration:

```sh
set -eu
make
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

printf '1\n%s\n%s/compressed.pdf\n3\n' samples/image-stream.pdf "$tmpdir" | ./compression-demo
printf '2\n%s/compressed.pdf\n%s/restored.pdf\n3\n' "$tmpdir" "$tmpdir" | ./compression-demo
cmp samples/image-stream.pdf "$tmpdir/restored.pdf"
```

PDF compression may report that no smaller stream was found. In that case it writes the original PDF unchanged; that copy has no marker for this program to restore.

## How compression works

### Non-PDF files

File type is selected from the input filename extension, case-insensitively. A `.pdf` extension selects the PDF path; every other extension—including `.png`, `.jpg`, and `.txt`—uses the byte-container path. The program does not identify an image from its content or change its pixels.

The `CRL1` container has a 17-byte header containing the magic value, a storage mode, the original size, and a CRC-32 checksum. The payload is either:

- PDF-compatible run-length encoded bytes, when the encoded payload is smaller than the source bytes; or
- The original bytes in stored mode, when run-length encoding would expand the payload.

The header adds space, so a container can still be larger than its input even when run-length encoding saves a few payload bytes. Decompression checks the mode, decoded length, and checksum before writing the restored bytes.

### PDFs

The PDF path scans image XObjects in a restricted PDF structure. It can process an image stream only when the stream has a direct `/Length` value and no existing `/Filter`. For an eligible stream, the program adds a `/RunLengthDecode` filter and a private marker, updates the stream length, and rebuilds the classic cross-reference table.

The rewritten PDF is used only when the complete document is smaller. If the structure is unsupported or the file does not shrink, compression copies the original PDF to the output. Decompression removes only the run-length filter and marker that this program added.

This is not a general-purpose PDF optimizer. The current parser requires a single classic cross-reference table and direct stream lengths; encrypted, signed (`/ByteRange`), incremental, linearized, xref-stream, and object-stream PDFs are not supported. Existing image filters are left alone.

## Public C API

[`src/c/compression/compression.h`](../../../src/c/compression/compression.h) declares the module's two public operations:

```c
int compressFile(const char *inputPath, const char *outputPath);
int decompressFile(const char *inputPath, const char *outputPath);
```

Both functions operate on paths and return `1` when processing and output writing succeed, or `0` on invalid input or an I/O/format error. A valid but unsupported or non-shrinking PDF is copied unchanged by `compressFile`, so success does not guarantee a smaller file. Internal encoding and PDF parsing helpers remain private to `compression.c`.

## Troubleshooting

- **“No smaller eligible PDF image stream was found”** means the PDF was copied unchanged. That output has no marker for this program to restore, so decompression will report that it has no supported RLE image streams.
- **“The input is not a valid CRL1 container”** means the non-PDF input is not a container produced by this program, or its bytes are damaged. Check that you selected the compressed file.
- **Decompression rejects a PDF** when it has no image stream marked by this program. A PDF that was copied unchanged cannot be restored by this program.
- **The menu reports failure** if a path cannot be read or written, the input is malformed, or input and output use the same path string. Check the paths and ensure the destination directory exists.

## Repository layout

| Path                             | Purpose                                                                      |
| -------------------------------- | ---------------------------------------------------------------------------- |
| `src/c/main/`                    | Interactive menu and path input.                                             |
| `src/c/compression/`             | Public file operations, CRL1 encoding, run-length coding, and PDF rewriting. |
| `src/c/fileIO/fileInput/`        | Reads the complete input file and routes by extension.                       |
| `src/c/fileIO/fileOutput/`       | Writes the output bytes.                                                     |
| `samples/`                       | Text, image, and PDF inputs for manual workflows.                            |
| `docs/compression-project/at3/`  | Assessment brief and submission guidance.                                    |
| `docs/compression-project/plan/` | Proposal and research notes; not a description of implemented features.      |

## Documentation and validation commands

Install the documentation dependencies before running these commands:

```sh
pnpm install
pnpm docs:dev
```

Other available documentation commands are:

```sh
pnpm docs:build
pnpm docs:preview
pnpm check:markdown
pnpm format:markdown
```

`docs:build` writes the static site to `dist/docs/`. `docs:preview` builds the site before serving it. `format:markdown` writes formatting changes; `check:markdown` checks without changing files.

For the configured C quality checks, run:

```sh
pnpm exec terra-quality quality --all --warn
```

This also runs the configured `make` build. The complete C profile requires `clang-format`, `clang`, `clang-tidy`, and Doxygen to be available on `PATH`. To remove the demo executable and analyzer output, run `make clean`.
