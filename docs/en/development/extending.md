# Extending firmware and hardware modules

[Index](../README.md) · [Canonical Russian](../../ru/development/extending.md) · [Configuration](configuration.md)

Reproduce [build](../getting-started/building.md) and [tests](testing.md) first. Check active
TOML, datasheet/electrical revision; future config is not active wiring. Do not change
partitions/PSRAM/GPIO to make a build pass. Shared contracts belong in owning SDK.

## Add a module/source

Use an existing modules/BME280/BMP280/VEML7700 pattern: driver namespace flint::modules,
common bus/PWM/compensation flint::hardware. Add sources/header/module.json. Extend
select_device.py load_device/generated_headers with strict fields/types/ranges, pins/address/
shared bus validation and feature macros. A new directory alone does not define config syntax.
Extend test_device.py for selected/unselected source/dependency exclusion and invalid inputs;
src/CMakeLists consumes generated FLINT_MODULE_SOURCES/IDF_REQUIRES.

Only config/devices/<id>/device.toml is editable model truth; devices.json chooses default.
No separate panel headers/presets. Connect feature-guarded drivers in hardware/peripherals.cpp.
One worker owns every I2C client including touch/RTC; no parallel Core driver objects.
An absent module must create no driver/dependency/probe/bus/recovery/wakeups and return
not_supported. A configured failed source is source_unavailable with reason/state; conversion/
init is pending/not_ready, never placeholder zero.

Use sensors::Backend plus hardware/sensor_mailbox.h for bounded desired state/snapshots,
not another public cache. Backend normalizes source priority/ID, SI units, ranges/axes and
temperature kind. Apps use logical channels, not chips/GPIO/registers. Related BME samples
retain identity. Worker combines system/foreground/service demand, waits IRQ/deadline, gives
touch priority, splits init/recovery into states and sleeps/shuts down unused sources.
Recovery increments generation; cache reads do not change timestamps. Flash may delay workers:
record gaps/jitter/missed deadlines, not hard realtime claims.
[System policies](../hardware/sensor-service.md), [Broker contract](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/en/SENSORS.md).

## System consumers and validation

core/application.cpp adapts Platform callbacks. System consumers use Broker demand; no new
polling/authorization subsystem. Background admission uses live internal heap/largest block/
reserves and model policy; PSRAM does not replace internal/DMA RAM. Low voltage uses cached
fresh voltage, not SOC/charging. Services cannot implicitly render/wake displays.
[SDK scheduler](https://github.com/DeeTech-Labs/Flint-IgniteSDK/blob/main/docs/en/PLATFORM-GUIDE.md).

Run tooling, relevant host timeout/calibration/saturation/recovery tests, SDK broker/core
tests and ESP32-S3 builds with/without the module. Inspect generated device.cmake exclusion.
Physical acceptance records electrical revision/power/pull-ups, I2C/ADC counts/duration,
touch under sampling/Flash, heap/PSRAM/stack, age/gaps/coalescing, BLE/USB/install, shutdown/
recovery and idle/display/sampling current. Power-cut tests need protected data and separate
execution. Host admission of two VMs does not validate the model's final service count.
