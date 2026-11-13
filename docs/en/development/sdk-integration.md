[Русский оригинал](../../ru/development/sdk-integration.md)

# SDK and touch/UI integration

OS uses SDK `ContractRevision=0x20261114`, pinned to local commit `18d22f4` in `platform/`. Normal builds use the submodule; FLINT_PLATFORM_DIR is available for deliberate SDK development. See [sensor service](../hardware/sensor-service.md).

```sh
idf.py -B build/sdk-integration -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build
```

SDK owns generation: `cmake/FlintGenerate.cmake` creates registries and packages under `<build>/generated/sdk/`. OS searches generated headers before source headers and builds `factory.bin` from the same SDK's system packages. Firmware skips examples and does not rewrite SDK sources. See the [SDK guide](../../../platform/docs/en/SDK.md#generation-without-source-tree-writes).

On another machine, initialize the pinned SDK with `git submodule update --init --recursive`. Flare pins the same SDK commit. See the [2026-11-12 device report](../validation/2026-11-12-icon-ui-resources.md).

The contract uses FLM3 and FLIM v2; older packages must be rebuilt. Binary protocol framing and the partition layout are unchanged. Application-only OTA does not update factory packages: firmware and `factory.bin` must come from the same SDK tree.
