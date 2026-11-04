# Firmware configuration

`sdkconfig.defaults` selects ESP32-S3, binary native USB, authenticated NimBLE and LittleFS. `partitions.csv` targets the verified 4 MiB Flash: two 1024 KiB application slots, 192 KiB protected factory packages and 1728 KiB writable user storage. The separate 2 MiB PSRAM does not change Flash capacity.

The new map moves `ota_1`, `factory` and `storage`. Before applying it to an existing device, back up and migrate user storage and provision the new factory image. Do not apply it as an application-only OTA update.

Hardware sources: `components/flint/board/models/flint_spark/s3_supermini/`. Device selection generates the board headers before component configuration. There is no C3 profile.
