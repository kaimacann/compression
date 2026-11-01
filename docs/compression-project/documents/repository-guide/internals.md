---
title: How it works
description: Follow the current byte-container and PDF image-stream processing paths.
---

## Non-PDF files

Every non-PDF input is treated as bytes, including `.png`, `.jpg`, and `.txt` files. The program does not inspect image pixels or change document formatting.

The `CRL1` container has a 17-byte header containing its magic value, storage mode, original size, and CRC-32 checksum. Its payload is either run-length encoded when that saves space, or stored unchanged when encoding would expand it. The header can still make the complete container larger than a small input.

During restoration, the program checks the container mode, decoded size, and checksum before writing output.

## PDF files

The PDF path scans image XObjects in a restricted PDF structure. An image stream is eligible only when it has a direct `/Length` and no existing `/Filter`.

For eligible streams, compression adds `/RunLengthDecode` and a private marker, updates the stream length, and rebuilds the classic cross-reference table. The rewritten document is used only when the complete PDF becomes smaller. Otherwise, a valid unsupported or non-shrinking PDF is copied unchanged.

Restoration removes only the run-length filter and marker added by this program. Existing image filters are left alone.

## Read once, write once

`readInputFile` loads the full file into memory and identifies PDF input from its extension. The compression module prepares the result in memory, then calls `writeOutputFile` once after processing succeeds.
