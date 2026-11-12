[Русский оригинал](../../ru/development/configuration.md)

# Firmware configuration

`sdkconfig.defaults` selects ESP32-S3, binary native USB, authenticated NimBLE and LittleFS. `partitions.csv` targets the verified 4 MiB Flash: two 1024 KiB application slots, 192 KiB protected factory packages and 1728 KiB writable user storage. The separate 2 MiB PSRAM does not change Flash capacity.

The new map moves `ota_1`, `factory` and `storage`. Before applying it to an existing device, back up and migrate user storage and provision the new factory image. Do not apply it as an application-only OTA update.

Hardware sources: `src/board/models/flint_spark/s3_supermini/`. Device selection generates the board headers before component configuration. There is no C3 profile.

## BLE policy

`ble_enabled` is persisted separately from Companion and defaults to enabled. Restore access over USB after disabling it. `CONFIG_FLINT_BLE_START_MEMORY_REQUIREMENT` reserves 64 KiB internal beyond the common reserve for startup. See [ARCHITECTURE.md](../architecture/ble-lifecycle.md) for lifecycle, worker, diagnostics and failures.

[Configuration layout and migration](repository-layout.md). Version: `config/version.cmake`; board registry: `config/devices.json`; generated files: `<build-dir>/generated/`.
