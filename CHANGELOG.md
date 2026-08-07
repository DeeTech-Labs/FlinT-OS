# Changelog

All notable changes to **FlinT OS** are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project aims to follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
for firmware (`FLINT_OS_VERSION_*` / git tags).

## [Unreleased]

## [0.2.0] - 2026-08-07

### Added

- Dirty-aware community face template (`community/face_template.cpp.example`) and FACES.md paint contract notes
- Host tests for `date_fmt`, day-mode hour/profile, and `tools/gen_*` schema smoke (`tests/host/`, `tools/tests/`)
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
