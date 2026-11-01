# Compression project

This repository contains a C11 command-line compression demo and a Markdown documentation site for the assessment and project proposals. The [repository guide](docs/compression-project/documents/repository-guide.md) describes how the current code works and includes copyable workflows.

## Quick start

You need a C11 compiler available as `cc` and `make`.

```sh
make
./compression-demo
```

Or build and launch the interactive menu with `make run`. Choose **1** to compress a file, **2** to decompress one, or **3** to exit. Use separate input and output paths; identical path strings are rejected.

## Choose the output extension

- Use a `.pdf` output path when compressing a PDF. The decompressor uses the extension to choose its PDF restoration path.
- Use a non-PDF extension such as `.crl` for other files. Text and image files are wrapped as bytes; image pixels are not decoded or recompressed.

The sample inputs are in [`samples/`](samples/). The guide explains supported formats, limitations, round-trip commands, the public C API, and documentation workflows.

## Documentation site

Install the Node dependencies, then start the local site:

```sh
pnpm install
pnpm docs:dev
```

Build the static site with `pnpm docs:build` or build and preview it with `pnpm docs:preview`.
