# Architecture — FlinT OS

ESP-IDF firmware for **FlinT** devices (base board **FlinT Spark**). Product code lives in `components/flint/`.

Directory roles: [MODULES.md](MODULES.md). Adding a face: [FACES.md](FACES.md). Quick start / serial keys: [README](../README.md).

## Flash & boards

- Board profile: `-DFLINT_BOARD=<id>` (default `flint_spark`) — `components/flint/board/README.md`
- Partitions (`config/partitions.csv`): dual OTA app slots, `storage` (asset LittleFS), `cfg` (prefs + soft clock), coredump
- Full `idf.py flash` rewrites `storage`; `cfg` survives asset reflash

## Dependency direction

```
main → flint → LovyanGFX + ESP-IDF

flint (top → bottom):
  app → shell → ui | picker | settings UI
                  ↓
         settings, day_mode, time, net, display, i18n
                  ↓
              board, config, util

Community faces:
  face_*.cpp → sdk/flint_face_sdk.h (+ glance / ui_tokens as documented)
```

## Face contract

**May include:** `sdk/flint_face_sdk.h` and standard C/C++ headers; allowed helpers such as `faces/glance.h`, `display/ui_tokens.h`.

**Must not include / call:** `net/` service APIs, `settings/`, `app/`, `shell/`, `config/secrets.h`.

| Hook | When |
|------|------|
| `onActivate` | Face becomes active |
| `onForceRedraw` | Full paint |
| `onTick` | Incremental paint when dirty ≠ 0 |
| `onDeactivate` | Before switch |

Shell fills `FaceContext` each paint. Prefer context fields over reading services.

`FaceInfo.minApi` is checked at register / activate. Bump `FLINT_OS_API_VERSION` when the Face ABI breaks.

Fonts/icons: `flintFont` / `flintDrawIcon`. Pack when mounted; otherwise procedural fallback — [`assets/README.md`](../assets/README.md).

## Dirty events

Services mark bits in `ui/ui_dirty.h`; `uiFaceTick` paints only when dirty ≠ 0.

| Flag | Typical source |
|------|----------------|
| `SECOND` / `MINUTE` / `DAY_MODE` | app time scan |
| `WIFI` | Wi‑Fi up/down |
| `TIME_VALID` / `FORCE` | first NTP, explicit redraw |
| `SETTINGS` | prefs changed |
| `MOTION` | short face animations |

If `showSeconds` is false, lone `SECOND` events are ignored.

## Shell

States: `FACE` ⇄ `PICKER` ⇄ `SETTINGS`. Input is USB serial until hardware buttons exist — key table in the [README](../README.md#serial-controls-no-buttons-yet).

Factory default face when `cfg` is empty: `ACTIVE_FACE_ID` in `config/config.h`.

## Size posture

`config/sdkconfig.defaults`: `-Os`, no C++ exceptions/RTTI, Wi‑Fi STA WPA2-PSK only, trimmed LWIP/mbedTLS, no app HTTP client.

## Includes

From inside `flint`, paths are relative to the component root:

```cpp
#include "config/config.h"
#include "sdk/flint_face_sdk.h"
```
