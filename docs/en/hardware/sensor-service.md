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

ESP-IDF6.1, ESP32-S3 and IgniteSDK ContractRevision0x20261114 are required. Until the SDK
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

The sensor SDK is pinned to local commit `18d22f4` in `platform/`. Normal builds use this submodule; an external FLINT_PLATFORM_DIR is only needed for deliberate SDK development.


## Background service VM model policy

The optional [background] device.toml section is independent of hardware selection. Spark
uses provisional max_services=2, total declared memory131072B and foreground reserve49152B.
This is an admission ceiling, not a physical two-VM guarantee. A missing section or
max_services=0 disables the API and Core allocates no service slots. There is no FreeRTOS
task per service: Core dispatches cooperative VMs sharing one Sensor Broker cache/cooldown
with explicit InstanceKey ownership.

The100ms window allows16384 instructions/64 native calls, quantum4096/16,4000us wall,
2000us callback and1000us native-call budgets. Minimum timer/sensor/flush periods are
100/100/60000ms. Configuration rejects unknown fields, invalid types/ranges and inconsistent
nested budgets. Wall counters use esp_timer; instruction counts are not wall-clock limits.

Admission checks current internal free heap and largest contiguous block after subtracting
CONFIG_FLINT_INTERNAL_MEMORY_RESERVE, foreground reserve and32768B of additional BLE startup
headroom. Existing VMs already consume free heap; the separate total policy bounds all
services' declared memory. PSRAM is not advertised as additional internal service memory.
Two live VMs still require validation under BLE/UI/Flash load.

When battery ADC is configured, low-energy policy reads fresh cached voltage_mv only.
The default3400mV threshold is configurable as background.low_voltage_mv. It is a conservative
voltage policy, not SOC percentage or remaining-runtime estimation. Failed/stale/unknown
voltage does not imply low energy. No battery module means no callback. Core owns energy
pause/resume and reasons.

The factory Services app shows actual service rows, three per page, state/reason and
state-dependent Start/Stop, Pause/Resume, Auto and Revoke controls. Actions execute only on
input release; target/action changes during a gesture cancel the command. Users can stop
a service and revoke execution locally without Flare. Core requires a verified installed
factory package and system.diagnostics; ordinary UI/service VMs cannot gain this privilege
from a package ID. The app requires CapBackground and is excluded on unsupported models.

Configuration tests and sandbox UI tests with mocked rows do not establish physical VM
behavior. Measure two-VM admission, peak/fragmented heap, Core wakeups, wall/CPU under BLE/UI
load and current for running/paused/no-services. Validate voltage thresholds with calibrated
battery input and power-cycle/autostart/grant revocation. These hardware measurements are
pending; max_services=2 remains provisional.


## Static size of the validated build

On2026-11-13 the active flint_spark profile (ST7789 only, background policy2) was built
with ESP-IDF6.1.0, ESP32-S3 and pinned IgniteSDK 18d22f4 / ContractRevision0x20261114 through
the regular platform submodule. The factory image includes Services. `idf.py build size`:

| Metric | Bytes |
| --- | ---: |
| Padded flint_os.bin | 803760 |
| Total image sections from linker map | 803650 |
| Flash Code | 523884 |
| Flash Data | 171636 |
| Static DIRAM | 105142 |
| Remaining1MiB app partition | 244816 |

Static DIRAM remains105142B compared with the validated pre-background build:
.text72811B, .data18899B, .bss13432B. This is linker-map/esp_idf_size evidence, **not runtime
heap measurement** or proof that two service VMs fit in RAM. BackgroundState, the Core-owned
Broker, Lua VMs, queues and dynamic caches allocate at runtime and require real free-heap,
largest-block and peak measurements under load. These numbers describe this build, not
another toolchain/configuration/SDK revision. Local logs: temp/background-idf-final.log
and temp/background-idf-size.json.
