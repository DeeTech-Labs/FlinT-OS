# FlinT board profiles — setup guide

Build-time hardware selection for FlinT OS (same idea as `i18n/lang/`).

**All hardware authoring stays in this directory.** Do not edit
`display/LgfxBoard.h` or `components/LovyanGFX/CMakeLists.txt` to add a board or
panel — the generator wires `impl/` and LovyanGFX sources from JSON.

```bash
# From repo root — see also tools/README.md
python3 tools/gen_board_catalog.py --help
python3 tools/gen_board_catalog.py --list
python3 tools/gen_board_catalog.py --set-default              # pick catalog default
python3 tools/gen_board_catalog.py --board flint_spark
# codegen + set-target from JSON + build (ESP-IDF must be available):
python3 tools/gen_board_catalog.py --board <board_id> --build
python3 tools/gen_board_catalog.py --flash -p /dev/ttyACM0          # flash; build only if no .bin
python3 tools/gen_board_catalog.py --monitor -p /dev/ttyACM0        # monitor only
```

CMake still runs the generator on configure **without** `--build` (codegen only).
`-DFLINT_BOARD` remains optional when you drive `idf.py` yourself.

---

## Who writes what

| You want… | You edit… | C++ in `impl/`? |
|-----------|-----------|-----------------|
| New PCB / other pins, **same known panel** | `boards/*.json` + `board_index.json` | **No** |
| Same controller, other size / invert / offset | `periph/display/*.json` + board | **No** |
| Common SPI panel already in stock list | periph JSON (`"panel": "ST7789"` …) + board | **No** |
| Exotic panel or non-SPI bus | custom `impl/*.h` + periph + board | Yes (advanced) |

**Default path for community: JSON only.** Stock SPI factory is `impl/lgfx_spi_stock.h`
(maintained with the project). It picks the LovyanGFX panel class from periph `"panel"`.

Supported stock SPI panels (from common ESP32/Arduino TFT usage — guides + TFT_eSPI):
**ILI9341, ST7789, ST7735, ST7735S, ST7796, GC9A01** (alias `GC9A01A`),
**ILI9488, ILI9486, ILI9481, ILI9342, ILI9163**.
See `impl/README.md`.

---

## Layout

```text
board/
├── board_index.json          # catalog + default board id
├── boards/                   # one JSON per product / wiring
│   └── <board_id>.json
├── periph/
│   └── display/              # reusable panel profiles
│       └── <display_id>.json
├── impl/
│   ├── lgfx_spi_stock.h      # ★ default — do not copy per board
│   ├── README.md
│   └── <custom>.h            # only if panel not in stock list
├── board_config.inc          # GENERATED — C++ macros
├── board_impl.inc            # GENERATED — #include of selected impl
└── board_build.cmake         # GENERATED — LovyanGFX bus sources / requires
```

---

## Concepts

1. **`periph/display/`** — what the panel is (controller name, size, invert, SPI freq).
2. **`boards/`** — how *this* PCB is wired (GPIO, rotation, canvas, idf target).
3. **`impl/`** — C++ factory. Prefer stock (`lgfx_spi_stock`); custom only when needed.

Same display on two pinouts → two board JSON files, one shared periph. No new `.h`.

---

## Typical: add a board (known panel)

1. Copy `boards/flint_spark.json` → `boards/<your_id>.json`
2. Change `id`, `pinout`, `rotation`, `backlight`, `idf_target` if needed
3. Keep `"display": "st7789_spi_240x280"` (or another existing periph id)
4. Register in `board_index.json`
5. Build: `idf.py set-target …` then `idf.py -DFLINT_BOARD=<your_id> build`

---

## New size / quirks of a stock panel (still no C++)

1. Add `periph/display/<panel>_<bus>_<WxH>.json`:

```json
{
  "id": "st7789_spi_240x280",
  "type": "display",
  "panel": "ST7789",
  "bus": "spi",
  "panel_width": 240,
  "panel_height": 280,
  "offset_x": 0,
  "offset_y": 20,
  "invert": false,
  "rgb_order": false,
  "spi_3wire": true,
  "readable": false,
  "freq_write_hz": 40000000,
  "freq_read_hz": 16000000,
  "quirks": { "defer_dispon": true },
  "impl": "lgfx_spi_stock",
  "lgfx_sources": ["platforms/esp32/Bus_SPI.cpp"],
  "idf_requires": ["esp_driver_spi", "esp_driver_ledc", "esp_driver_gpio"]
}
```

- `"panel"` must be one of the stock names (case-insensitive).
- `"impl": "lgfx_spi_stock"` — or omit `impl` for SPI; generator defaults to stock.
- Naming tip: `<panel>_<bus>_<WxH>` (native orientation, before rotation).

2. Point a board’s `"display"` at that id; register the board; build.

---

## Greenfield (repo `board/` empty)

