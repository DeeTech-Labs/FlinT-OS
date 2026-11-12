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

Package admissibility uses the stable profile limit of 48 KiB internal and installed PSRAM capacity. Launch checks current free/largest values separately: `CONFIG_FLINT_INTERNAL_MEMORY_RESERVE` (32 KiB default) protects DMA, transport and recovery; `CONFIG_FLINT_PSRAM_MEMORY_RESERVE` defaults to 8 KiB. Asynchronous BLE startup retains another 32 KiB. The mandatory-service baseline is a reference; launch uses the current largest block, including after BLE releases memory. PSRAM never increases the internal budget. Reserves are conservative and still require full hardware stress validation; BLE pools and stacks have not been reduced.

The USB RX ring is bounded to 4160 bytes: one MaxPayload=4096 frame including framing plus an empty deferred commit/close. The increase from 2048 is justified by observed burst loss during Lua/display work: IDF silently drops RX when its ring overflows. The client keeps at most one normal request plus one empty `install_commit`; unlimited pipelining is unsupported. Transfer/parsing remains bounded to 256 bytes per pass.

The IDF SOF monitor can briefly report absence after delayed Flash/tick callbacks. OS confirms 20 ms of continuous absence using the IRAM clock, not a count of delayed ticks. Confirmed absence closes the session and clears driver buffers; hello and session_close separate logical clients immediately. `usb_sof_glitches` counts suppressed brief absences and `usb_rx_capacity_bytes` reports ring capacity. The public IDF driver does not expose a direct ring-drop counter.

## Display: controlled stripe comparison

On Spark, SDK b318eaa, 80 MHz SPI, BLE advertising, 20 launcher/clock alternations (40 full 280×240 frames, 2688000 pixels), single rows were compared with two-row stripes:

| Per frame | 1 row | 2 rows |
|---|---:|---:|
| Pixel transfers | 240 | 120 |
| SPI | 27.037 ms | 20.429 ms |
| Copy | 10.609 ms | 10.551 ms |
| Paint | 20.606 ms | 20.613 ms |

SPI time fell by 24.4%; the sum of these three measurements fell about 11.4%. This is a controlled comparison, not a comparison against unknown baseline firmware history. Neither run showed display errors or USB session resets. Differences in `display_pixels`, `display_transfers`, `copy_total_us`, and `spi_total_us` reproduce the comparison; SDK supplies `paint_total_us`. Counters cover displayScene, excluding initial fill/recovery.

The 1120-byte DMA stripe uses the existing union with the previous Scene; static_assert preserves total scratch size. The driver accepts at most two rows and completes transfer synchronously before buffer reuse. Odd final rows and clipping are checked by GRAM comparison with the SDK renderer, SPI faults by host recovery tests. The full framebuffer remains; no unmeasured glyph caches or dirty-region sets were introduced. Physical tearing/orientation still need visual validation.

## Final RAM, Lua and image size

Launch uses current free/largest values; the baseline is retained for comparison only. BLE startup also preserves the unused part of the active app's declared internal budget. The Core owner performs this check; the worker cannot access Runtime. On a board running `CONFIG_SPIRAM=n`, clock/display/USB worked, temporary Lua launch failure with BLE enabled retained the valid installed package, disabling the radio allowed Lua to launch, and radio startup could not consume reserved app memory. BLE could be enabled again after stopping the app. This tests unavailable PSRAM, not physical chip fault injection.

The profile now sets `CONFIG_SPIRAM_IGNORE_NOTFOUND=y`: missing/failed external RAM initialization permits internal-only startup; an enabled memory-test failure still aborts in IDF. `.bss`/`.noinit` are not placed in PSRAM. `CONFIG_ESP_TIMER_IN_IRAM=y` is required for the USB observer during Flash writes; CMake rejects invalid configuration. Run `idf.py reconfigure build` after updating.

SDK reservation after the bounded header retains up to 4116 bytes for a maximum frame rather than geometric growth to 8192; `usb_parser_capacity_bytes` exposes actual capacity. BLE peer addresses use fixed-format parsing without scanf or byte-order changes. `gmtime_r` plus numeric formatting preserves `HH:MM` and `DD.MM.YYYY`; tests cover UTC offsets, day/year rollover and leap dates. The image before this optimization was 742208 bytes; after it and final fallback settings it is 732912 bytes, a 9296-byte reduction. The slot remains 1 MiB; the partition table is unchanged.

Map analysis: fonts.cpp is about 78844 bytes and the Lua VM about 14929. Roughly 57 KiB attributed to stdio includes a merged linker string pool; the actual `esp_stdio_register.str1.1` object section is 75 bytes. Such attribution must not justify dropping diagnostics. Assertions, Lua instruction limits, GC and protective checks were not weakened for size.

Isolated Lua 5.4.9 host profile: Windows, MinGW 13, Release library, heap cap 32768; median of seven batches of 3000 calls. A 1000-iteration integer loop took 3.756 µs/call without a hook and 17.937 µs with an equivalent production count-hook on every instruction. Allocating 100 temporary tables with normal GC took 10.845 µs/call and peaked at 11406 bytes; forcing a full collection after each call took 11.711 µs, including 3.422 µs GC, with a 17646-byte peak. This is an isolated host workload, not ESP32 timing or a proposal to disable hook/GC. The Spark test counter showed logic max 578 µs and Lua used/peak 5384/5422 bytes. Hook frequency and GC policy are unchanged.

