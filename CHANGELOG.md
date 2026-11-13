# Changelog

All notable changes to **FlinT OS** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
for firmware (`FLINT_OS_VERSION_*` / git tags).

## [Unreleased]

### Changed
- Integrate SDK `18d22f4` / breaking contract `0x20261114` and FLM3 component manifests; rebuild firmware and factory packages together. Installed incompatible package data and grants are retained.
- Move shared I2C/ADC and touch/RTC access to one demand-driven hardware worker with bounded mailboxes, recovery generations and prioritized touch; remove the former Core-task sensor polling and direct measurement helpers.
- Integrate the SDK sensor/RTC/output contract and reject incompatible SDK configurations early. See [sensor integration and hardware acceptance](docs/en/hardware/sensor-service.md).
- Keep Spark configuration directly under `config/devices/flint_spark/` with model ID `flint_spark`; retain the future Waveshare-based hardware as an inactive `device.future.toml` alongside the active configuration.
- Configure each device with one self-contained `device.toml`, including panel parameters; discover profiles automatically, generate C++/CMake settings and exclude unselected hardware drivers. Device tooling now requires Python 3.11 or newer.
- Integrate the pinned SDK icon/touch/UI contract and generate registries and factory packages in the build directory without rewriting SDK sources.
- Render the application grid with 48 px color/theme icons, clipped alpha composition and bounded resource caching; rebuild factory packages for FLM3/FLIM v2.
- Move firmware sources to `src/`, board/version settings to `config/`, and branding/documentation assets to `resources/`; group RU/EN documentation by topic and generate board/version/factory artifacts in build directories. Preserve local `menuconfig` settings in `config/sdkconfig`.
- Rebuild factory screens for touch: right-edge Apps navigation, inertial grid scrolling, Settings controls and English/Russian labels.
- Show a protected factory Ignite splash only on physical power-on, then restore the selected clock without blocking command handling.
- Display actual transfer/verification progress, package-operation results, application failures, pairing comparisons and access-change notices. Keep confirmation in Flare and recovery independent of package storage.

- Require ESP-IDF 6.1.0 or newer; pin the firmware CI build to 6.1.0.

- Replace LovyanGFX with a board-specific ST7789 driver using ESP-IDF SPI Master and synchronous DMA completion. Preserve RGB565 rendering, dirty regions, panel settings and deferred display enable; render recovery with built-in SDK glyphs and a static scanline buffer.
- Share display scene-cache and DMA scanline storage while preserving dirty-region rendering and framebuffer-independent recovery.
- Reduce SPI validation code in IRAM by omitting detailed ESP-IDF check-macro messages only in `esp_driver_spi`; retain argument checks, error returns, assertions and FlinT error handling.

### Added
- Cooperative background-service admission using live internal heap, a model-wide memory/CPU pool, low-voltage pause policy and a factory Services screen for offline start/stop/pause/autostart/revoke. Spark limits are provisional until physical measurements.
- System battery-voltage freshness, verified asynchronous RTC write/readback, debounced focused buttons, muted/rate-limited buzzer control and diagnostic scheduling counters.
- Add optional shared-I2C QMI8658C/PCF85063A drivers, calibrated battery ADC, PWM backlight/buzzer and GPIO power/buttons for future Spark hardware; keep its explicit Waveshare pin-revision draft inactive.
- Add optional BMP280/BME280 forced measurements and VEML7700 ambient light readings with Core deadlines and no periodic polling. Document module usage in headers under `flint::modules`.
- Record the ESP32-S3 icon/UI resource validation, including RAM, stack, Flash and the remaining cold-launcher latency limitation in `docs/en/validation/2026-11-12-icon-ui-resources.md`.
- Add Core-task touch input with generation-safe cancellation, bounded active sampling and a gated CST816 driver; prepare Waveshare wiring without changing the selected Spark profile.
- Protocol v1 correlated settings/face commands with validation and atomic persistence.
- Persistent Studio face with configurable colors, text placement and 16x16 icon.
- NimBLE companion GATT service for INFO/STATUS/TIME with bounded indications.
- Shared USB/BLE application dispatcher and application-task command queue.
- Companion face with persistent quick note, autonomous focus timer and PC metrics.
- USB-only Companion command authorization and find-watch status indicator.
- Companion protocol and pairing limitations documented.


