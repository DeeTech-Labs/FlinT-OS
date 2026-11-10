# Diagnostics and baseline

Diagnostics use binary FLCP. Both primary and secondary USB Serial/JTAG consoles
are disabled; CMake rejects either conflicting selection in an existing sdkconfig.
ROM/bootloader startup output is separate from the application's binary channel.
On 10 November 2026 the clean ESP-IDF 6.1.0 config differed from the previous local
config only in the two secondary-console selections and the derived USB-console
enabled symbol. The local config was corrected and regenerated without replacing
other settings.

Read-only USB snapshot from the physical Spark S3, before replacing its firmware:
build `657ded42eb48132ec693f4e4d36b910f3a19a2da489f5fc000a9214e75c69346`,
capabilities 127, active `system.clock/main`. Maxima cover its previous uptime;
the duration and earlier scenarios are unknown.

| Metric | Baseline |
|---|---:|
| Internal free | 212047 bytes |
| Internal minimum | 207059 bytes |
| Largest internal block | 139264 bytes |
| PSRAM free | 1959796 bytes |
| Main task stack watermark | 27008 bytes |
| Max runtime tick | 49 us |
| Max frame (scene + display) | 189878 us |
| Lua used / peak | 0 / 0 bytes |

This is a baseline, not evidence of improvement or a complete memory peak. Current,
display/recovery errors, Lua, installation under load and BLE need separate board
scenarios. Do not shrink stacks or BLE pools based on this single snapshot.

New counters separate requests, runtime logic ticks, scenes, resource reads,
rasterization (including resource I/O), DMA row copies and SPI. Phases may be
nested and must not be added. USB TX counts bytes accepted by the driver; BLE TX
counts bytes accepted by NimBLE, not desktop delivery. `usb_read_empty_wakes`
counts deadline wakes without notifications. Notification causes coalesce IRQs.
`boot_snapshot_ready=1` marks the snapshot after Core and asynchronous BLE startup
or a reported initialization failure. Monitor minimum/largest heap and watermarks
throughout stress scenarios. SDK prioritizes Lua fault details and reports
`diagnostics_truncated=1` when optional fields exceed the reply limit.

The hardware USB module runs on the Core owner: one request and up to 256 RX/TX
bytes per pass. Partial TX frames survive backpressure. Driver callbacks and
physical-session edges notify the owner; unchanged USB ticks do not wake it.
The small connection accessor is mapped to IRAM for the tick hook. Do not attach
VFS/select as a competing owner of the binary channel's callback.

BLE RX, indication completion and connection changes notify the owner. The fixed
20-ms maintenance loop is replaced by pairing, TX retry and ready-event deadlines.
USB ISRs and NimBLE callbacks never call Core. Install verification now advances
in 4096-byte Core ticks, with deferred replies serialized as complete frames.
Metadata/durable writes and recovery remain synchronous; no strict wall-clock bound is claimed.

Builds default to pinned `platform/`. `-DFLINT_PLATFORM_DIR=/absolute/path/to/IgniteSDK`
is an explicit development override, not a replacement for pinned-submodule
validation before integration.

## RAM preflight

Package admissibility uses the stable profile limit of 48 KiB internal and installed PSRAM capacity. Launch checks current free/largest values separately: `CONFIG_FLINT_INTERNAL_MEMORY_RESERVE` (32 KiB default) protects DMA, transport and recovery; `CONFIG_FLINT_PSRAM_MEMORY_RESERVE` defaults to 8 KiB. Asynchronous BLE startup retains another 32 KiB. After the mandatory-service baseline, the internal budget is also bounded by the baseline largest block. PSRAM never increases the internal budget. Reserves are conservative and still require full hardware stress validation; BLE pools and stacks have not been reduced.

The USB RX ring is bounded to 4160 bytes: one MaxPayload=4096 frame including framing plus an empty deferred commit/close. The increase from 2048 is justified by observed burst loss during Lua/display work: IDF silently drops RX when its ring overflows. The client keeps at most one normal request plus one empty `install_commit`; unlimited pipelining is unsupported. Transfer/parsing remains bounded to 256 bytes per pass.

The IDF SOF monitor can briefly report absence after delayed Flash/tick callbacks. OS confirms 20 ms of continuous absence using the IRAM clock, not a count of delayed ticks. Confirmed absence closes the session and clears driver buffers; hello and session_close separate logical clients immediately. `usb_sof_glitches` counts suppressed brief absences and `usb_rx_capacity_bytes` reports ring capacity. The public IDF driver does not expose a direct ring-drop counter.
