[Русский оригинал](../../../CONTRIBUTING.md)

# Contributing to FlinT OS

Thanks for helping improve **FlinT OS** (firmware for **FlinT** devices; base board **FlinT Spark**), by **DeeTech Labs**.

## Before you start

- [README](../../../README.md), [Architecture](../architecture/overview.md), shared SDK: [`platform/README.md`](../../../platform/README.md)
- Release notes: [`CHANGELOG.md`](../../../CHANGELOG.md)
- Questions → [Discussions](https://github.com/DeeTech-Labs/FlinT-OS/discussions); bugs/features → [Issues](https://github.com/DeeTech-Labs/FlinT-OS/issues)
- Support routing → [SUPPORT.md](../project/support.md)

## Development setup

Use ESP-IDF 6.1.0, C++20 and Python 3.11+. Install Pillow for compiler font/image processing and integration fixtures. Never commit real credentials.

```bash
python3 tools/select_device.py --build
```

Use four spaces and no tabs. `.editorconfig` and `.clang-format` define the source style.

## Code generation

Factory applications are JSON projects under `platform/system/`. Regenerate their binaries and the machine-readable API/permission registries before exporting the shared implementation:

```bash
python3 platform/tools/generate_registry.py
python3 platform/tools/build_system.py
```

Edit the pinned IgniteSDK repository and update the consumer submodules. Hardware sources remain under `src/modules/`.

## Host tests

```bash
cmake -S platform -B platform/build/docs-sdk -DCMAKE_BUILD_TYPE=Debug
cmake --build platform/build/docs-sdk --config Debug
ctest --test-dir platform/build/docs-sdk -C Debug --output-on-failure
python3 -m unittest discover -s tests/tooling -v
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

Create a standalone project with the shared compiler instead of registering native faces inside Core. See [Ignite packages](ignite-packages.md) and `platform/examples/pc-monitor/`. Keep application access declared and scoped; unsupported APIs must not be advertised.

## Boards & partitions

- Hardware: [Spark board guide](../hardware/spark-s3-supermini.md)
- Flash map: `config/partitions.csv` (dual application slots + `factory` + `storage`)

## Naming

**FlinT OS** (firmware), **FlinT Spark** (base device), **FlinT** (product line), **DeeTech Labs** (org).

## License

See [licensing scope](../legal/licensing.md), [LICENSE](../../../LICENSE) and [NOTICE](../../../NOTICE).

## Code of Conduct

Participation is governed by the [Contributor Covenant](../project/code-of-conduct.md).

## Documentation and contribution terms

Russian is canonical; update Russian documentation and its English translation together. Authors retain copyright. Contributions use the applicable component license; they do not imply assignment or unrestricted commercial relicensing. See [licensing scope](../legal/licensing.md).

## Small-change validation

Start with the [extension map](extending.md), not all docs. Text needs diff/link checks;
config/generator needs `python -m unittest discover -s tests/tooling -v`, selected/unselected
source/dependency tests. Drivers/worker need `cmake -S tests/host -B build/host`, build/CTest
with --config/-C Debug, then ESP32-S3 build and separate physical acceptance. Shared Core
changes belong in owning SDK with Lua/no-Lua and both consumers. Firmware/storage/recovery
need size/error checks and separately authorized physical power-cut tests with protected data.
Examples need actual commands/config/RU/EN checks. Pillow is required before configure.
Artifacts only in owning build/temp; independent submodule uses platform/build. Never mix kits.
Hardware smoke changes test packages; flashing/smoke on someone else's board needs agreement.
Issues/PRs include revision/TOML/firmware+SDK SHA, command/symptom and no private keys/data.
Explicitly report unperformed heap/stack/power/Flash/USB/BLE/sensor checks.
