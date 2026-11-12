# Hardware data service

Core exclusively owns the public sensors::Broker, permissions, subscriptions and cache.
The OS backend transports bounded desired snapshots and one latest sample per source
(up to16). Slow processing coalesces samples without memory growth. The worker never runs
Lua or reads Core state; it only notifies the owner task that transport data is available.

One conditional FreeRTOS worker owns all I2C, including touch/RTC, and battery ADC.
Without these configured modules its task and mailbox/cache are compiled out. The active
flint_spark still selects ST7789 only; this change does not activate the future profile.
The catalog comes from configuration; public sensor handles/chip ID checks start on demand.

CST816 reset uses10/200ms deadlines. IRQ wakes the worker; contacts use16ms reads. Touch
is serviced between I2cDevice transfers (5ms timeout each). Core drains up to16 raw records
with acquisition generation. Overflow cancels gestures; stale generations are suppressed
until release. Physical latency remains unverified.

BMP/BME: forced12ms conversions then sleep, minimum1000ms/default5000ms; Bosch init uses
one transaction per worker turn. VEML7700: minimum500ms/default1000ms, gain1/8,100ms
integration,210ms deadline and shutdown after reads. Saturation fails; no autorange or
nonlinearity correction. IMU: minimum20ms/default40ms, CTRL2/CTRL3 ODR code7 below36ms,
code8 otherwise. Exact6DOF rate depends on QMI8658C revision. Acceleration is m/s2,+/-2g;
angular velocity rad/s,+/-512dps. Device axes are identity sensor X/Y/Z, never display-rotated.
Case/screen alignment and mounting calibration are physically unverified.

Without demand there is no public sensor polling; IMU powers down and ALS finishes its
current conversion then shuts down. Worker sleeps until IRQ, changed demand or a real
deadline. Failures stop drivers. Recovery increments source generation and publishes pending
to invalidate old good cache. Up to3 attempts before a valid sample use2/4s backoff; then
a new explicit refresh enables another cycle. Reading never renews sample timestamps.

Battery ADC uses a30s system cache. power keys: voltage_mv,age_ms,measured_at_ms,status;
failed or older-than90s voltage is unavailable. RTC UTC cache extrapolates from monotonic
time without Core-side I2C. One asynchronous pending write uses a token and completes
after verified readback. Oscillator-stop and malformed BCD fail.

PWM remains Core-owned. Beep limits:5s continuous,100ms between commands, quiet time at
least the previous burst, mute defaultfalse persisted by Core. Repeated commands cannot
extend a burst past5s. PWR/BOOT IRQs use30ms debounce and1000ms hold, focused input value1/2.
Power hold is asserted before task creation. Releasing power is explicit and requires
storage/transports quiesced; buttons never cut power themselves.

Diagnostics expose metadata, not raw permission-controlled readings: wakes/missed deadlines,
max wake/cycle times, stack watermark, gaps/coalescing and I2C/ADC counts/errors/durations.
These are runtime counters, not measured power. I2C counters cover I2cDevice transfers;
CST816 raw transfers use a separate driver.

## Build and migration

ESP-IDF6.1, ESP32-S3 and IgniteSDK ContractRevision0x20261113 are required. Until the SDK
is committed/pinned, configure -DFLINT_PLATFORM_DIR=../Flint-IgniteSDK; configure rejects
older SDKs. Rebuild packages for the incompatible revision. Partition layout and storage
formatting are unchanged; /storage/packages/settings.bin and grants.bin remain in place.
Existing grants never implicitly authorize new sensor permissions.

## Hardware validation still required

Host mocks and ESP32-S3 compile/build do not prove physical behavior. Check touch latency,
IRQ/overflow under I2C load; wiring/pullups/addresses; RTC oscillator-stop, battery removal,
readback/reboot; ADC calibration/divider; IMU ODR/axes/data readiness; disconnect/reconnect/
recovery; button bounce/hold and USB power. Record counters/stack, CPU awake time and
battery/USB current for idle/no-demand, ALS,20/40ms IMU and BLE. Test power loss during
settings/grants/RTC writes and safe shutdown. These timing, hardware and energy tests
have not been performed.

PWM deadline stop runs on the Core event loop: physical overrun under task contention needs device measurement; this is not a hardware cutoff timer.

## SDK revision

The sensor SDK is pinned to local commit `3c8ad11` in `platform/`. Normal builds use this submodule; an external FLINT_PLATFORM_DIR is only needed for deliberate SDK development.
