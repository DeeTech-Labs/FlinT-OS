[Русский оригинал](../../ru/development/repository-layout.md)

# Repository layout

```text
config/       maintained settings, board parameters, version, partition table
cmake/        build modules and generated-header templates
src/          ESP-IDF component: OS implementations and hardware integration
main/         ESP-IDF entry point
platform/     pinned IgniteSDK submodule
resources/    OS branding and documentation source assets
tools/        developer commands and hardware checks
tests/host/   OS host tests and IDF stubs
tests/tooling/ Python tooling regression tests
docs/ru/      canonical documentation by topic
docs/en/      matching English translations
build/        ignored local output, generated headers, dependency lock, factory images
```

`src/` is registered through `EXTRA_COMPONENT_DIRS`; its ESP-IDF component name is `src`, and `main` uses `REQUIRES src`. Target registration stays in local `CMakeLists.txt`; shared build logic lives in `cmake/`. Component manifests and `Kconfig.projbuild` stay beside their components. Tool-specific root and `.github/` configuration files keep their required locations.

`config/devices.json` stores only the default ID; `config/devices/<model>/device.toml` is the single device configuration. Profiles are discovered automatically; panel parameters live in the same `device.toml`. Display and touch implementations live in `src/modules/`. GPIO has one board-configuration source. An implementation draft is not automatically a selectable profile.

`config/version.cmake` is the only version source. CMake generates `generated/config/flint_version.h`; the selector generates `generated/board/`. SDK factory packages are built in the selected build directory's `factory/`. Shared SDK resources remain in the submodule.

## Migration

`components/flint/` moved to `src/`; `core/system.cpp` became `core/application.cpp`. `brand/` and `.github/assets/` became `resources/`. `tools/tests/` moved to `tests/tooling/`; the Bash launcher is `tools/flint_tools.sh`.

Reconfigure after component paths change. Preserve previous builds with a new directory: `idf.py -B build/layout -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build`. Avoid unnecessary `set-target`, which resets configuration state.

Local settings default to `config/sdkconfig` and survive `idf.py fullclean`. The first configure copies the legacy root `sdkconfig` if the new file is absent; the original is preserved. Explicit `-DSDKCONFIG=<path>` or selector `--sdkconfig <path>` selects independent settings. A new explicitly selected file starts from defaults without importing the root file. `--build-dir` changes only artifact placement. `set-target` intentionally resets configuration and preserves the old file as `.old`; avoid it for routine S3 validation. The dependency lock is `<build-dir>/dependencies.lock`.

Root documents retain their full text. Topic/resource URLs changed, so external links need updating. Package formats, protocol and partition layout are unchanged. Local build and IDE files are not removed automatically.

## SDK generation

The SDK's shared `flint_generate_sdk` function generates OS registry headers and factory packages under `<build>/generated/sdk/include/` and `<build>/generated/sdk/factory/`. Firmware builds skip examples. `factory.bin` remains at the build root. Generation and tests do not update source `platform/include/`, `platform/factory/` or `platform/examples/`; unchanged outputs keep their timestamps.

SDK is pinned to 6e7a853 / 0x20261114. Use the pinned revision from the remote rather than selecting latest. [Integration](sdk-integration.md).
