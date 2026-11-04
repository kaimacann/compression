# Compression Project Docs

Assessment material, project specifications, and a practical guide to the C compression repository. Canvas remains authoritative for assessment requirements, dates, and marking decisions.

## Get started

The Docs site requires Node.js and pnpm 10.30.3. The C program requires a C11 compiler and Make.

```sh
pnpm install
pnpm docs
make run
```

`pnpm docs` starts the shared Docs site on its assigned local port. `make run` builds and runs the C program.

## Build and clean

Run `pnpm build` to build the Docs site under `dist/docs`. Run `make` to build the C executable and `make clean` to remove build output.

## Documentation

- [Assessment](docs/assessment/): assignment information and separately labelled business and technical specifications.
- [Compression guide](docs/compression/): installation, command-line use, file handling, and reference material.

The C program is a coursework project. Do not treat its encryption features as a security recommendation.

## License

Repository files are licensed under [MIT](LICENSE), subject to third-party rights in sample assets.
