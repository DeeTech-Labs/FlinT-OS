[Русский оригинал](../../ru/development/sdk-integration.md)

# SDK and touch/UI integration

As of 12 November 2026, `Flint-IgniteSDK/` and `FlinT-OS/platform/` contain matching uncommitted touch/UI changes (`ContractRevision=0x20261112`). OS builds with its local `platform/` without an external override. The submodule pointer remains `594eae4631f4fc5242a9c828bb7e3523e9758149`; that commit alone lacks the new contracts.

```sh
idf.py -B build/sdk-integration -DSDKCONFIG=config/sdkconfig -DIDF_TARGET=esp32s3 build
```

SDK owns generation: `cmake/FlintGenerate.cmake` creates registries and packages under `<build>/generated/sdk/`. OS searches generated headers before source headers and builds `factory.bin` from the same SDK's system packages. Firmware skips examples and does not rewrite SDK sources. See the [SDK guide](../../../platform/docs/en/SDK.md#generation-without-source-tree-writes).

Another machine needs the complete SDK working-tree changes. A future authorized SDK commit and publication must be followed by a deliberate OS pin update; otherwise a clean clone gets the older SDK. No commits or pin changes were made here.

Package formats, protocol and partition layout did not change as part of generation isolation. New touch/UI contracts were already present in the original uncommitted work. Application-only OTA does not update factory packages: firmware and `factory.bin` must come from the same SDK tree.
