# FlinT OS tools

Codegen helpers. Run from the **repository root** (`flint_os/`).

Interactive picker (scans `tools/*.py`, shows each script’s docstring summary):

```bash
./flint_tools
./flint_tools 1 -- --list          # by number + tool args
```

| Tool | When to run | Output |
|------|-------------|--------|
| [`select_device.py`](#select_devicepy) | After editing `components/flint/board/` (also auto on **CMake configure**) | `device_config.h`, `device_display.h`, `device.cmake` |
| [`gen_i18n_catalog.py`](#gen_i18n_catalogpy) | After editing `components/flint/i18n/lang/` (also auto on **build** if inputs changed) | `i18n_catalog.inc` |
| [`gen_asset_pack.py`](#gen_asset_packpy) | After editing `assets/<category>/*.png` (also auto on **build** if inputs changed) | `assets/fs_root/core.flintpack` |

Schema / id smoke: `python3 -m unittest discover -s tools/tests -v` (see `CONTRIBUTING.md`).

Requires **Python 3** (stdlib only). `--build` / `--flash` / `--monitor` need ESP-IDF under
`~/.espressif` (or `IDF_PATH` / `FLINT_IDF_PATH`). The script finds `idf.py`,
creates the Espressif Python venv if missing (`idf_tools.py install-python-env`),
sources `export.sh` when needed, and runs `idf.py` with that venv (so a host with
only `python3` still works).

---

## `select_device.py`

Selects a fixed product revision from `components/flint/board/models/`.
Hardware files are maintained with the product; peripheral JSON composition is removed.

Full board authoring guide: [`components/flint/board/README.md`](../components/flint/board/README.md).

### Commands

```bash
python3 tools/select_device.py --help
python3 tools/select_device.py --list
python3 tools/select_device.py --list --json
python3 tools/select_device.py                      # select release device
python3 tools/select_device.py --device flint_spark_c3_dev  # select hardware files

# One-shot: codegen + set-target from the fixed revision + build
python3 tools/select_device.py --device flint_spark_c3_dev --build

# Flash only (builds once if build/flint_os.bin is missing)
python3 tools/select_device.py -d flint_spark_c3_dev --flash -p /dev/ttyACM0

# Monitor only
python3 tools/select_device.py --monitor -p /dev/ttyACM0

# Combine as needed
python3 tools/select_device.py --flash --monitor -p /dev/ttyACM0
python3 tools/select_device.py --build --flash -p /dev/ttyACM0
```

| Flag | Meaning |
|------|---------|
| `-h` / `--help` | Help + examples |
| `-l` / `--list` | List board ids (no files written) |
| `--json` | With `--list`, JSON output |
| `-b` / `--device ID` | Device revision id (else `FLINT_DEVICE`, else release default) |
| `-q` / `--quiet` | Generate without status line |
| `--build` | `idf.py build` (set-target if needed) |
| `--flash` | `idf.py flash` only; builds first **only if** `build/flint_os.bin` is missing |
| `--monitor` | `idf.py monitor` only (no build/flash) |
| `-p` / `--port` | Serial port for flash/monitor |
| `--no-set-target` | Skip `idf.py set-target` for build/flash |

Board selection order: `--device` → env `FLINT_DEVICE` → release default.

`idf_target` comes from the fixed revision — you do **not** need a separate manual
`idf.py set-target …` when using `--build` / `--flash` (unless `--no-set-target`).

### Build integration (CMake)

Root `CMakeLists.txt` runs this on every configure **without** `--build` / `--list`
(select hardware files — safe and fast):

```bash
# Still valid manual IDF flow:
idf.py set-target esp32c3
idf.py -DFLINT_DEVICE=flint_spark_c3_dev build
```

Build a supported revision:

```bash
python3 tools/select_device.py --device <id> --build
```

### Outputs (do not edit by hand)

- `components/flint/board/device_config.h`
- `components/flint/board/device_display.h`
- `components/flint/board/device.cmake`

---

## `gen_i18n_catalog.py`

Builds the string catalog from gettext sources under `components/flint/i18n/lang/`.

Locale authoring notes: [`components/flint/i18n/lang/README.md`](../components/flint/i18n/lang/README.md).

### Commands

```bash
python3 tools/gen_i18n_catalog.py
```

No CLI flags today — always regenerates the full catalog from `flint.pot` + locale `.po` files listed in the script.

### When to run

Usually **automatic**: root CMake (`cmake/flint_codegen.cmake`) regenerates
`i18n_catalog.inc` during `idf.py build` only when `.pot` / `.po` / lang JSON
(or the script) changed. Manual run still works:

```bash
python3 tools/gen_i18n_catalog.py
```

After changing:

- `components/flint/i18n/lang/flint.pot`
- any `*/flint.po`
- (and keep `lang_index.json` / `lang_map.json` in sync if adding a locale)

### Output (do not edit by hand)

- `components/flint/i18n/i18n_catalog.inc`

---

## `gen_asset_pack.py`

Packs `assets/<category>/<FlintIconId>_<size>.png` into LittleFS `core.flintpack`.
Icon IDs are parsed from `components/flint/sdk/resources.h` (`FlintIconId`).
Pack `format_ver` / `api_min` / `pack_ver` come from
`components/flint/config/flint_version.h`.
See [`assets/README.md`](../assets/README.md).

Usually **automatic** on build when PNGs / `resources.h` / the script change
(`flint_gen_asset_pack` → LittleFS `storage` image). Manual:

```bash
python3 tools/gen_asset_pack.py
```

Then full `idf.py flash` (not `app-flash` only) to rewrite `storage`.
The separate `cfg` LittleFS partition (UI prefs + soft clock) is not part of the
asset image and is left alone when only `storage` is rewritten.


## sync_time.py

USB time synchronization: `python3 tools/sync_time.py [--port /dev/ttyACM0]`.
Requires `pyserial`, included in the ESP-IDF environment. Use `--watch` to sync
on USB connection and hourly while it runs in your terminal. No startup service
is installed. See [USB-TIME.md](../docs/USB-TIME.md) for setup and protocol.
