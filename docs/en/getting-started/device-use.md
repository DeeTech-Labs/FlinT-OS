# Using the device and recovery

[Index](../README.md) · [Canonical Russian](../../ru/getting-started/device-use.md) · [Flare steps](https://github.com/DeeTech-Labs/Flint-Flare/blob/main/docs/en/USER-GUIDE.md)

Development firmware is not generic ESP32-watch firmware. Active flint_spark is Super Mini+
ST7789, with no configured touch/buttons/battery/sensors. Manage this display-only board
through Flare; do not expect physical input, charge or temperature. Inactive device.future.toml
for Waveshare is physically unverified. Do not activate it without revision/GPIO/memory review.

A compatible preflashed device needs no reflash for Ignite installation. Use data USB, select
port then USB in Flare; wait for compatible handshake. Initial BLE pairing requires trusted
USB confirmation. A contract mismatch needs coordinated firmware/factory/Flare/packages,
not erased user data. Flare guide provides exact controls.

## Apps, access and services

Ignite Hub installs .ignit, confirms completion, launches selected components and selects
clock/theme/language roles. Factory identities are protected. Review requested data before
granting; declarations are requests. Revoke stops new readings/events, not already delivered
or intentionally stored app values. UI and service start independently; closing UI/Flare does
not stop device services. Diagnostics offers Start/Stop/Pause/Resume, execution revoke and
separate autostart permission/preference. Autostart defaults off and needs both grants.
Updates never grant new permissions automatically.

Factory Services supports offline stop/revoke on a model with working physical input.
**The active display-only Spark has no such input configured**; use Flare/USB instead.
Time sync/UTC offset require confirmed results; RTC failure can be partial Core/RTC success.
Brightness/automatic mode/mute require configured hardware. Battery shows voltage without
a validated SOC/charging model. Explicit diagnostic leases end on panel close/disconnect and
are separate from application grants.

## Troubleshooting

No port: data cable, power, OS port, close competing terminal/monitor, select port again.
Incompatible build: align contract revisions/factory/packages; never erase to bypass it.
`not_supported`: absent configuration/backend; requests cannot enable hardware.
`source_unavailable`: configured missing/failed/stale source; inspect state/reason and wiring.
`permission_denied`: component declaration, confirmed grant, current package generation.
pending/not_ready: wait for initialization/conversion deadline; zero is not an absent sample.
Service start: execute grant, model support, energy policy, memory/fault reason; do not raise
limits blindly. Faulted services require explicit restart after fixing the cause. Disabled BLE
requires USB to enable again. UTC changes cannot repair monotonic deadlines.

## Recovery and firmware

When connected: My watch → Watch settings → Restore factory components → confirm.
Core clears role selections and activates factory theme/locale/clock. This does not erase
storage, reinstall factory.bin, repair a corrupted volume or stop all background services;
stop services separately. If Core/factory/storage cannot start, this control may be unavailable.
Recovery rendering uses built-in glyphs independent of runtime framebuffer; no display leaves
logger output. Record symptom/revision/connection and use [support](../../../SUPPORT.md).

Flare has no firmware updater. Developers use [build/flash](building.md) with matching
factory.bin. Application-only OTA does not replace factory/partitions. Partition changes
require a separate backup/migration/recovery plan; sensor/service integration changed none.
Do not erase_flash for normal updates. Hardware BOOT/reset must follow the actual board,
not unverified future GPIO diagrams. Hardware durability/timing/power remain separate checks.
