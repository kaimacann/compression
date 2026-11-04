# Compression Project Docs

Assessment materials and a project proposal for image compression and encryption workflows. Canvas remains authoritative for course requirements, dates, and marking decisions.

## Getting started

The Docs site requires Node.js and pnpm 10.30.3. The C demo requires a C11 compiler and Make. From the repository root:

```sh
pnpm install
pnpm docs
make run
```

`pnpm docs` starts the Docs site. `make run` builds and starts `compression-demo`.

## Commands

Run `pnpm build` to build the Docs site under `dist/docs`. Use `make` to build the C executable and `make clean` to remove it. The repository does not define an automated test script.

## Security and limitations

The encryption demo is coursework, not a security recommendation. Review the assessment brief for the prototype's scope and limitations.

## Structure

`docs/at3/` contains the assessment brief and submission guides. `docs/plan/` contains the compression proposal. `samples/` contains image, PDF, and text inputs; `src/c/` contains the native demo.

## References

- [Assessment brief](docs/at3/brief.md)
- [Compression plan](docs/plan/0-overview.md)
- [Build and run targets](Makefile)
- [MIT license](LICENSE)

## Contributing

Keep course requirements distinct from authored proposal material. Use focused documentation edits.

## Credits and license

Repository files are licensed under [MIT](LICENSE), subject to third-party rights in sample assets.
