# FlinT OS architecture

Firmware platform for **FlinT** devices (base: **FlinT Spark**), by **DeeTech Labs**.
This note covers the Face paint contract and dirty/event wiring. Module ownership: see [`MODULES.md`](MODULES.md) when present; Face SDK: `components/flint/sdk/flint_face_sdk.h`.

## Layers

```text
hardware / board JSON
  → services (time, net, settings, day_mode)
    → FaceContext (uiBuildContext)
      → Face::onTick / onForceRedraw
        → display (partial or full present)
```

Faces read `FaceContext` and draw. They must not call Wi‑Fi, HTTP, NVS, or settings APIs.

## Events → dirty → paint

`flintEventEmit` → `uiDirtyBridge` (`event/event.cpp`) → `uiMarkDirty` → `uiFaceTick` (`ui/ui.cpp`).

| Event / source | Dirty bit(s) | Face entry |
|----------------|--------------|------------|
| `FLINT_EVT_SECOND` | `UI_DIRTY_SECOND` | `onTick` |
| `FLINT_EVT_MINUTE` | `UI_DIRTY_MINUTE` | `onTick` |
| `FLINT_EVT_WIFI` | `UI_DIRTY_WIFI` | `onTick` |
| shell motion cadence (`uiMarkDirty`) | `UI_DIRTY_MOTION` | `onTick` (colon pulse, …) |
| `FLINT_EVT_SETTINGS` | `UI_DIRTY_SETTINGS` | `onTick` (**soft**) |
| `FLINT_EVT_DAY_MODE` | `UI_DIRTY_DAY_MODE` | `onForceRedraw` |
| `FLINT_EVT_TIME_VALID` | `UI_DIRTY_TIME_VALID` \| `FORCE` | `onForceRedraw` |
| `FLINT_EVT_FORCE_REDRAW` | `UI_DIRTY_FORCE` | `onForceRedraw` |

### Prefs vs FORCE

- **Prefs are soft.** After a settings/NVS change, emit **`FLINT_EVT_SETTINGS` only**. Do **not** also emit `FLINT_EVT_FORCE_REDRAW`.
- `uiFaceTick` routes `UI_DIRTY_SETTINGS` to **`onTick`**, not `onForceRedraw`.
- Faces must treat `UI_DIRTY_SETTINGS` (and/or changed fields in `FaceContext`) as layout-sensitive: rebuild time layout, date line, mode label, i18n strings as needed — often a full paint **from `onTick`**.
- Use **`FLINT_EVT_FORCE_REDRAW`** for shell transitions (enter face), explicit user force, and other “wipe the canvas” cases — not as a stand-in for prefs.

Clock edits from the settings overlay emit `MINUTE` / `TIME_VALID` as appropriate; returning to the face still uses shell `FORCE` on enter.
