---
title: Quick start
description: Build and launch the C demo or start the documentation site.
---

## Before you start

<table>
  <thead><tr><th>Workflow</th><th>Requirements</th></tr></thead>
  <tbody>
    <tr><td>C demo</td><td>A C11 compiler available as <code>cc</code>, and <code>make</code>.</td></tr>
    <tr><td>Documentation site</td><td>Node.js 24.12 or later, and pnpm 10.30.3.</td></tr>
  </tbody>
</table>

## Build and run the demo

From the repository root, build and launch the interactive menu:

<Code language="bash">{`make
./compression-demo`}</Code>

Choose **1** to compress a file, **2** to decompress one, or **3** to exit. Enter separate input and output paths; identical path strings are rejected.

:::tip[Protect the source]
The program reads the whole input into memory. Give it a different output path and make sure the destination directory already exists.
:::

## Start the documentation site

Install the pinned pnpm dependencies, then run the local development server:

<Code language="bash">{`pnpm install
pnpm docs:dev`}</Code>

The development server watches the Markdown under `docs/`. For a static build or preview, use:

<table>
  <thead><tr><th>Command</th><th>Result</th></tr></thead>
  <tbody>
    <tr><td><code>pnpm docs:build</code></td><td>Writes the site to <code>dist/docs/</code>.</td></tr>
    <tr><td><code>pnpm docs:preview</code></td><td>Builds the site, then serves the static output.</td></tr>
    <tr><td><code>pnpm check:markdown</code></td><td>Checks Markdown formatting without changing files.</td></tr>
    <tr><td><code>pnpm format:markdown</code></td><td>Formats the README and docs Markdown in place.</td></tr>
  </tbody>
</table>

## Run the configured C quality profile

The configured profile builds the demo with `make`. The complete C checks also need `clang-format`, `clang`, `clang-tidy`, and Doxygen on `PATH`.

<Code language="bash">{`pnpm exec terra-quality quality --all --warn
make clean`}</Code>

`make clean` removes the demo executable and analyzer output.
