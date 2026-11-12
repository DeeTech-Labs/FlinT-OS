[Русский оригинал](../../ru/hardware/spark-s3-supermini.md)

# FlinT Spark S3 Super Mini

Only supported profile: `flint_spark_s3_supermini`, target `esp32s3`.
The C3 profile was removed; selecting its ID is an error.

Development wiring selected for ST7789 240×280 (landscape UI 280×240):

- SCLK: GPIO12
- MOSI/SDA: GPIO11
- CS: GPIO10
- DC: GPIO4
- RST: GPIO5
- MISO and software backlight control: unused
- Status LED: disabled (onboard LED configuration is unverified)
- Native USB: GPIO19/20, not assigned to the display

GPIO0/3/45/46 (strapping), GPIO19/20 (USB) and memory pins are not used by the display. Display wiring still requires visual confirmation on the device.

The connected development unit was identified on 2026-11-04 as ESP32-S3 revision v0.2, with 4 MiB embedded XMC Flash and 2 MiB embedded AP_3v3 PSRAM. Defaults use the 4 MiB partition layout and enable Quad PSRAM at 40 MHz with a boot memory test. Other boards sold under the same name may differ.

```sh
python3 tools/select_device.py --list
python3 tools/select_device.py --device flint_spark_s3_supermini --build
```

Board implementations live in `src/board/models/flint_spark/s3_supermini/`; parameters in `config/devices/flint_spark/s3_supermini/`. The selector writes `device_config.h`, `device_display.h` and `device.cmake` into `<build-dir>/generated/board/`. Generated files are ignored by git.

See [platform migration status](../../../platform/README.md) for what is implemented and what still needs replacing.
