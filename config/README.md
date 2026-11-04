# Firmware configuration

`sdkconfig.defaults` selects ESP32-S3, binary native USB, authenticated NimBLE and LittleFS. `partitions.csv` is a provisional 4 MiB layout with two application slots, protected factory packages and writable user storage.

Hardware sources: `components/flint/board/models/flint_spark/s3_supermini/`. Device selection generates the board headers before component configuration. There is no C3 profile.
