[Русский оригинал](../ru/TOOLS.md)

# Local tools

- `select_device.py`: select the maintained S3 profile and optionally build, flash or monitor when explicitly requested.
- `../platform/tools/compile_ignite.py`: compile a JSON project and assets into a binary Ignite.
- `../platform/tools/build_system.py`: compile factory Ignite sources.
- `../platform/tools/generate_registry.py`: generate API and permission registries.

The previous textual time-sync client, asset-pack generator and old renderer exporter were removed with their replaced contracts.

The `platform/` SDK is a pinned Git submodule; initialize it with `git submodule update --init --recursive`. There is no source export step.
