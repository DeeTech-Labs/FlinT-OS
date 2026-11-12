[Русская версия](../../ru/hardware/waveshare-touch-preparation.md)

# Waveshare ESP32-S3-Touch-LCD-1.69 preparation

This is an inactive preparation, not a supported flashing profile. The device has
not arrived; electrical operation, touch orientation and power consumption have
not been verified. The selected `flint_spark_s3_supermini` configuration is unchanged.

According to [Waveshare documentation](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69),
the touch controller is CST816T, with I2C address `0x15`, SDA GPIO11, SCL GPIO10,
reset GPIO13 and interrupt GPIO14; the display resolution is 240×280.
Do not confuse this board with the separate 1.69inch Touch LCD module using CST816S.
Documentation contains different board revisions: check the received board markings
and its schematic before connecting hardware.

`src/board/models/waveshare/esp32_s3_touch_lcd_169/touch.h` contains
these parameters and a **raw portrait identity transform**. It is not included by
the active board and does not establish an orientation for the OS landscape canvas.
A complete profile requires ST7789V2, Flash/PSRAM, power, GPIO, backlight polarity
and canvas geometry configuration; those settings have not been added.

`input/cst816.cpp` independently implements a five-byte read starting at register
`0x02` (contact count and 12-bit X/Y), checks event bits and accepts single-contact
reports. Register `0xFA=0x60` enables periodic contact and state-change interrupts.
Register information comes from the [manufacturer example](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69/tree/main/examples/arduino/libraries/Arduino_DriveBus/src/touch_chip)
and its [bundled SensorLib](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69/blob/main/examples/arduino/libraries/SensorLib/src/touch/TouchDrvCST816.cpp).
These libraries are not included in the firmware.

The driver uses the ESP-IDF I2C master API with a 5 ms transaction timeout,
creates no task and preserves controller auto sleep. The IRQ only wakes the owner;
the Core task reads and processes reports. A shared I2C bus may be passed to `begin`:
the driver removes its own device but does not delete the borrowed bus. External
pull-ups are required for a driver-owned bus. The process-wide GPIO ISR service
survives `stop`; only this driver's pin handler is removed. If the existing ISR
service uses IRAM, the callback and its data must also remain accessible while
the flash cache is disabled.

After delivery, verify the four corners, holding, movement, release, cancellation
across application switches, I2C error recovery and idle power. This preparation
must not be treated as hardware-validated before those checks.
