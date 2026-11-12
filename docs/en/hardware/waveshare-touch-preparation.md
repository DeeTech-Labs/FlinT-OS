[Русский оригинал](../../ru/hardware/waveshare-touch-preparation.md)

# Future FlinT Spark using Waveshare ESP32-S3-Touch-LCD-1.69

The full hardware draft is in
[device.future.toml](../../../config/devices/flint_spark/device.future.toml).
The selector ignores it and the active Spark remains unchanged. The board has not
arrived: compilation and host tests do not validate electrical operation or power.

Inventory and the main pin assignment come from the [Waveshare guide](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69)
and [official examples](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69).

| Peripheral | OS implementation | Future configuration |
|---|---|---|
| ST7789V2, 240×280 | `flint::modules::St7789` | MOSI7, SCLK6, CS5, DC4, RST8 |
| CST816T | `flint::modules::Cst816` | I2C 0x15, RST13, INT14 |
| QMI8658C | `flint::modules::Qmi8658` | I2C 0x6b, INT38 |
| PCF85063A | `flint::modules::Pcf85063` | I2C 0x51, INT39 |
| Battery voltage | `flint::modules::BatteryAdc` | ADC1/GPIO1, 200k/100k divider, ×3 |
| Backlight | `flint::modules::PwmBacklight` | GPIO15, LEDC timer1/channel1 |
| Buzzer | `flint::modules::PwmBuzzer` | GPIO42, LEDC timer0/channel0 |
| Power/buttons | `flint::modules::PowerButton` | SYS_EN41, PWR40, BOOT0 |

Touch/IMU/RTC share SDA11/SCL10 and one ESP-IDF bus. Drivers own only their device
handles; the common bus lasts until firmware restart. External pull-ups are
required and provided in the board schematic. Duplicate I2C addresses or other
functions assigned to these GPIO are rejected.

## Revisions and autonomous circuits

The [V2.1 schematic](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.69/ESP32-S3-Touch-LCD-1.69_V2.1.pdf)
differs from the main guide: buzzer=39, RTC_INT=41, SYS_EN=42. The Arduino example
also contains an older PWR36/SYS_EN35/buzzer33 mapping. Verify markings and the
received revision's schematic before activation. The draft explicitly uses the
main guide's mapping and does not auto-detect a revision.

ETA6098 charging and the ME6217 3.3V regulator operate autonomously; the schematic
provides no I2C control interface. There is no invented charger driver, charge
percentage or charging-state telemetry. RST connects to CHIP_PU, not a GPIO.
16 MB Flash and 8 MB octal PSRAM require separate sdkconfig verification; TOML
does not change the active Spark's 4 MB partition layout or Quad PSRAM setup.

## Module usage

Every `src/modules/<id>/<id>.h` documents purpose, units, lifecycle, constraints
and an example. Drivers use `flint::modules`; common I2C/PWM and Bosch compensation
use `flint::hardware`. A section in the single device.toml selects a module;
unselected driver sources and IDF requirements are excluded.

`hardware/peripherals.h` connects the selected modules to the Core task:

- `deviceHardwareBegin()` holds power, initializes peripherals and leaves IMU,
  buzzer and backlight off. Display initialization enables backlight after filling GRAM.
- `deviceMotionEnable/Read` exposes raw acceleration/angular rate with fixed ranges;
  there is no polling task, fusion, automatic WoM or IMU interrupt handler.
- `deviceRtcRead/Set` reads/writes UTC. Oscillator-stop and malformed BCD are rejected;
  failed writes leave STOP set so partial time is not trusted. RTC is not automatically
  synchronized with Core epoch; alarm/wakeup is not implemented.
- `Platform::power` supports `voltage_mv` through the existing permission boundary.
  ADC readings are calibrated, with no synthetic fallback on failure.
- `deviceTone(Hz,durationMs)` is limited to 5 seconds; Core schedules its stop deadline.
  Off LEDC timers are paused. `deviceBacklight(0..100)` sets brightness.
- `deviceReleasePower()` must run on Core only after Flash writes and transports are
  quiesced. Buttons currently expose raw diagnostics without UI policy or debounce.

There are no new Lua APIs or permission bypasses. IMU/RTC/sound are native integration
APIs; readiness and measurements use existing diagnostics. RTC/IMU INT pins are
reserved but do not implement automatic wakeup.

## External BMP280, BME280 and VEML7700

These sensors are not onboard. Commented sections at the end of the future TOML
are examples to enable only after checking wiring, power and pull-ups. BMP280/BME280
use 0x76/0x77; VEML7700 uses 0x10. BMP280 and BME280 need different addresses if both
are connected.

Shared calibration and compensation follow the [Bosch BMP280 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf)
and [BME280 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf).
`deviceEnvironmentStart(EnvironmentSensor::bmp280/bme280)` triggers one x1 forced
conversion. Core completes it after a 12 ms deadline; `deviceEnvironmentRead` returns
the cached result in °C, Pa and %RH for BME280 only. There is no busy-wait, permanent
normal mode, altitude or sea-level correction.

`deviceLightStart/Read` follows the same pattern. The [Vishay application note](https://cdn.sparkfun.com/assets/8/7/4/4/1/VEML7700_Application_Note.pdf)
defines gain 1/8 and integration 100 ms; reading is scheduled after 210 ms with a
first-sample margin. ALS shuts down after reading, including I2C read errors.
Output is linear lux at 0.4608 lx/count; autorange and high-light nonlinearity
correction are not implemented. Saturation is an error. Diagnostics show the last
successful result; no periodic collection starts automatically.

## Physical validation

Verify revision GPIO, Flash/PSRAM, panel colors/orientation, all four touch corners,
RTC and battery-voltage accuracy, IMU axis signs, PWM, buttons and power release.
Measure idle power and test I2C/ADC failures separately.
