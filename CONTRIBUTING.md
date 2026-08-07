# Contributing to FlinT OS

Thanks for helping improve **FlinT OS** (firmware for **FlinT** devices; base board **FlinT Spark**), by **DeeTech Labs**.

## Before you start

- [README](README.md), [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), Face SDK: `components/flint/sdk/flint_face_sdk.h`
- Questions → [Discussions](https://github.com/DeeTech-Labs/FlinT-OS/discussions); bugs/features → [Issues](https://github.com/DeeTech-Labs/FlinT-OS/issues)
- Support routing → [SUPPORT.md](SUPPORT.md)

## Development setup

1. ESP-IDF v5.x / v6.x activated.
2. Secrets (gitignored):

   ```bash
   cp components/flint/config/secrets.example.h components/flint/config/secrets.h
   ```

3. Build for FlinT Spark:

   ```bash
   idf.py set-target esp32c3
   idf.py build
   idf.py -p /dev/ttyACM0 flash monitor
   ```

Never commit `secrets.h` or credentials.

## Branching & PRs

- Do not push commits directly to `main`.
- Branch from up-to-date `main` (`feature/…`, `fix/…`, `chore/…`, `docs/…`).
- Open a Pull Request; fill the PR template.
- Prefer small, reviewable PRs.

## Commit messages

[Conventional Commits](https://www.conventionalcommits.org/), English, imperative:

```text
feat(settings): Add date format options
fix(ui): Prevent mode label overlapping wifi badge
docs: Clarify Face SDK include rules
```

## Faces

- Prefer new files under `components/flint/faces/` over editing core services.
- Include only `sdk/flint_face_sdk.h` (plus allowed display/glance helpers); keep `onTick` non-blocking.
- Register in `components/flint/faces/faces_builtin.cpp`.
- Start from `community/face_template.cpp.example`.
- Details: [`docs/FACES.md`](docs/FACES.md).

## Boards & partitions

- Hardware: [`components/flint/board/README.md`](components/flint/board/README.md)
- Flash map: `config/partitions.csv` (dual OTA + `storage` + `cfg`)

## Naming

**FlinT OS** (firmware), **FlinT Spark** (base device), **FlinT** (product line), **DeeTech Labs** (org).

## License

Apache License 2.0 — see [LICENSE](LICENSE) and [NOTICE](NOTICE).

## Code of Conduct

Participation is governed by the [Contributor Covenant](CODE_OF_CONDUCT.md).