A 717395-byte package passed hardware checks with chunk payload up to MaxPayload=4096: logical close before publication canceled verification and resume retained upload progress. During commit, 80–82 info requests plus scene requests were served while Lua continued. Verification took 3.65–4.51 s in separate runs. A step reached 625312 µs: durable metadata/Flash/GC remain synchronous and no strict wall-clock bound is guaranteed. The test package was removed and factory clock restored. Across ten warmed-up BLE start/stop cycles with Lua, largest internal block remained 102400 bytes and PSRAM stayed stable. These are scenario-specific peaks, not proof that every app fits.

## Power and validation limits

Current was not measured: no current meter/sensor is available. `ble_enabled` behavior and removal of periodic owner wakeups are verified but do not establish battery-life gains. The current profile uses 160 MHz CPU and 1000 Hz FreeRTOS; PM, BLE modem sleep and automatic Light-sleep remain disabled. ESP-IDF 6.1 DFS requires checking SPI/APB locks and timing constraints; automatic Light-sleep requires tickless idle and valid wake sources. Connected USB Serial/JTAG needs a PM-lock/USB-recovery policy; sleep must not remove the only route for re-enabling disabled BLE. Tick rate, PM and modem sleep were not changed without measurements.

The user will test encrypted BLE, repeated pairing and existing-bond retention after deinit/start on another Bluetooth-equipped laptop. The ecosystem-root TODO contains the task and handoff context. No Bluetooth adapter was detected on this PC. Linux/BlueZ builds, physical tearing/orientation, power cuts during commit/checkpoint, and physical PSRAM faults were not verified here. Host SPI faults and a no-PSRAM configuration do not substitute for them.

Verified: SDK CTest 14/14 with Lua and 8/8 without Lua; Flare CTest 17/17 including desktop_offline, preview_application_api and companion_sessions; OS pinned SDK 14/14, host 3/3, tooling unittest 2/2; ESP-IDF 6.1 firmware with and without PSRAM. The separate Qt Creator Qt 6.12/MinGW 13 build also compiled and passed 17/17. Package formats and ContractRevision are unchanged, persistent BLE defaults to enabled, and there is no partition migration. These results do not complete the hardware checks listed above.


## Resumable installation publication

The pinned SDK divides install_commit publication into owner-thread stages: proof
check, previous reference write, object move and atomic current reference replace.
Full GC no longer runs at commit completion: before publication it handles one
directory entry/reference pair per step with bounded memory. The active Runtime
continues until activation and survives durable publication failure. Permission
revocation is a separate step and skips grants.bin writes when no enabled grant
matches the package. Epoch checkpointing waits until installation finishes.

Disconnect before publication cancels validation while retaining resume; after it
starts the owner finishes publication and discards the disconnected session reply.
Reconnect must inspect the catalog. install_publishing exposes this boundary;
install_publish_{count,total_us,max_us} separate publication from validation/GC.
An individual LittleFS/Flash operation remains synchronous, without a hard
real-time guarantee.

Repeated validation after splitting publication (SDK a68abd4): image 734096 bytes,
build ID 2c324d3e0c8b2c0bf055ab8b110f092fa0bf6d9eb2a5e9b283c506aa28131724.
Two 717395-byte package runs took 3.59/4.04 s, each serving 82 info requests plus
scenes while Lua continued. Observed install_step maximum was 84.6–101.4 ms,
install_publish 53.5–67.6 ms, and maximum info latency 111.5/139.3 ms.
USB disconnects=0, diagnostics_truncated=0; test packages were removed.
The historical 625 ms above belongs to the previous finalization chain. Flash
state differs between runs, so this is neither a guaranteed speedup factor nor
a worst-case deadline. Warmed-up largest blocks still need encrypted BLE stress.

A repeatable hardware USB test is included in tools/check_install_responsiveness.py:
`python tools/check_install_responsiveness.py --port <port> --output <new.json>`.
It requires pyserial and the pinned SDK compiler tool dependencies, modifies the
device, installs temporary test.flint.installcheck, checks cancel/resume, measures
latency, restores the previous application and removes the test package. Close
Flare first to release the port. Existing JSON files are not overwritten. This
hardware-only check is excluded from unattended CTest without a device.

A third run during parallel host/firmware builds also passed: commit 5.385 s,
96 info requests plus scenes, maximum info latency 285.8 ms, install_step 200.6 ms
and install_publish 200.6 ms. Lua continued, USB disconnects=0,
diagnostics_truncated=0 and cleanup completed. All three JSON results are kept;
the slower run is included when assessing latency.

A local Spark check passed 10 start/stop cycles: disabled releases about 69 KiB internal, PSRAM remains unchanged and lifecycle errors=0. This validates memory/link behavior, not current consumption or retention of an existing bond through repeated pairing.
