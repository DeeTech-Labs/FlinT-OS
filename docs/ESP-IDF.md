# ESP-IDF build

Target: ESP32-S3 only. The locally verified SDK is ESP-IDF 6.0.2. Python tooling discovers the existing SDK and its managed environment.

```sh
python3 tools/select_device.py --list
python3 tools/select_device.py --build
```

With an activated SDK, `idf.py set-target esp32s3` followed by `idf.py build` also works. Use a separate build directory/configuration to preserve unrelated local builds.

The main task uses a 16 KiB stack. USB application launch nests Core dispatch, package validation and scene decoding; the measured stack usage reached 8736 bytes during clock/settings switching, exceeding the former 8 KiB allocation. Keep the stack in internal RAM; available PSRAM does not increase a task’s configured stack.

The firmware image includes a read-only `factory` LittleFS image compiled from system Ignite sources. `storage` is mounted separately for user packages, settings and grants. Non-erased corrupt storage is not automatically formatted. USB is reserved for binary Companion frames; logs do not interleave its data stream.

The 4 MiB partition map reserves two 1024 KiB application slots, 192 KiB factory storage, 1728 KiB writable storage, NVS and a coredump partition. The connected development unit has verified 4 MiB Flash and 2 MiB Quad PSRAM; PSRAM is separate volatile memory. A 661376-byte firmware image leaves 387200 bytes of headroom in each application slot.

This map changes the previous `ota_1`, `factory` and `storage` offsets. Back up and migrate existing user storage before flashing; write the new partition table and factory image as well as the firmware. Application-only OTA does not update the partition map.

No `flash`, serial monitor, BLE scan or physical connection is part of local validation.
