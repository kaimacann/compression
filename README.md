# Compression Project Docs

Assessment material and a practical guide to the C compression repository. Canvas remains authoritative for assessment requirements, dates, and marking decisions.

## Start here

Requirements: Node.js, pnpm 10.30.3, a C11 compiler available as `cc`, and Make. Run these commands from the repository root:

```sh
pnpm install --frozen-lockfile
make run
```

The first command installs the locked Docs dependencies. `make run` builds `compression-demo`, then asks you to create or enter a local account before opening the menu. Command-line operations also require sign-in. Ordinary files are archived as lossless `TCMP` data and the resulting archive is encrypted; the restore option decrypts it for decompression and leaves the archive encrypted. PNG, JPEG, and PDF media processing offers unchanged or Terra sketch vector output; these media outputs do not use the `TCMP` encryption flow. Vector mode is intentionally aggressive: its two-colour default targets simple, low-detail, single-ink sketches, can discard important detail, and is not general-purpose image compression. Keep the original when fidelity matters. PDF mode rebuilds a smaller PDF when possible and exports SVG sidecars; image inputs produce SVG. Leave the output blank to use a sibling name: unchanged media keeps its extension, vectorized images use `.svg`, PDFs keep `.pdf`, and byte archives use `.tcmp`. `decompress` restores only `TCMP` archives; SVG vectorization is lossy and cannot be reversed. To stop the menu, choose its exit option; `Ctrl+C` also stops it.

## Makefile walkthrough

The Makefile builds one executable from `src/c/main`, `src/c/compression`, `src/c/encryption`, and `src/c/login`. `SOURCES` lists its C translation units; `HEADERS` lists headers whose changes must trigger a rebuild. `$(CC)` is the compiler, `$(CPPFLAGS)` supplies preprocessor options, `$(CFLAGS)` selects C11 and warning flags, and `$(LDLIBS)` links the math library. Override them on the command line when needed, for example `make CC=clang` or `make CFLAGS='-std=c11 -Wall -Wextra -pedantic -g -O0'`.

| Command                 | Purpose                                                                                    |
| ----------------------- | ------------------------------------------------------------------------------------------ |
| `make`                  | Build `compression-demo`; this is the default target.                                      |
| `make run`              | Build if needed, then run the interactive menu.                                            |
| `make clean`            | Remove the executable and root-level Clang analyzer `.plist` files.                        |
| `make check-doc-params` | List documented C functions that lack `@param` tags; exit unsuccessfully if any are found. |
| `make format-c`         | Format the executable's listed C sources and headers in place; requires `clang-format`.    |
| `make format-c-check`   | Check that same formatting without changing files.                                         |

`make run` is the normal command for demonstrating the program. Use `make` when you only need a fresh executable, and `make clean && make` to force a complete rebuild. The Makefile does not install the executable globally; run it from the repository with `./compression-demo` after building.

## Docs and quality commands

| Command                   | Purpose                                                                                           |
| ------------------------- | ------------------------------------------------------------------------------------------------- |
| `pnpm run docs`           | Start the local Docs development server on port 3010. Stop it with `Ctrl+C`.                      |
| `pnpm run build`          | Build the static Docs site under `dist/docs`.                                                     |
| `pnpm run quality`        | Run configured quality checks, including the C build and parameter-documentation check.           |
| `pnpm run quality:all`    | Run the complete configured quality suite.                                                        |
| `pnpm run format`         | Format the README and editable Docs Markdown in place; the verbatim assessment brief is excluded. |
| `pnpm run format:c`       | Run `make format-c`.                                                                              |
| `pnpm run format:c:check` | Run `make format-c-check`.                                                                        |
| `pnpm run docs:doxygen`   | Generate the C API reference under `dist/doxygen`; requires Doxygen.                              |

## Project material

- [Assessment](docs/assessment/): assignment information and separately labelled business and technical specifications.
- [Compression guide](docs/compression/): installation, command-line use, file handling, and reference material.

The C program is a coursework project. Its login stores plaintext credentials and its fixed-key XOR stream cipher is educational only; neither provides production security. Vector mode is lossy, and `TCMP` is not a standard archive format.

## License

Repository files are licensed under [MIT](LICENSE), subject to third-party rights in sample assets.
