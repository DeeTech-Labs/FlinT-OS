[Русский оригинал](../../ru/validation/2026-11-12-icon-ui-resources.md)

# Icons and system UI: device validation, 2026-11-12

Measured on an ESP32-S3 Super Mini, revision 0.2, 4 MiB Flash and 2 MiB PSRAM. Spark profile: CPU 160 MHz, 280×240 canvas, ESP-IDF 6.1.0; no physical touch (`touch_ready=0`).
OS: `a7ede1d0563f014995d046497b0605e3595266ef`; SDK: `9d768efc36df3c974c57c1d02211af1d1875bea7`.
Application: 751856 bytes, 28% free in the 1 MiB OTA slot. Running `hello` matched ELF descriptor SHA-256 `0c61e1c53232257a2723072fbffbc8075a3051d42afddcbfb8833f59724c46bd`.
Written image SHA-256: `dd3a434a1fec25cf88b4de252ec9a08d41d0fdb4f7e23341e70d0f5b459818d0`.

A fresh build used `build/pr-clean` without an SDK override; reconfigure/build checked the final pin. Commands: `idf.py -B build/pr-clean build`, `idf.py -B build/pr-clean reconfigure build`, `idf.py -B build/pr-clean -p COM3 flash`. A complete 4 MiB backup remains in ignored local `temp/`. Existing and generated partition tables matched byte for byte. Flash arguments wrote bootloader, table, otadata, ota_0 and factory, excluding NVS and writable storage. Esptool hash verification passed.

## Scenario and memory

Installed a separate test package based on SDK `examples/icons` (13814 bytes); its scene contained three ResourceImage operations (INDEX8, A4, RGB565+A8), and Launcher contained three CatalogIcon operations. Ran 12 Settings → test application → Launcher → Clock cycles, collecting 52 diagnostics snapshots including initial/restored states. Snapshots followed launch by 120 ms; lifetime counters include a preliminary run in the same boot. Host-observed installation took 2547 ms including USB, hash and publication, not isolated image loading.

| Screen, 12 cycles | Free internal RAM, bytes | Lua used/peak, bytes |
| --- | ---: | ---: |
| Settings | 158335–158435 | 18778 |
| Three images | 190607–190667 | 0 |
| Launcher | 146795–146943 | 29475 |
| Clock | 183707–183799 | 0 |

No monotonic free-memory decline was observed over these 12 cycles; first/last Clock values were 183727/183771 bytes. This bounded run does not prove absence of all leaks. Boot minimum internal heap: 96296 bytes; minimum observed largest block: 64512. PSRAM free: 1959796 in every snapshot; lifetime minimum: 1942896. Main task stack minimum watermark: 18044 bytes. These are whole-firmware metrics, not an isolated decoder peak.

Display errors, recoveries, resource-read errors, request errors and `last_logic_error` were zero. The test package was removed, the package set matched before/after (9), and the original screen was restored. Raw diagnostics and Flash backup stay local in `temp/`.

## Timing limitation

Cold Launcher entry produced a lifetime maximum full frame of **728234 µs**, raster maximum **494968 µs**, SPI maximum **31266 µs**. These nested intervals must not be added. Mean paint time per actually painted frame between adjacent snapshots ranged: Launcher 478704–494948 µs; Settings 50637–50727; three images 31603–31801; Clock 21494–21742. Launching Launcher again invalidates its file cache, so these are not warm redraw measurements for one icon.

Launcher entry latency is substantial. Cold lookup traverses the existing PackageManager and package validation; rendering also includes text/resources. There is no isolated hardware icon-decoder timer, so these results do not attribute all time to alpha blending or establish optimal performance. Profiling/optimizing cold entry remains separate work. USB validation did not establish display appearance, physical gestures, FPS or power consumption.

## Calculations and host validation

48×48 FLIM sizes including header: A4 1168 bytes; INDEX8 `2320+3N`, maximum 3088; RGB565+A8 6928. Clock/Settings and their two masks use 8089 payload bytes. Decoder row/palette storage reuses the existing 4096-byte glyph scratch; no full RGBA buffer. Target ABI sizes: decoder 36, IconCache 1088 bytes. Added main-Core fields total approximately 1148 bytes before padding, plus up to 3072 bytes for six LittleFS caches and FILE/VFS/allocator metadata. This is a calculation, **not a measured incremental peak**; no heap comparison against icon-free firmware was made. See [SDK benchmark](../../../platform/docs/en/FLIM-BENCHMARK.md).

Local validation: SDK 20/20 and 10/10 without Lua; Flare 24/24; OS host 5/5; tooling 9/9; Markdown links. SDK Windows/Linux CI passed both Lua configurations. Local positive SVG rasterization depends on Cairo availability. Host tests cover large catalogs, long names, clipping, invalid resources and theme changes; those cases were not exercised by this device scenario.
