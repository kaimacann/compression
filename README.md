# Compression

Assessment documentation built from root Markdown files with the shared `@terra-symposium/docs` site.

## Local development

Install dependencies and start the local documentation site:

```sh
pnpm install
pnpm docs:dev
```

Build the static site into `dist/docs/`:

```sh
pnpm docs:build
pnpm docs:preview
```

The six authored pages live in the root `docs/` folder: the official assessment brief, code and report submission guides, and the compression API proposal and plan. The Docs package supplies routes, 404, reader layout, styling, navigation, and search from those files. Add `.md` or `.mdx` pages to extend the site.

## C program scaffold

Build the C compression stub with `make`, then run it with `./compression-demo` or `make run`. Choose compression or decompression and enter input and output paths. Compression recognizes PDFs and image files by extension. The PDF flow reaches parsing, image-object discovery (one placeholder object), per-image compression, replacement, serialization, and file-output stubs. Image inputs reach decoding and raster mode by default; the strokes-mode branch is also stubbed. These stages do not modify or create output data yet. The menu wrappers read paths and route to the stub APIs declared in `src/c/compression/compression.h`.

Sample inputs are in `samples/`: `sample.txt`, `bald.png`, and `duck.png`.