1. Keep / restore `impl/lgfx_spi_stock.h` (shipped with FlinT OS — not written per user).
2. Add `periph/display/….json` with `"panel": "ST7789"` (or other stock) and `"impl": "lgfx_spi_stock"`.
3. Add `boards/<id>.json` with pins + `"display"`.
4. Add `board_index.json` with `default` + boards list.
5. `python3 tools/gen_board_catalog.py` → then `idf.py … build`.

You still **do not** write a per-board `.h`.

---

## Advanced: custom `impl/*.h`

Only when:

- panel is not in the stock SPI list, or
- bus is not covered by stock (QSPI / RGB / …), or
- you need init sequences stock cannot express.

Then:

1. Add `impl/<custom_id>.h` defining `class LGFX` (use macros from `board_config.inc`).
2. Periph: `"impl": "<custom_id>"`, plus `lgfx_sources` / `idf_requires`.
3. Optionally extend `lgfx_spi_stock.h` instead, so the next person stays JSON-only.

See `impl/README.md`.

---

## Field reference

### `board_index.json`

| Field | Meaning |
|-------|---------|
| `default` | Board id when `FLINT_BOARD` / `-DFLINT_BOARD` is empty |
| `boards[].id` | Build selector |
| `boards[].name` | Human label |
| `boards[].file` | Filename under `boards/` |

### `boards/<id>.json`

| Field | Required | Meaning |
|-------|----------|---------|
| `id` | yes | Must match index id |
| `idf_target` | yes | Must match `idf.py set-target` |
| `display` | yes | Periph display id (filename stem) |
| `rotation` | yes | LovyanGFX `0…3` |
| `pinout.display` | yes (SPI) | GPIO; unused = `-1` |
| `backlight.gpio` | no | `-1` = none |
| `canvas.strategy` | yes | `bss` \| `psram` \| `none` |
| `canvas.max_w` / `max_h` | yes | FB size **after** rotation |
| `layout` | no | UI geometry label |
| `touch` | no | Future periph id |

**SPI pins required:** `cs`, `dc`, `rst`, `mosi`, `sclk`. Optional: `miso`, `busy`.

### `periph/display/<id>.json`

| Field | Required | Meaning |
|-------|----------|---------|
| `id` | yes | = filename stem |
| `type` | yes | `"display"` |
| `panel` | yes | Controller id → stock `FLINT_PANEL_KIND_*` |
| `bus` | yes | `spi` \| `qspi` \| `i80` \| `rgb` |
| `panel_width` / `panel_height` | yes | Native pixels |
| `offset_x` / `offset_y` | no | GRAM window offset |
| `invert` / `rgb_order` / `spi_3wire` / `readable` | no | → macros |
| `freq_write_hz` | yes | SPI write clock |
| `freq_read_hz` | no | default 16 MHz |
| `quirks.defer_dispon` | no | skip DISPON in init (ST7789) |
| `impl` | no* | default `lgfx_spi_stock` when `bus` is `spi` |
| `lgfx_sources` | yes* | relative to LovyanGFX `src/lgfx/v1/` |
| `idf_requires` | recommended | extra IDF components |

\*SPI without `lgfx_sources` defaults to `platforms/esp32/Bus_SPI.cpp`.

---

## Generator output

- `FLINT_PANEL_KIND_<PANEL> 1` (e.g. `FLINT_PANEL_KIND_ST7789`) — selects class inside `lgfx_spi_stock.h`
- pins, rotation, canvas size, SPI flags, `FLINT_PANEL_DEFER_DISPON`
- `board_impl.inc` → `#include "board/impl/<impl>.h"`
- `board_build.cmake` → LovyanGFX sources / requires

---

## Build selection

```bash
idf.py -DFLINT_BOARD=flint_spark build
```

Or one-shot from the generator (reads `idf_target` from the board JSON):

```bash
python3 tools/gen_board_catalog.py --board flint_spark --build
```

`idf_target` in the board JSON must match the current IDF target.

---

## Checklist

| Symptom | Check |
|---------|--------|
| `unknown board` | listed in `board_index.json`? |
| `panel … not in lgfx_spi_stock` | use a supported `"panel"` or add custom impl / extend stock |
| `missing impl header` | custom `impl` id without matching `.h`? |
| `pinout.display missing` | SPI needs cs/dc/rst/mosi/sclk |
| `expects idf_target=` | run matching `idf.py set-target` |
| Wrong colors / flash | `invert`, `defer_dispon`, `offset_y` |
| Canvas OOM / clip | `canvas.max_w/h` vs rotation |

```bash
python3 tools/gen_board_catalog.py --board <board_id>
```

---

## Out of scope here (for now)

- Face layout pixels (`display/ui_tokens.h`) — `layout` is a label only
- Touch / buttons — reserved board fields; add later under `periph/`
- Wi‑Fi secrets — `config/secrets.h`
