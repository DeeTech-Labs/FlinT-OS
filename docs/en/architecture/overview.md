[Русский оригинал](../../ru/architecture/overview.md)

# FlinT OS architecture

`main/app_main.cpp` initializes NVS and starts the application loop. `src/core/application.cpp` owns Core and integrates hardware adapters with the portable SDK. USB, BLE and input dispatch work to the Core owner. OS owns hardware implementations; pinned `platform/` owns shared contracts.

- [Display and recovery](display.md).
- [BLE lifecycle](ble-lifecycle.md).
- [Input and system UI](input-and-system-ui.md).
- [Companion](companion-protocol.md).
- [Repository layout](../development/repository-layout.md).
- [Diagnostics](../validation/performance.md).
- [IgniteSDK](../../../platform/docs/en/ARCHITECTURE.md).

Settings belong in `config/`, hardware module implementations in `src/modules/`. Selected headers, version and factory packages are generated into the build directory: `generated/` and `factory/`. OS generation does not place these artifacts in source directories.

Pending UI integration: [SDK integration](../development/sdk-integration.md).
