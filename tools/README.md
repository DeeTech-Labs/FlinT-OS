# FlinT OS tools

Codegen helpers. Run from the **repository root** (`flint_os/`).

Interactive picker (scans `tools/*.py`, shows each script’s docstring summary):

```bash
./flint_tools
./flint_tools 1 -- --list          # by number + tool args
./flint_tools gen_board_catalog -- --set-default
```

| Tool | When to run | Output |
|------|-------------|--------|
| [`gen_board_catalog.py`](#gen_board_catalogpy) | After editing `components/flint/board/` (also auto on CMake configure) | `board_config.inc`, `board_impl.inc`, `board_build.cmake` |
| [`gen_i18n_catalog.py`](#gen_i18n_catalogpy) | After editing `components/flint/i18n/lang/` | `i18n_catalog.inc` |

Requires **Python 3** (stdlib only). `--build` / `--flash` / `--monitor` need ESP-IDF under
`~/.espressif` (or `IDF_PATH` / `FLINT_IDF_PATH`). The script finds `idf.py`,
creates the Espressif Python venv if missing (`idf_tools.py install-python-env`),
sources `export.sh` when needed, and runs `idf.py` with that venv (so a host with
only `python3` still works).

---

## `gen_board_catalog.py`

Builds hardware macros/CMake fragments from JSON under `components/flint/board/`.

Full board authoring guide: [`components/flint/board/README.md`](../components/flint/board/README.md).

### Commands

```bash
python3 tools/gen_board_catalog.py --help
python3 tools/gen_board_catalog.py --list
python3 tools/gen_board_catalog.py --list --json
python3 tools/gen_board_catalog.py --set-default              # interactive menu
python3 tools/gen_board_catalog.py --set-default flint_spark  # non-interactive
python3 tools/gen_board_catalog.py                      # codegen default board
python3 tools/gen_board_catalog.py --board flint_spark  # codegen only

# One-shot: codegen + set-target from JSON + build
python3 tools/gen_board_catalog.py --board my_desk_s3 --build

# Flash only (builds once if build/flint_os.bin is missing)
python3 tools/gen_board_catalog.py -b my_desk_s3 --flash -p /dev/ttyACM0

# Monitor only
python3 tools/gen_board_catalog.py --monitor -p /dev/ttyACM0

# Combine as needed
python3 tools/gen_board_catalog.py --flash --monitor -p /dev/ttyACM0
python3 tools/gen_board_catalog.py --build --flash -p /dev/ttyACM0
```

| Flag | Meaning |
|------|---------|
| `-h` / `--help` | Help + examples |
| `-l` / `--list` | List board ids (no files written) |
| `--set-default [ID]` | Set `board_index.json` default (numbered menu if ID omitted) |
| `--json` | With `--list`, JSON output |
| `-b` / `--board ID` | Board id (else `FLINT_BOARD`, else index default) |
| `-q` / `--quiet` | Generate without status line |
| `--build` | `idf.py build` (set-target if needed) |
| `--flash` | `idf.py flash` only; builds first **only if** `build/flint_os.bin` is missing |
| `--monitor` | `idf.py monitor` only (no build/flash) |
| `-p` / `--port` | Serial port for flash/monitor |
| `--no-set-target` | Skip `idf.py set-target` for build/flash |

Board selection order: `--board` → env `FLINT_BOARD` → `board_index.json` → `default`.

`idf_target` comes from the board JSON — you do **not** need a separate manual
`idf.py set-target …` when using `--build` / `--flash` (unless `--no-set-target`).

### Build integration (CMake)

Root `CMakeLists.txt` runs this on every configure **without** `--build` / `--list`
(codegen only — safe and fast):

```bash
# Still valid manual IDF flow:
idf.py set-target esp32c3
idf.py -DFLINT_BOARD=flint_spark build
```

Preferred after adding your board:

```bash
python3 tools/gen_board_catalog.py --board <id> --build
```

### Outputs (do not edit by hand)

- `components/flint/board/board_config.inc`
- `components/flint/board/board_impl.inc`
- `components/flint/board/board_build.cmake`

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

After changing:

- `components/flint/i18n/lang/flint.pot`
- any `*/flint.po`
- (and keep `lang_index.json` / `lang_map.json` in sync if adding a locale)

### Output (do not edit by hand)

- `components/flint/i18n/i18n_catalog.inc`

Not invoked by CMake automatically — run manually (or from CI) after i18n edits, then rebuild.
