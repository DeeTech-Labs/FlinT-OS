# ESP-IDF build

Target: ESP32-S3 only. The locally verified SDK is ESP-IDF 6.0.2. Python tooling discovers the existing SDK and its managed environment.

```sh
python3 tools/select_device.py --list
python3 tools/select_device.py --build
```

With an activated SDK, `idf.py set-target esp32s3` followed by `idf.py build` also works. Use a separate build directory/configuration to preserve unrelated local builds.

The firmware image includes a read-only `factory` LittleFS image compiled from system Ignite sources. `storage` is mounted separately for user packages, settings and grants. Non-erased corrupt storage is not automatically formatted. USB is reserved for binary Companion frames; logs do not interleave its data stream.

The 4 MiB partition map reserves two 1280 KiB application slots, 192 KiB factory storage, 1216 KiB writable storage, NVS and a coredump partition. It is a build baseline, not a measurement of the connected board. Physical capacity and PSRAM configuration must be established before flashing.

No `flash`, serial monitor, BLE scan or physical connection is part of local validation.
