# ESP-IDF build

Target: ESP32-S3 only. The locally verified SDK is ESP-IDF 6.0.2. Python tooling discovers the existing SDK and its managed environment.

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
