# Build and partition defaults

- `sdkconfig.defaults`: ESP-IDF defaults for the fixed product revision.
- `partitions.csv`: ESP32-C3 4 MB layout with two 1280 KB firmware slots,
  128 KB system `assets`, 1216 KB writable package `storage`, and 64 KB `cfg`.
- `assets` is flashed with the firmware. `storage` and `cfg` are not flashed
  during ordinary updates; an erased filesystem is initialized at first boot.

Hardware sources: `components/flint/board/models/flint_spark/c3_dev/`.
Package storage and first-time provisioning: [Ignits](../docs/IGNITS.md).
