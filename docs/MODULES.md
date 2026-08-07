# Modules — FlinT OS

What exists under `components/flint/` today. Paths are from that component root.

| Directory | Role |
|-----------|------|
| `app/` | Boot orchestration: bring up services, drive shell loop |
| `shell/` | State machine (`shell.cpp`) + overlays: `shell_picker.*`, `shell_settings.*` |
| `settings/` | Load/save prefs and soft clock on `cfg` LittleFS (`ui.cfg` / `time.cfg`) |
| `config/` | Compile-time defaults, version macros, local `secrets.h` |
| `board/` | Board JSON profiles → generated `board_config.inc` (canvas → `FLINT_DESIGN_*` / `FLINT_SAFE_INSET`) |
| `display/` | LovyanGFX HAL, shared fonts, icon draw, UI tokens (design space from board) |
| `assets/` | Mount `storage` LittleFS; blit from `core.flintpack` |
| `sdk/` | Public Face SDK headers (`flint_face_sdk.h`, resources) |
| `faces/` | `Face` API, registry, glances, built-in faces (+ `face_digital_layout.*`) |
| `day_mode/` | Hour→mode (`day_mode_hour.*`), profiles (`day_mode_profile.*`), palettes/wash |
| `i18n/` | Language packs / `flintTr` |
| `net/` | Wi‑Fi STA |
| `time/` | Soft clock, SNTP, date formatting helpers |
| `ui/` | Active-face paint path + dirty bitfield + chrome/splash |
| `event/` | Lightweight event bus |
| `anim/` | Short motion helpers (e.g. overlay fade) |
| `util/` | `millis` / delay / heap helpers |

Related outside `flint/`:

| Path | Role |
|------|------|
| `main/` | ESP-IDF entry only |
| `components/LovyanGFX/` | Slim graphics component |
| `config/` | `sdkconfig.defaults`, `partitions.csv` |
| `assets/` | PNG sources for the asset pack |
| `tools/` | Codegen (board catalog, i18n, asset pack) |
| `tests/host/` | Host C++ (date_fmt, day-mode, dirty rules, settings clamp); CI via `host-tests.yml` |
| `tools/tests/` | Unittest smoke for `gen_*` schema / ids (+ all boards validate) |
| `community/` | Face templates (not linked into the build) |
| `.github/workflows/` | CI (`host-tests.yml`) |

**Shell growth:** keep `shell_settings.cpp` focused — new setting rows / formatters / cycles go into helpers beside it, not back into a monolithic shell file.

Persistence split:

- **`storage`** — read-mostly asset pack (rewritten on full flash)
- **`cfg`** — mutable prefs + soft clock (survives asset reflash)
- **`nvs`** — ESP-IDF / Wi‑Fi system NVS (not FlinT UI prefs; one-shot migrate into `cfg` only)
