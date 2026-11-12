[Русский оригинал](../../ru/development/sdk-integration.md)

# SDK and touch/UI integration

As of 12 November 2026, OS pins published SDK `9d768efc36df3c974c57c1d02211af1d1875bea7` (`ContractRevision=0x20261112`, [SDK PR #6](https://github.com/DeeTech-Labs/Flint-IgniteSDK/pull/6)). OS builds with `platform/` without an external override or uncommitted SDK copies.

```sh
idf.py -B build/sdk-integration -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build
```

SDK owns generation: `cmake/FlintGenerate.cmake` creates registries and packages under `<build>/generated/sdk/`. OS searches generated headers before source headers and builds `factory.bin` from the same SDK's system packages. Firmware skips examples and does not rewrite SDK sources. See the [SDK guide](../../../platform/docs/en/SDK.md#generation-without-source-tree-writes).

On another machine, initialize the pinned SDK with `git submodule update --init --recursive`. The matching Flare uses the same revision. See the [2026-11-12 device report](../validation/2026-11-12-icon-ui-resources.md).

The contract uses FLM2 and FLIM v2; older packages must be rebuilt. Binary protocol framing and the partition layout are unchanged. Application-only OTA does not update factory packages: firmware and `factory.bin` must come from the same SDK tree.
