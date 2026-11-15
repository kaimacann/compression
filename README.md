# Compression

Compression lets students explore how different files can be made smaller and restored.

## Get started

Install Node.js, pnpm 10.30.3, a C11 compiler, and Make. From the repository root:

```sh
pnpm install --frozen-lockfile
make run
```

`make run` builds `compression-demo` and opens its sign-in and operation menu. Follow the prompts for a local account. Choose the exit option or press `Ctrl+C` to stop.

## Usage

The byte archive format is lossless `TCMP`; the restore operation decrypts and decompresses its archive. PNG, JPEG, and PDF media modes can emit simplified SVG or smaller PDF output. Vector mode is lossy and best suited to simple sketches, so keep the original when fidelity matters. See the [formats and Makefile guide](docs/BUILD_AND_FORMATS.md) for output paths and options.

## Validation

| Command                | Checks                        |
| ---------------------- | ----------------------------- |
| `pnpm run quality`     | Changed and untracked files   |
| `pnpm run quality:all` | Full configured Quality scope |
| `pnpm run test`        | Project test suite            |
| `pnpm run build`       | Build output                  |

Quality can apply safe fixes; review the diff before committing. No separate browser or Playwright test command is defined here.

## Contributing

`quality:all` checks the complete configured scope, including C documentation parameters. `build` generates the Docs site. The [Makefile guide](docs/BUILD_AND_FORMATS.md) documents native build, format, and Doxygen targets.

Open a pull request against `development` and review the results of [`repository-check.yml`](.github/workflows/repository-check.yml) before merging. Keep unrelated changes out of the pull request.

## Documentation

- [Assessment material](docs/assessment/)
- [Compression guide](docs/compression/)
- [Build and formats reference](docs/BUILD_AND_FORMATS.md)

## License

MIT, subject to third-party rights in sample assets. See [LICENSE](LICENSE).
