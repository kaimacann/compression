---
title: Reference and troubleshooting
description: Command syntax, build targets, C API, limitations, and common errors.
---

## Command reference

Run the executable from the repository root or provide paths relative to the current directory. Quote paths containing spaces.

<table>
  <thead><tr><th>Command</th><th>Behaviour</th></tr></thead>
  <tbody>
    <tr><td><code>./compression-demo compress &lt;input&gt; &lt;output.tcmp&gt;</code></td><td>Write a raw or Huffman <code>TCMP</code> v1 frame.</td></tr>
    <tr><td><code>./compression-demo decompress &lt;input.tcmp&gt; &lt;output&gt;</code></td><td>Validate and restore bytes from a <code>TCMP</code> frame.</td></tr>
    <tr><td><code>./compression-demo inspect &lt;input&gt;</code></td><td>Report file size and the ten most frequent byte values.</td></tr>
    <tr><td><code>./compression-demo search &lt;input&gt; &lt;literal-text&gt;</code></td><td>Search literal bytes and print matching offsets.</td></tr>
    <tr><td><code>./compression-demo --help</code></td><td>Print command syntax.</td></tr>
    <tr><td><code>./compression-demo</code></td><td>Start the interactive menu.</td></tr>
  </tbody>
</table>

The process exits with status `0` after success, `1` after an operation failure, and `2` for unsupported command syntax. Existing output paths are never replaced.

## Build targets

<table>
  <thead><tr><th>Make target</th><th>Behaviour</th></tr></thead>
  <tbody>
    <tr><td><code>make</code></td><td>Build with the configured C11 compiler and warning flags.</td></tr>
    <tr><td><code>make run</code></td><td>Build if needed, then start the interactive menu.</td></tr>
    <tr><td><code>make debug</code></td><td>Build with the <code>DEBUG</code> preprocessor symbol, symbols, and no optimisation.</td></tr>
    <tr><td><code>make release</code></td><td>Build with optimisation.</td></tr>
    <tr><td><code>make install PREFIX="$HOME/.local"</code></td><td>Install the executable under the selected prefix.</td></tr>
    <tr><td><code>make uninstall PREFIX="$HOME/.local"</code></td><td>Remove it from that prefix.</td></tr>
    <tr><td><code>make clean</code></td><td>Remove the executable and root-level analyzer output.</td></tr>
  </tbody>
</table>

The default build uses only the standard C headers `stdio.h`, `stdlib.h`, and `string.h`. It has no third-party C dependencies. The implementation uses the C11 `fopen` exclusive-create mode for output safety.

## Public C interface

`src/c/compression/compression.h` provides the path-based operations. Each function returns `1` on success and `0` on failure.

<Code language="c">{`int compressFile(const char *inputPath, const char *outputPath);
int decompressFile(const char *inputPath, const char *outputPath);
int inspectFile(const char *inputPath);
int searchFile(const char *inputPath, const char *pattern);`}</Code>

File input and exclusive output creation are isolated in `src/c/fileIO/`. The compression module owns the `TCMP` framing and Huffman tree. The interactive and command-line interfaces are in `src/c/main/main.c`.

## Troubleshooting

<table>
  <thead><tr><th>Symptom</th><th>Resolution</th></tr></thead>
  <tbody>
    <tr><td><code>cc</code> or <code>make</code> is not found</td><td>Install the platform's C toolchain and Make. If needed, select another compiler with <code>make CC=clang</code>.</td></tr>
    <tr><td>Input cannot be opened</td><td>Check the path, current directory, file permissions, and whether the file exists.</td></tr>
    <tr><td>Output already exists</td><td>Choose another destination or remove the old output yourself. The program does not overwrite files.</td></tr>
    <tr><td><code>Input is not a valid or supported TCMP v1 file</code></td><td>Check that the input is a complete <code>TCMP</code> v1 output; other compression formats are unsupported.</td></tr>
    <tr><td>Payload is truncated, corrupt, has trailing data, or fails its checksum</td><td>Recreate the container from the original input. The stored FNV-1a value is not cryptographic authentication.</td></tr>
    <tr><td><code>make</code> reports no rule or an unexpected target</td><td>Run Make from the repository root and check that <code>Makefile</code> is present.</td></tr>
    <tr><td>Docs commands are missing</td><td>Run <code>pnpm install --frozen-lockfile</code> and check the Node.js and pnpm versions in the <a href="../">getting started guide</a>.</td></tr>
  </tbody>
</table>

## Supported behaviour

<table>
  <thead><tr><th>Area</th><th>Current behaviour</th></tr></thead>
  <tbody>
    <tr><td>Input</td><td>Opaque bytes from a seekable file; file extension does not affect processing.</td></tr>
    <tr><td>Compression</td><td>Deterministic Huffman coding when its complete frame is smaller than the raw frame; otherwise raw framing.</td></tr>
    <tr><td>Decompression</td><td>Raw and Huffman <code>TCMP</code> v1 frames, with declared-size, frequency, and FNV-1a checksum validation.</td></tr>
    <tr><td>Inspection</td><td>Byte count and sorted byte-frequency summary.</td></tr>
    <tr><td>Search</td><td>Literal byte sequence search with Knuth–Morris–Pratt; no semantic document parsing.</td></tr>
    <tr><td>Output</td><td>New file only; existing destinations are rejected; incomplete outputs are removed after detected failures when the filesystem permits.</td></tr>
    <tr><td>Encryption</td><td>Not implemented by this executable.</td></tr>
    <tr><td>PDF and image processing</td><td>Not implemented; files are handled as byte streams.</td></tr>
    <tr><td>Interoperability</td><td><code>TCMP</code> is private to this repository and is not ZIP, gzip, zlib, or a published standard.</td></tr>
  </tbody>
</table>