### Fixed
- Keep power hold asserted if worker creation fails, invalidate old touch acquisitions across app generations, and attempt ALS shutdown after cancellation/errors without exposing raw readings through ordinary diagnostics.
- Register the indication characteristic with the access callback required by NimBLE; without it GATT setup failed and the board did not advertise.
- Initialize the security key-store callbacks and require encrypted subscription before protocol exchange.
- Reserve a NimBLE key-store slot for pre-pairing capacity checks while keeping runtime bonding disabled.

## [0.2.0] - 2026-08-07

### Added

- Dirty-aware community face template (`community/face_template.cpp.example`) and FACES.md paint contract notes
- Host tests for `date_fmt`, day-mode hour/profile, and `tools/gen_*` schema smoke (`tests/host/`, `tests/tooling/`)
- GitHub Action `host-tests` on PR / `main` (host C++ + unittest)
- `DAY_MODE_WAITING` path when `!hasTime` (neutral profile; no synthetic noon → day)
- Shell overlay modules `shell_picker` / `shell_settings` (split from monolithic shell)
- CONTRIBUTING codegen section and Keep a Changelog `CHANGELOG.md`
- Configurable date format and separator in settings (DMY/MDY/YMD, `.` `/` `-`)
- JSON board hardware profile catalog (`tools/gen_board_catalog.py`) for FlinT Spark and future boards
- LittleFS asset pack (`core.flintpack`) with RSSI Wi‑Fi icons
- `cfg` LittleFS partition for UI prefs and soft-clock persistence (separate from asset `storage`)
- Community docs under `docs/` (architecture, faces, ESP‑IDF, modules) and contributor templates

### Changed

- Partial present: GRAM window + per-row `writePixelsDMA(w)` with FB stride
  (SPI = w·h·2, not full panel); multi-rect uses one panel txn; DEBUG_LEVEL≥2
  logs avg present µs / w×h / px
- Digital face presents only changed time glyphs (not full timeBand); AM/PM is
  a separate rect; time-font metrics cached across layout rebuilds
- `wifiNetBars()` returns maintain()/link-event cache (no `get_ap_info` per
  FaceContext); resident asset pack prefer-cap + 4-slot status icon decode cache
- Colon MOTION dirty only on fade-quantum change; time glyph path sets font once
  per batch
- UiTok design space / safe inset come from board `canvas.max_*` and `safe_inset` via `board_config.inc`
- Prefs dirty contract: `FLINT_EVT_SETTINGS` is soft → `onTick` (no paired `FORCE_REDRAW`)
- Digital face layout helpers + UI geometry tokens for time / AM·PM / date bands
- Retarget ST7789 panel to 240×280 with safe-area insets for rounded glass
- Tighten display path and shrink runtime footprint
- Keep Face API / pack / digital face version fields at 1 for pre-release

### Fixed


- Event bus logs when `flintEventSubscribe` rejects (full table / null handler)

### Removed

- Weather stack and HTTP client dependency

## [0.1.0] - 2026-08-05

Initial tagged release of **FlinT OS** for **FlinT Spark** (by **DeeTech Labs**).

### Added

- ESP‑IDF application with shell, settings, and event-driven faces
- Soft clock with SNTP sync, timezone DB, and brand splash
- Day modes with palettes, digital face, and glance hooks
- UI design tokens, overlay chrome, and motion helpers
- i18n catalog (en_US / ru_RU) and Face SDK surface for community faces

[Unreleased]: https://github.com/DeeTech-Labs/FlinT-OS/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/DeeTech-Labs/FlinT-OS/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/DeeTech-Labs/FlinT-OS/releases/tag/v0.1.0
