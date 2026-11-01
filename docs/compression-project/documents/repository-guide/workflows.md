---
title: File workflows
description: Run byte-for-byte round trips with the repository's text and PDF samples.
---

## Pick the output extension

The input filename extension selects the processing path. The extension is checked without regard to letter case.

<table>
  <thead><tr><th>Input</th><th>Output extension</th><th>Path</th></tr></thead>
  <tbody>
    <tr><td><code>.pdf</code></td><td><code>.pdf</code></td><td>PDF image-stream processing or restoration.</td></tr>
    <tr><td>Any other extension</td><td>For example, <code>.crl</code></td><td>Reversible byte container.</td></tr>
  </tbody>
</table>

## Round-trip the text sample

This compresses `samples/repeated.txt`, restores it, then compares the original and restored bytes.

<Code language="bash">{`
set -eu
make
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

printf '1\n%s\n%s/repeated.crl\n3\n' samples/repeated.txt "$tmpdir" | ./compression-demo
printf '2\n%s/repeated.crl\n%s/recovered.txt\n3\n' "$tmpdir" "$tmpdir" | ./compression-demo
cmp samples/repeated.txt "$tmpdir/recovered.txt"
`}</Code>

`cmp` produces no output when the files match. A non-PDF compressed file needs a non-PDF extension so decompression selects the container path.

## Round-trip the PDF sample

The PDF fixture contains an eligible repeated grayscale image stream. Keep the compressed output extension as `.pdf` so decompression selects PDF restoration.

<Code language="bash">{`
set -eu
make
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

printf '1\n%s\n%s/compressed.pdf\n3\n' samples/image-stream.pdf "$tmpdir" | ./compression-demo
printf '2\n%s/compressed.pdf\n%s/restored.pdf\n3\n' "$tmpdir" "$tmpdir" | ./compression-demo
cmp samples/image-stream.pdf "$tmpdir/restored.pdf"
`}</Code>

:::note[If the PDF does not shrink]
The program copies a valid PDF unchanged when no eligible rewrite makes the complete file smaller. That copy has no restoration marker, so decompression will not treat it as a compressed PDF.
:::
