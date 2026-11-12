[Русский оригинал](../../ru/development/configuration.md)

# Firmware configuration

`sdkconfig.defaults` selects ESP32-S3, binary native USB, authenticated NimBLE and LittleFS. `partitions.csv` targets the verified 4 MiB Flash: two 1024 KiB application slots, 192 KiB protected factory packages and 1728 KiB writable user storage. The separate 2 MiB PSRAM does not change Flash capacity.

The new map moves `ota_1`, `factory` and `storage`. Before applying it to an existing device, back up and migrate user storage and provision the new factory image. Do not apply it as an application-only OTA update.

Hardware sources: `src/modules/st7789/`. Device selection generates the board headers before component configuration. There is no C3 profile.

## BLE policy

`ble_enabled` is persisted separately from Companion and defaults to enabled. Restore access over USB after disabling it. `CONFIG_FLINT_BLE_START_MEMORY_REQUIREMENT` reserves 64 KiB internal beyond the common reserve for startup. See [ARCHITECTURE.md](../architecture/ble-lifecycle.md) for lifecycle, worker, diagnostics and failures.

[Configuration layout and migration](repository-layout.md). Version: `config/version.cmake`; default ID: `config/devices.json`; generated files: `<build-dir>/generated/`.

## Device configuration

Python **3.11+** (standard-library `tomllib`) and Pillow are required. Each model
has one editable file: `config/devices/<model>/device.toml`. The beginning of the
[complete Spark configuration](../../../config/devices/flint_spark/device.toml):

```toml
name = "FlinT Spark / S3 Super Mini development"

[modules.st7789]
rotation = 3
frequency = 80000000
pins = { mosi = 11, sclk = 12, cs = 10, dc = 4, reset = 5 }
```

An omitted module is disabled: its sources and ESP-IDF requirements are excluded
from firmware. Missing display/touch devices use minimal OS interface adapters.
Without a display, recovery reports its reason through the logger.

The same file contains `[modules.st7789.panel]`: glass and GRAM dimensions,
offsets, MADCTL, inversion, three-wire and the complete power/gamma sequence.
There are no separate panel files or preset references. `rotation`, `frequency`
and GPIO belong in `[modules.st7789]`. Only rotation 3, 240×280 glass, 240×320
GRAM and a 280×240 canvas are implemented; other geometries are rejected.
Matching dimensions do not establish matching electrical parameters or init commands.

Enable CST816 by adding a section to the same file. The syntax example below uses
hypothetical free GPIO and **is not a Waveshare wiring diagram**:

```toml
[modules.cst816]
pins = { sda = 1, scl = 2, reset = 6, interrupt = 7 }
raw_size = [240, 280]
swap_xy = true
# mirror_x = false
# mirror_y = false
# frequency = 400000
# address = 0x15
```

Swapped raw dimensions must match the canvas. Verify mirroring and GPIO against
the physical board. Validation covers types, unknown fields, ESP32-S3 GPIO ranges,
duplicate GPIO assignments, dimensions, rotation, GRAM bounds and command lengths.
It does not validate board wiring, power or board-specific Flash/PSRAM pin
reservations; hardware verification remains necessary.

Canvas dimensions come from the supported panel geometry. Without a display,
the OS viewport remains 280×240; arbitrary canvas dimensions are not configurable.

Models are discovered automatically from `config/devices/<model>/device.toml`.
The ID is the directory name: `flint_spark`. `config/devices.json` stores only the
default ID. No controller subdirectory is needed. The future Waveshare-based Spark
configuration lives alongside it in `config/devices/flint_spark/device.future.toml`;
the selector ignores this file. Once the board arrives, verify and complete the
file before replacing `device.toml`. This is Spark's future hardware, not a separate
device model.

The old ID `flint_spark_s3_supermini` is replaced by `flint_spark`. For an existing
CMake cache, explicitly set `-DFLINT_DEVICE=flint_spark`; `select_device.py --build`
passes the new ID automatically.

```sh
python3 tools/select_device.py --list
python3 tools/select_device.py --device flint_spark --build
```

`src/modules/<id>/module.json` describes driver roles, sources and requirements;
it does not change when adding a board. New hardware types require a real driver,
a manifest and a configuration validator in the selector. A TOML section cannot
enable an unimplemented driver.

`device_config.h`, `device_display.h`, `device_touch.h` and `device.cmake` are
generated under `<build>/generated/board/`. The model's manual `device_config.h`,
`device.cmake`, `display.h` and `modules.json` are removed and no longer consumed.
CMake tracks TOML and manifests. Switching configurations removes stale headers
for disabled devices; invalid configurations leave previous generated files intact.
Protocol, Flash partitions and the pinned SDK are unchanged.

## Additional peripherals

Supported modules include `qmi8658`, `pcf85063`, `battery_adc`, `pwm_buzzer`,
`pwm_backlight`, `power_button`, and external `bmp280`, `bme280`, `veml7700`.
Sections, GPIO, native APIs, units and limitations are documented in the
[future Spark guide](../hardware/waveshare-touch-preparation.md) and module headers.
I2C sensors share SDA/SCL; address conflicts are rejected. BMP280 and BME280 can
coexist at different addresses.
