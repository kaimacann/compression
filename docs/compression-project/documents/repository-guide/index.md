---
title: Repository guide
description: Build, use, and understand the current C compression demo.
---

The repository combines an interactive C11 demo with a documentation site. This guide follows the code as it works today; project proposals describe separate, planned systems.

:::note[Current scope]
Non-PDF files are handled as bytes in a reversible `CRL1` container. PDFs use a conservative image-stream rewrite. Image pixels are not decoded, and the demo does not perform lossy compression, encryption, or stroke-to-SVG conversion.
:::

## Follow the data

The menu calls one file operation. Each operation reads the input once, selects a path from its extension, then writes one output.

<Nodes
  nodes={`[
    {"id":"menu","type":"origin","data":{"title":"main() menu"}},
    {"id":"operation","type":"process","data":{"title":"compressFile / decompressFile"}},
    {"id":"read","type":"data","data":{"title":"readInputFile · whole file"}},
    {"id":"route","type":"decision","data":{"title":"Extension is .pdf?"}},
    {"id":"pdf","type":"process","data":{"title":"PDF stream transform / restore"}},
    {"id":"container","type":"process","data":{"title":"CRL1 encode / decode"}},
    {"id":"write","type":"process","data":{"title":"writeOutputFile · once"}},
    {"id":"done","type":"terminal","data":{"title":"Output file"}}
  ]`}
  edges={`[
    {"id":"e1","source":"menu","target":"operation"},
    {"id":"e2","source":"operation","target":"read"},
    {"id":"e3","source":"read","target":"route"},
    {"id":"e4","source":"route","target":"pdf","label":"PDF"},
    {"id":"e5","source":"route","target":"container","label":"other"},
    {"id":"e6","source":"pdf","target":"write"},
    {"id":"e7","source":"container","target":"write"},
    {"id":"e8","source":"write","target":"done"}
  ]`}
  direction="TB"
  height={560}
/>

## Choose a page

<table>
  <thead>
    <tr><th>Page</th><th>Use it to</th></tr>
  </thead>
  <tbody>
    <tr><td><a href="./quick-start/">Quick start</a></td><td>Build the demo, open its menu, and run the docs site.</td></tr>
    <tr><td><a href="./workflows/">File workflows</a></td><td>Copy round-trip commands for the text and PDF samples.</td></tr>
    <tr><td><a href="./internals/">How it works</a></td><td>See how CRL1 containers and eligible PDF streams are processed.</td></tr>
    <tr><td><a href="./reference/">API and limits</a></td><td>Check the public C calls, supported inputs, and troubleshooting tips.</td></tr>
  </tbody>
</table>
