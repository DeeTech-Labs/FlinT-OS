[Русский оригинал](../ru/ARCHITECTURE.md)

# Architecture

The maintained architecture specification lives in [FlinT Platform](../../platform/README.md). The `platform/` directory is a pinned Git submodule of Flint IgniteSDK, shared with Flare. Initialize it with `git submodule update --init --recursive`. Shared implementation changes belong in that repository.

Hardware: `components/flint/board/`, `display/`, `ble/` and `core/system.cpp`.
Portable services: `platform/src/` and `platform/include/flint/`.
Factory applications: `platform/system/` compiled into `platform/factory/`.
Development contracts: `platform/spec/` and generated registries.

The display uses a native ST7789 driver (`display/st7789.cpp`) over ESP-IDF SPI Master. Power/gamma commands, orientation and offsets belong to the board profile. The driver is owned by the main application task; synchronous SPI/DMA completion allows immediate reuse of the scanline buffer. Initialization and transfer errors propagate to the caller, and partially initialized resources are released.

The IgniteSDK renderer and dirty-region calculation are preserved. The RGB565 framebuffer needs 134,400 bytes, preferring PSRAM with an internal-RAM fallback. The previous scene and the 560-byte DMA scanline share 1352 bytes of static internal storage. The scene is read for dirty-region calculation; its lifetime then ends and the scanline becomes active. The scene is reconstructed only after all transfers succeed. This saves a separate 560-byte buffer without new allocations, additional SPI transfers or changing ISR placement in IRAM. Initial fill and recovery use the same storage independently of the framebuffer. `DISPON` follows the initial background fill. Recovery retains its messages, background, red text and line positions, using IgniteSDK's built-in `Body` font instead of the removed library's font. It needs no full framebuffer, filesystem or Core instance.

CMake enables `CONFIG_COMPILER_OPTIMIZATION_CHECKS_SILENT` only for `esp_driver_spi`. Argument checks and error returns remain enabled; detailed `ESP_RETURN_ON_*` and `ESP_GOTO_ON_*` messages inside this component are omitted. Assertions, ISR placement in IRAM, other SPI messages and diagnostics in other components are unchanged. FlinT still handles and logs returned errors. This reduces IRAM validation code at the cost of less detailed low-level SPI diagnostics.

Host driver checks (SPI/GPIO simulation, no device access):

```sh
cmake -S tests/host -B /tmp/flint-display -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/flint-display
ctest --test-dir /tmp/flint-display --output-on-failure
```

The tests compare pixels against the portable renderer and cover addressing, panel settings, dirty regions, transfer failures, resource cleanup and recovery after allocation failure. They do not verify SPI electrical behavior, physical orientation, tearing or device performance. Lua, LittleFS and package formats are unchanged.


Demand-driven Companion uses the [shared SDK contract](../../platform/docs/en/COMPANION.md). [Diagnostics and baseline](PERFORMANCE.md).

## BLE availability

Persistent `ble_enabled` controls the radio independently of Companion consent. An absent value migrates to `1`; `0` disables advertising and connections. The board has no buttons/touch: re-enable through USB. Bond keys in NVS are not erased. Settings ACK confirms acceptance of persistent policy; `ble_state`, `ble_transition_active`, and `ble_lifecycle_error` describe the hardware transition.

The Core owner manages intent and session termination. Start/stop uses a temporary `ble_policy` task with a 4096-byte stack because vendor `nimble_port_stop` waits for the host task without a bounded timeout. This task cannot access Core/PackageManager. After the host leaves `nimble_port_run`, the worker deletes it through `nimble_port_freertos_deinit`, then deinitializes host/controller and frees the RX queue. No extra task remains after transition and no idle polling is added. Transition failures are diagnostic and are not retried indefinitely.

Each new start checks current internal free/largest against reserves; `CONFIG_FLINT_BLE_START_MEMORY_REQUIREMENT` defaults to 64 KiB beyond the common reserve. Repeated intents coalesce. Disabling invalidates the queue, callbacks and session generation. The BLE settings reply may finish before stop, with a two-second grace bound. Host/worker watermark retains the minimum across start/stop cycles.

A local Spark check passed 10 start/stop cycles: disabled releases about 69 KiB internal, PSRAM remains unchanged and lifecycle errors=0. This validates memory/link behavior, not current consumption or retention of an existing bond through repeated pairing.
