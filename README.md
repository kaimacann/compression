# Compression project

This repository contains a C11 file-compression command-line program and a Markdown documentation site. The executable supports lossless Huffman compression and decompression in the project-specific `TCMP` v1 container, byte-frequency inspection, and literal text search. The container records and validates an FNV-1a 64 checksum of the original bytes; this checksum is not cryptographic authentication.

## Build and run

Install a C11 compiler and GNU Make, then build from the repository root:

```sh
make
./compression-demo --help
```

For a user-local installation, run `make install PREFIX="$HOME/.local"` and add `$HOME/.local/bin` to `PATH`. Remove it with `make uninstall PREFIX="$HOME/.local"`.

Compress and restore a file:

```sh
./compression-demo compress samples/sample.txt /tmp/sample.tcmp
./compression-demo decompress /tmp/sample.tcmp /tmp/sample-restored.txt
cmp samples/sample.txt /tmp/sample-restored.txt
```

The tool never replaces an existing output. Choose a new destination path before each run. Huffman coding is selected only when its container is smaller than the raw-frame option; otherwise the container stores the original bytes unchanged.

## Available commands

```text
compression-demo compress <input> <output.tcmp>
compression-demo decompress <input.tcmp> <output>
compression-demo inspect <input>
compression-demo search <input> <literal-text>
compression-demo                # interactive menu
```

`inspect` sorts byte-frequency records with merge sort. `search` uses Knuth–Morris–Pratt and reports matching byte offsets. Both commands accept arbitrary file bytes; the search expression is literal command-line text.

This program does not encrypt files, process PDF objects or image pixels, or implement an industry-standard archive format. `TCMP` is a versioned format for this repository. Do not use it as a security, backup, or interchange format.

## Documentation site

The Docs site requires Node.js 24.12 or later and pnpm 10.30.3:

```sh
pnpm install --frozen-lockfile
pnpm docs:dev
```

Use `pnpm docs:build` to build static pages or `pnpm docs:preview` to build and serve them locally. See the [Compression guide](docs/compression/) for installation, file handling, command reference, and troubleshooting. Assignment materials and authored proposals remain under [Assessment](docs/assessment/).
