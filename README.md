# Compression Project Docs

Assessment documentation built with the shared Terra Docs layout.

## Local development

Install dependencies and start the local documentation site:

```sh
pnpm install
pnpm dev
```

Build the static site for verification:

```sh
pnpm build
pnpm preview
```

The assessment pages and the separate in-class API proposal live under `src/content/docs/assessment/`. The shared Docs package provides the page shell and navigation; this repository keeps its assessment-specific flow diagrams and badge components.

## C program scaffold

Build the C compression stub with `make`, then run it with `./compression-demo` or `make run`. Choose compression or decompression and enter input and output paths. Compression recognizes PDFs and image files by extension. The PDF flow reaches parsing, image-object discovery (one placeholder object), per-image compression, replacement, serialization, and file-output stubs. Image inputs reach decoding and raster mode by default; the strokes-mode branch is also stubbed. These stages do not modify or create output data yet. The menu wrappers read paths and route to the stub APIs declared in `src/c/compression/compression.h`.
