# FlinT OS assets (LittleFS)

Editable PNGs in category folders → one pack on `storage`.

**Rule:** pack only what a designer edits as a picture. Lines, fills, and text
stay draw primitives (fonts / `fillRect` / hairlines). Do not rasterize chrome
or copy for flash or i18n.

## Workflow

1. Edit / add PNG under the right category folder
2. `idf.py build` (runs `gen_asset_pack.py` only if PNG / `FlintIconId` changed)
   — or manually: `python3 tools/gen_asset_pack.py`
3. `idf.py flash` (full flash rewrites storage; `app-flash` keeps it)

## Categories

| Folder | Use |
|--------|-----|
| `icons/` | Status (wifi), shared UI glyphs |
| `splash/` | Boot mark (`BrandMark`); optional full splash frame later |
| `chrome/` | Designer bitmap decor only — not hairlines / bars |
| `glances/` | Face glance art beyond shared icons (reserved) |
| `fs_root/` | Generated pack only — do not edit |

## Naming

`<FlintIconId>_<size>.png` — names and numeric IDs come from
`components/flint/sdk/resources.h` (`FlintIconId`); the pack tool reads that enum
(do not duplicate IDs in Python).

```text
icons/Wifi3_16.png
splash/BrandMark_52.png
```

RGBA, filter None; transparent = empty, opaque = ink (tint at runtime).
Runtime blit skips empty pixels (no solid size×size plate behind the glyph).

## Runtime blit

Pack loads **resident in RAM** when it fits a heap budget (grows with icons;
falls back to stream-from-LittleFS if larger). Icons decode to an RGB565 scratch
then one `pushImage`. Keep dirty/clip paths; do not expand full present to
compensate for slow draws.
