# Modules — FlinT OS

What exists under `components/flint/` today. Paths are from that component root.

| Directory | Role |
|-----------|------|
| `app/` | Boot orchestration: bring up services, drive shell loop |
| `shell/` | State machine: face, face picker, settings overlays |
| `settings/` | Load/save prefs and soft clock on `cfg` LittleFS (`ui.cfg`) |
| `config/` | Compile-time defaults, version macros, local `secrets.h` |
| `board/` | Board JSON profiles → generated `board_config.inc` |
| `display/` | LovyanGFX HAL, shared fonts, icon draw, UI tokens |
| `assets/` | Mount `storage` LittleFS; blit from `core.flintpack` |
| `sdk/` | Public Face SDK headers (`flint_face_sdk.h`, resources) |
| `faces/` | `Face` API, registry, glances, built-in faces |
| `day_mode/` | Day segments + palettes consumed via `FaceContext` |
| `i18n/` | Language packs / `flintTr` |
| `net/` | Wi‑Fi STA |
| `time/` | Soft clock, SNTP, date formatting helpers |
| `ui/` | Active-face paint path + dirty bitfield |
| `event/` | Lightweight event bus |
| `anim/` | Short motion helpers (e.g. colon pulse) |
| `util/` | `millis` / delay / heap helpers |

Related outside `flint/`:

| Path | Role |
|------|------|
| `main/` | ESP-IDF entry only |
| `components/LovyanGFX/` | Slim graphics component |
| `config/` | `sdkconfig.defaults`, `partitions.csv` |
| `assets/` | PNG sources for the asset pack |
| `tools/` | Codegen (board catalog, i18n, asset pack) |
| `community/` | Face templates (not linked into the build) |

Persistence split:

- **`storage`** — read-mostly asset pack (rewritten on full flash)
- **`cfg`** — mutable prefs + soft clock (survives asset reflash)
- **`nvs`** — ESP-IDF / Wi‑Fi system NVS (not FlinT UI prefs)
