[Русский оригинал](../ru/ESP-IDF.md)

# ESP-IDF build

Target: ESP32-S3 only. The required SDK is ESP-IDF 6.1.0. Python tooling discovers the existing SDK and its managed environment.

```sh
python3 tools/select_device.py --list
python3 tools/select_device.py --build
```

With an activated SDK, `idf.py set-target esp32s3` followed by `idf.py build` also works. Use a separate build directory/configuration to preserve unrelated local builds.

The main task uses a 32 KiB internal stack. The shared Lua configuration limits
nested C calls to 40. The stack covers parser, callback, package-validation and filesystem call chains;
measure its headroom when changing these paths. PSRAM does not
increase a task's stack. Spark defaults enable `CONFIG_FLINT_LUA_RUNTIME`; the
source list and limits come directly from the pinned SDK.

The firmware image includes a read-only `factory` LittleFS image compiled from system Ignite sources. `storage` is mounted separately for user packages, settings and grants. Non-erased corrupt storage is not automatically formatted. USB is reserved for binary Companion frames; logs do not interleave its data stream.

The 4 MiB partition map reserves two 1024 KiB application slots, 192 KiB factory storage, 1728 KiB writable storage, NVS and a coredump partition. The connected development unit has verified 4 MiB Flash and 2 MiB Quad PSRAM; PSRAM is separate volatile memory. ESP-IDF prints image size and headroom and rejects firmware that exceeds the 1 MiB application slot.

This map changes the previous `ota_1`, `factory` and `storage` offsets. Back up and migrate existing user storage before flashing; write the new partition table and factory image as well as the firmware. Application-only OTA does not update the partition map.

Host tests never flash or connect to a device. Explicit hardware validation (ESP-IDF Python environment or pyserial required) is:

```sh
python tools/test_lua_device.py --port /dev/ttyACM0 --report /tmp/spark-lua.json
```

This command installs/removes only `test.lua-*` fixtures and launches factory clock;
it does not flash firmware or erase Flash. Detailed reports are temporary diagnostics, not repository documents.

## Application API 1 integration

SDK is pinned to `594eae4631f4fc5242a9c828bb7e3523e9758149` (main after SDK PR #5). The commit is published to origin; `platform/` pins this exact revision with Application API 1, Demand v1 and resumable installation. Rebuild firmware; package/Companion formats and Flash partitions are unchanged. CONFIG_FLINT_LUA_RUNTIME enables CapAppApi (128); new projects use app_api: 1.

The main loop waits for USB/BLE notifications and the nearest Core/transport deadline; periodic 20 ms BLE maintenance is removed. RX/TX work is bounded per pass and package verification continues in Core::tick. Sustained work yields one FreeRTOS tick. See [PERFORMANCE.md](PERFORMANCE.md) for measurements and limitations.

corePostInput(InputEvent) and coreRuntimeGeneration() in core/bridge.h are application-task-only. Drivers capture generation when obtaining input; ISR/other-task producers must use their own bounded queue drained by the application task. Runtime enforces queue limits and input.focused. The selected Spark profile has no buttons, touch or battery sensor: physical input remains unavailable and power returns not_supported. No hardware signals or measurements are synthesized.

Private state uses shared Core storage /storage/packages/app-<id>.kv; SDK owns quotas, atomic writes, CRC, update/removal semantics and write throttling. Timers and extended diagnostics also stay in SDK. Lua heap/instruction limits were not raised; native peaks, stack, Flash durability and timing require new device checks.

CI uses the official release image `espressif/idf:v6.1`: the upstream release tag is `v6.1`, while CMake reports version 6.1.0. The Docker tag `v6.1.0` is not published. Local and CI builds must use the same release rather than the moving `release-v6.1`/`latest` tags.
