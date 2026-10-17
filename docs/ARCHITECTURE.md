# FlinT OS architecture

Firmware platform for **FlinT** devices (base: **FlinT Spark**), by **DeeTech Labs**.
Face paint contract and dirty/event wiring. Module map: [`MODULES.md`](MODULES.md). Face SDK: `components/flint/sdk/flint_face_sdk.h`.

## Layers

```text
hardware / board JSON
  → services (time, USB, settings/cfg, day_mode)
    → FaceContext (uiBuildContext)
      → Face::onTick / onForceRedraw
        → display (partial or full present)
```

Faces read `FaceContext` and draw. They must not call USB/GPIO, NVS, or settings/cfg APIs.

## Day mode and waiting

Time-of-day modes (`MORNING` / `DAY` / `EVENING` / `NIGHT`) come only from a **valid** local hour via `dayModeFromHour` (`day_mode_hour.*`).

When `clockTimeGetLocal` fails (`!hasTime`), `uiBuildContext` sets **`DAY_MODE_WAITING`** — not a synthetic noon hour. Waiting uses a neutral profile (`showSeconds` / `showModeLabel` off for AUTO; date line already carries “Waiting for time”). `dayModeFromHour` never returns `WAITING`.

## Events → dirty → paint

`flintEventEmit` → `uiDirtyBridge` (`event/event.cpp`) → `uiMarkDirty` → `uiFaceTick` (`ui/ui.cpp`).

| Event / source | Dirty bit(s) | Face entry |
|----------------|--------------|------------|
| `FLINT_EVT_SECOND` | `UI_DIRTY_SECOND` | `onTick` |
| `FLINT_EVT_MINUTE` | `UI_DIRTY_MINUTE` | `onTick` |
| `FLINT_EVT_USB` | `UI_DIRTY_USB` | `onTick` |
| shell motion cadence (`uiMarkDirty`) | `UI_DIRTY_MOTION` | `onTick` (colon pulse, …) |
| `FLINT_EVT_SETTINGS` | `UI_DIRTY_SETTINGS` | `onTick` (**soft**) |
| `FLINT_EVT_DAY_MODE` | `UI_DIRTY_DAY_MODE` | `onForceRedraw` |
| `FLINT_EVT_TIME_VALID` | `UI_DIRTY_TIME_VALID` \| `FORCE` | `onForceRedraw` |
| `FLINT_EVT_FORCE_REDRAW` | `UI_DIRTY_FORCE` | `onForceRedraw` |

### Prefs vs FORCE

- **Prefs are soft.** After a prefs change on **`/cfg/ui.cfg`**, emit **`FLINT_EVT_SETTINGS` only**. Do **not** also emit `FLINT_EVT_FORCE_REDRAW`.
- `uiFaceTick` routes `UI_DIRTY_SETTINGS` to **`onTick`**, not `onForceRedraw`.
- Faces must treat `UI_DIRTY_SETTINGS` (and/or changed fields in `FaceContext`) as layout-sensitive: rebuild time layout, date line, mode label, i18n strings as needed — often a full paint **from `onTick`**.
- Use **`FLINT_EVT_FORCE_REDRAW`** for shell transitions (enter face), explicit user force, and other “wipe the canvas” cases — not as a stand-in for prefs.

Clock edits from the settings overlay emit `MINUTE` / `TIME_VALID` as appropriate; returning to the face still uses shell `FORCE` on enter.

## Persistence

| Store | Role |
|-------|------|
| `storage` LittleFS | Asset pack (`core.flintpack`) |
| `cfg` LittleFS | UI prefs (`ui.cfg`) + soft clock (`time.cfg`) |
| ESP-IDF NVS | Legacy migration — **not** FlinT UI prefs |

One-shot migrators still copy old `"flint"` NVS UI / epoch keys into `cfg` when the file is missing (`settings.cpp`, `clock_time.cpp`). **Sunset plan:** keep until field devices are known migrated; then delete migrate paths and NVS UI key reads in the same change (no long-lived dual-write).
