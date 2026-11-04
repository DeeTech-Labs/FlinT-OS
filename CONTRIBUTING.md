# Contributing to FlinT OS

Thanks for helping improve **FlinT OS** (firmware for **FlinT** devices; base board **FlinT Spark**), by **DeeTech Labs**.

## Before you start

- [README](README.md), [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), shared SDK: [`platform/README.md`](platform/README.md)
- Release notes: [`CHANGELOG.md`](CHANGELOG.md)
- Questions → [Discussions](https://github.com/DeeTech-Labs/FlinT-OS/discussions); bugs/features → [Issues](https://github.com/DeeTech-Labs/FlinT-OS/issues)
- Support routing → [SUPPORT.md](SUPPORT.md)

## Development setup

Use ESP-IDF 6.0.2, C++20 and Python 3. Install Pillow for compiler font/image processing and integration fixtures. Never commit real credentials.

```bash
python3 tools/select_device.py --build
```

Use four spaces and no tabs. `.editorconfig` and `.clang-format` define the source style.

## Code generation

Factory applications are JSON projects under `platform/system/`. Regenerate their binaries and the machine-readable API/permission registries before exporting the shared implementation:

```bash
python3 platform/tools/generate_registry.py
python3 platform/tools/build_system.py
python3 tools/sync_platform.py
```

Edit the canonical OS implementation rather than the exported Flare copy. Hardware sources remain under `components/flint/board/models/`.

## Host tests

```bash
cmake -S platform -B /tmp/flint-platform -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/flint-platform
ctest --test-dir /tmp/flint-platform --output-on-failure
python3 -m unittest discover -s tools/tests -v
```

Default verification is device-independent. Hardware tests are separate and opt-in.

## Branching & PRs

- Do not push commits directly to `main`.
- Branch from up-to-date `main` (`feature/…`, `fix/…`, `chore/…`, `docs/…`).
- Open a Pull Request; prefer small, reviewable diffs.

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/), English, imperative:

```text
feat(settings): Add date format options
fix(ui): Prevent mode label overlapping wifi badge
docs: Clarify Face SDK include rules
```

## Ignite applications and watch faces

Create a standalone project with the shared compiler instead of registering native faces inside Core. See [`docs/IGNITS.md`](docs/IGNITS.md) and `platform/examples/pc-monitor/`. Keep application access declared and scoped; unsupported APIs must not be advertised.

## Boards & partitions

- Hardware: [`components/flint/board/README.md`](components/flint/board/README.md)
- Flash map: `config/partitions.csv` (dual application slots + `factory` + `storage`)

## Naming

**FlinT OS** (firmware), **FlinT Spark** (base device), **FlinT** (product line), **DeeTech Labs** (org).

## License

Apache License 2.0 — see [LICENSE](LICENSE) and [NOTICE](NOTICE).

## Code of Conduct

Participation is governed by the [Contributor Covenant](CODE_OF_CONDUCT.md).
