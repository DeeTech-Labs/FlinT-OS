[Русский оригинал](../../ru/development/tools.md)

# Local tools

- `select_device.py`: select the maintained S3 profile and optionally build, flash or monitor when explicitly requested.
- `../platform/tools/compile_ignite.py`: compile a JSON project and assets into a binary Ignite.
- `../platform/tools/build_system.py`: compile factory Ignite sources.
- `../platform/tools/generate_registry.py`: generate API and permission registries.

The previous textual time-sync client, asset-pack generator and old renderer exporter were removed with their replaced contracts.

The `platform/` SDK is a pinned Git submodule; initialize it with `git submodule update --init --recursive`. There is no source export step.

[Configuration layout and migration](repository-layout.md). Version: `config/version.cmake`; default ID: `config/devices.json`; generated files: `<build-dir>/generated/`.

Bash launcher: `bash tools/flint_tools.sh`. Link validation: `python3 tools/check_docs.py`. Selector `--build-dir <path>` selects artifact placement; `--sdkconfig <path>` selects persistent local settings (default `config/sdkconfig`). With no action, the selector only generates headers; CMake generates them during builds; monitor does not generate files.
