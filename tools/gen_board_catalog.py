#!/usr/bin/env python3
"""Generate FlinT OS board_config.inc / board_impl.inc / board_build.cmake.

Authoring: components/flint/board/{board_index,boards,periph,impl}.
Default / base device: FlinT Spark (flint_spark).
Board selection: --board / FLINT_BOARD / board_index.default.

CMake (root CMakeLists.txt) calls this with no flags or only --board — that path
must keep generating artifacts and printing a one-line status on stdout.
Interactive flags (--list, --set-default, --help) are for humans and are not used by CMake.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOARD_DIR = ROOT / "components" / "flint" / "board"
LGFX_SRC = ROOT / "third_party" / "LovyanGFX" / "src" / "lgfx" / "v1"
OUT_INC = BOARD_DIR / "board_config.inc"
OUT_IMPL = BOARD_DIR / "board_impl.inc"
OUT_CMAKE = BOARD_DIR / "board_build.cmake"

# Default LovyanGFX sources (relative to lgfx/v1/) when periph omits lgfx_sources.
BUS_DEFAULT_LGFX_SOURCES: dict[str, list[str]] = {
    "spi": ["platforms/esp32/Bus_SPI.cpp"],
}

# Stock SPI factory (board/impl/lgfx_spi_stock.h). JSON "panel" must match.
# Popularity basis: ESP32/Arduino TFT buying guides + TFT_eSPI / DIYables driver sets
# (ILI9341, ST7789, ST7735, ST7796, GC9A01, ILI948x family); aliases below.
STOCK_SPI_IMPL = "lgfx_spi_stock"
STOCK_SPI_PANELS = frozenset(
    {
        "ILI9341",
        "ILI9342",
        "ST7789",
        "ST7735",
        "ST7735S",
        "ST7796",
        "GC9A01",
        "ILI9488",
        "ILI9486",
        "ILI9481",
        "ILI9163",
    }
)
# Marketplace / module markings → canonical kind used in FLINT_PANEL_KIND_*.
STOCK_SPI_PANEL_ALIASES = {
    "GC9A01A": "GC9A01",
}

PROG_DESCRIPTION = "Generate FlinT OS board headers/CMake fragments from board/*.json"
PROG_EPILOG = """\
examples:
  %(prog)s --list
  %(prog)s --set-default
  %(prog)s --set-default flint_spark
  %(prog)s --board flint_spark
  %(prog)s --board my_desk_s3 --build
  %(prog)s -b my_desk_s3 --flash -p /dev/ttyACM0
  %(prog)s --monitor -p /dev/ttyACM0

  --build / --flash / --monitor are independent idf.py actions.
  --flash builds only if build/<project>.bin is missing.
  --monitor is monitor-only (does not build or flash).
  idf_target comes from the board JSON (set-target when needed for build/flash).

cmake / idf.py (configure only):
  Root CMakeLists.txt runs this without --build/--flash/--list (codegen only).
"""


def normalize_panel(panel: str) -> str:
    raw = re.sub(r"[^A-Za-z0-9]", "", panel).upper()
    return STOCK_SPI_PANEL_ALIASES.get(raw, raw)


def resolve_impl(display: dict) -> str:
    impl = (display.get("impl") or "").strip()
    if impl:
        return impl
    if str(display.get("bus") or "") == "spi":
        return STOCK_SPI_IMPL
    raise SystemExit(
        f"periph {display.get('id')!r}: missing impl "
        f"(non-SPI buses need an explicit impl id)"
    )


def load_json(path: Path) -> dict:
    if not path.is_file():
        raise FileNotFoundError(str(path))
    return json.loads(path.read_text(encoding="utf-8"))


def load_index() -> dict:
    return load_json(BOARD_DIR / "board_index.json")


def save_index(index: dict) -> None:
    path = BOARD_DIR / "board_index.json"
    path.write_text(
        json.dumps(index, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def index_board_entries(index: dict) -> list[dict]:
    """Normalized rows from board_index.json boards[] (id + name)."""
    rows: list[dict] = []
    for entry in index.get("boards") or []:
        bid = str(entry.get("id") or "").strip()
        if not bid:
            continue
        rows.append(
            {
                "id": bid,
                "name": str(entry.get("name") or bid).strip() or bid,
            }
        )
    return rows


def bool_lit(v: bool) -> str:
    return "true" if v else "false"


def c_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def resolve_board_id(requested: str, index: dict) -> str:
    board_id = (requested or os.environ.get("FLINT_BOARD") or "").strip()
    if not board_id:
        board_id = str(index.get("default") or "").strip()
    if not board_id:
        raise SystemExit("no board id: pass --board, set FLINT_BOARD, or board_index.default")
    return board_id


def find_board_entry(index: dict, board_id: str) -> dict:
    for entry in index.get("boards") or []:
        if entry.get("id") == board_id:
            return entry
    known = ", ".join(e.get("id", "?") for e in (index.get("boards") or []))
    raise SystemExit(f"unknown board {board_id!r} (known: {known})")


def board_json_path(entry: dict, board_id: str) -> Path:
    board_file = entry.get("file") or f"{board_id}.json"
    return BOARD_DIR / "boards" / board_file


def pin(pinout: dict, name: str, default: int = -1) -> int:
    disp = pinout.get("display") or {}
    if name not in disp:
        return default
    return int(disp[name])


def validate(board: dict, display: dict, board_id: str) -> None:
    if board.get("id") != board_id:
        raise SystemExit(f"board file id {board.get('id')!r} != selected {board_id!r}")
    if display.get("type") != "display":
        raise SystemExit(f"periph {display.get('id')!r}: type must be display")
    bus = display.get("bus")
    if bus not in ("spi", "qspi", "i80", "rgb"):
        raise SystemExit(f"periph {display.get('id')!r}: unsupported bus {bus!r}")
    for key in ("panel_width", "panel_height", "freq_write_hz"):
        if key not in display:
            raise SystemExit(f"periph {display.get('id')!r}: missing {key}")
    canvas = board.get("canvas") or {}
    for key in ("strategy", "max_w", "max_h"):
        if key not in canvas:
            raise SystemExit(f"board {board_id}: canvas.{key} required")
    if canvas["strategy"] not in ("bss", "psram", "none"):
        raise SystemExit(f"board {board_id}: unknown canvas.strategy {canvas['strategy']!r}")
    pinout = board.get("pinout") or {}
    disp_pins = pinout.get("display") or {}
    led = board.get("status_led") or {}
    led_gpio = int(led.get("gpio", -1))
    if led and led.get("active") not in ("high", "low"):
        raise SystemExit(f"board {board_id}: status_led.active must be high or low")
    used_pins = {int(p) for p in disp_pins.values()}
    used_pins.add(int((board.get("backlight") or {}).get("gpio", -1)))
    if led_gpio >= 0 and led_gpio in used_pins:
        raise SystemExit(f"board {board_id}: status LED conflicts with display GPIO{led_gpio}")
    required_pins = ("cs", "dc", "rst", "mosi", "sclk")
    if bus == "spi":
        missing = [p for p in required_pins if p not in disp_pins]
        if missing:
            raise SystemExit(f"board {board_id}: pinout.display missing {missing}")

    impl = resolve_impl(display)
    display["impl"] = impl  # normalize for writers
    if "/" in impl or "\\" in impl or ".." in impl:
        raise SystemExit(f"periph {display.get('id')!r}: impl must be a bare id (got {impl!r})")
    impl_h = BOARD_DIR / "impl" / f"{impl}.h"
    if not impl_h.is_file():
        raise SystemExit(f"missing impl header {impl_h} (add it under board/impl/)")

    panel_raw = str(display.get("panel") or "").strip()
    if not panel_raw:
        raise SystemExit(f"periph {display.get('id')!r}: missing panel")
    panel = normalize_panel(panel_raw)
    display["_panel_kind"] = panel
    if impl == STOCK_SPI_IMPL and panel not in STOCK_SPI_PANELS:
        known = ", ".join(sorted(STOCK_SPI_PANELS))
        raise SystemExit(
            f"periph {display.get('id')!r}: panel {panel_raw!r} not in {STOCK_SPI_IMPL} "
            f"(supported: {known}). Add board/impl/<custom>.h or extend lgfx_spi_stock.h"
        )

    lgfx_sources = resolve_lgfx_sources(display)
    if not lgfx_sources:
        raise SystemExit(
            f"periph {display.get('id')!r}: no lgfx_sources for bus {bus!r} "
            f"(set lgfx_sources in the periph JSON)"
        )
    if LGFX_SRC.is_dir():
        for rel in lgfx_sources:
            path = LGFX_SRC / rel
            if not path.is_file():
                raise SystemExit(f"periph {display.get('id')!r}: lgfx source missing: {path}")


def resolve_lgfx_sources(display: dict) -> list[str]:
    explicit = display.get("lgfx_sources")
    if explicit is not None:
        if not isinstance(explicit, list) or not all(isinstance(x, str) for x in explicit):
            raise SystemExit(f"periph {display.get('id')!r}: lgfx_sources must be a string list")
        return list(explicit)
    bus = str(display.get("bus") or "")
    return list(BUS_DEFAULT_LGFX_SOURCES.get(bus, []))


def write_inc(board: dict, display: dict) -> None:
    quirks = display.get("quirks") or {}
    canvas = board["canvas"]
    pinout = board.get("pinout") or {}
    bl = board.get("backlight") or {}
    led = board.get("status_led") or {}
    impl = display["impl"]
    defer = bool(quirks.get("defer_dispon", False))
    has_touch = bool(board.get("touch"))

    lines = [
        "// Auto-generated by tools/gen_board_catalog.py — do not edit by hand.",
        f"// Board: {board['id']}  display: {display['id']}  impl: {impl}",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        f'inline constexpr const char* FLINT_BOARD_ID = "{c_escape(board["id"])}";',
        f'inline constexpr const char* FLINT_IDF_TARGET = "{c_escape(board["idf_target"])}";',
        f'inline constexpr const char* FLINT_DISPLAY_ID = "{c_escape(display["id"])}";',
        f'inline constexpr const char* FLINT_DISPLAY_PANEL = "{c_escape(str(display.get("panel", "")))}";',
        f'inline constexpr const char* FLINT_DISPLAY_BUS = "{c_escape(str(display.get("bus", "")))}";',
        f'inline constexpr const char* FLINT_DISPLAY_IMPL = "{c_escape(impl)}";',
        f'inline constexpr const char* FLINT_CANVAS_STRATEGY = "{c_escape(canvas["strategy"])}";',
        f'inline constexpr const char* FLINT_LAYOUT = "{c_escape(str(board.get("layout", "")))}";',
        "",
        f"#define FLINT_DISPLAY_IMPL_{impl} 1",
        f"#define FLINT_PANEL_KIND_{display['_panel_kind']} 1",
        f"#define FLINT_HAS_TOUCH {1 if has_touch else 0}",
        f"#define FLINT_PANEL_DEFER_DISPON {1 if defer else 0}",
        "",
        f"inline constexpr int FLINT_STATUS_LED_GPIO = {int(led.get('gpio', -1))};",
        f"inline constexpr bool FLINT_STATUS_LED_ACTIVE_LOW = {bool_lit(led.get('active') == 'low')};",
        "",
        f"inline constexpr int FLINT_TFT_CS = {pin(pinout, 'cs')};",
        f"inline constexpr int FLINT_TFT_DC = {pin(pinout, 'dc')};",
        f"inline constexpr int FLINT_TFT_RST = {pin(pinout, 'rst')};",
        f"inline constexpr int FLINT_TFT_MOSI = {pin(pinout, 'mosi')};",
        f"inline constexpr int FLINT_TFT_SCLK = {pin(pinout, 'sclk')};",
        f"inline constexpr int FLINT_TFT_MISO = {pin(pinout, 'miso', -1)};",
        f"inline constexpr int FLINT_TFT_BUSY = {pin(pinout, 'busy', -1)};",
        f"inline constexpr int FLINT_TFT_BL = {int(bl.get('gpio', -1))};",
        "",
        f"inline constexpr uint8_t FLINT_TFT_ROTATION = {int(board.get('rotation', 0))};",
        f"inline constexpr int FLINT_PANEL_W = {int(display['panel_width'])};",
        f"inline constexpr int FLINT_PANEL_H = {int(display['panel_height'])};",
        f"inline constexpr int FLINT_PANEL_OFFSET_X = {int(display.get('offset_x', 0))};",
        f"inline constexpr int FLINT_PANEL_OFFSET_Y = {int(display.get('offset_y', 0))};",
        f"inline constexpr bool FLINT_PANEL_INVERT = {bool_lit(bool(display.get('invert', False)))};",
        f"inline constexpr bool FLINT_PANEL_RGB_ORDER = {bool_lit(bool(display.get('rgb_order', False)))};",
        f"inline constexpr bool FLINT_SPI_3WIRE = {bool_lit(bool(display.get('spi_3wire', False)))};",
        f"inline constexpr bool FLINT_PANEL_READABLE = {bool_lit(bool(display.get('readable', False)))};",
        f"inline constexpr uint32_t FLINT_SPI_FREQ_WRITE = {int(display['freq_write_hz'])}u;",
        f"inline constexpr uint32_t FLINT_SPI_FREQ_READ = {int(display.get('freq_read_hz', 16000000))}u;",
        "",
        f"inline constexpr int FLINT_FB_MAX_W = {int(canvas['max_w'])};",
        f"inline constexpr int FLINT_FB_MAX_H = {int(canvas['max_h'])};",
        # UI design space = post-rotation canvas; safe inset from board (default 16).
        f"inline constexpr int FLINT_DESIGN_W = {int(canvas['max_w'])};",
        f"inline constexpr int FLINT_DESIGN_H = {int(canvas['max_h'])};",
        f"inline constexpr int FLINT_SAFE_INSET = {int(board.get('safe_inset', 16))};",
        "",
    ]
    OUT_INC.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_impl_include(display: dict) -> None:
    """Point LgfxBoard.h at board/impl/<id>.h — no edits outside board/."""
    impl = display["impl"]
    lines = [
        "// Auto-generated by tools/gen_board_catalog.py — do not edit by hand.",
        f'// Display impl: {impl}',
        f'#include "board/impl/{impl}.h"',
        "",
    ]
    OUT_IMPL.write_text("\n".join(lines), encoding="utf-8")


def write_cmake(board: dict, display: dict) -> None:
    impl = display["impl"]
    reqs = display.get("idf_requires") or []
    reqs_cmake = ";".join(str(r) for r in reqs)
    lgfx_sources = resolve_lgfx_sources(display)
    srcs_cmake = ";".join(lgfx_sources)
    lines = [
        "# Auto-generated by tools/gen_board_catalog.py — do not edit by hand.",
        f'set(FLINT_BOARD_ID "{board["id"]}")',
        f'set(FLINT_BOARD_IDF_TARGET "{board["idf_target"]}")',
        f'set(FLINT_BOARD_DISPLAY_ID "{display["id"]}")',
        f'set(FLINT_BOARD_DISPLAY_BUS "{display.get("bus", "")}")',
        f'set(FLINT_BOARD_DISPLAY_IMPL "{impl}")',
        f'set(FLINT_BOARD_CANVAS_STRATEGY "{board["canvas"]["strategy"]}")',
        f'set(FLINT_BOARD_IDF_REQUIRES "{reqs_cmake}")',
        f'set(FLINT_BOARD_LGFX_SOURCES "{srcs_cmake}")',
        "",
    ]
    OUT_CMAKE.write_text("\n".join(lines), encoding="utf-8")


def cmd_list_boards(*, as_json: bool) -> int:
    """Print boards from board_index.json (no codegen — safe for humans)."""
    index = load_index()
    default_id = str(index.get("default") or "")
    rows: list[dict] = []
    for entry in index.get("boards") or []:
        bid = str(entry.get("id") or "")
        name = str(entry.get("name") or "")
        path = board_json_path(entry, bid)
        idf_target = ""
        display = ""
        note = ""
        if not path.is_file():
            note = f"missing {path.relative_to(ROOT)}"
        else:
            try:
                board = load_json(path)
                idf_target = str(board.get("idf_target") or "")
                display = str(board.get("display") or "")
                if board.get("id") and board.get("id") != bid:
                    note = f"id mismatch in file ({board.get('id')})"
            except (OSError, json.JSONDecodeError) as e:
                note = f"invalid JSON: {e}"
        rows.append(
            {
                "id": bid,
                "name": name,
                "default": bid == default_id,
                "idf_target": idf_target,
                "display": display,
                "file": str(path.relative_to(ROOT)),
                "note": note,
            }
        )

    if as_json:
        print(json.dumps({"default": default_id, "boards": rows}, indent=2, ensure_ascii=False))
        return 0

    if not rows:
        print("no boards in board_index.json", file=sys.stderr)
        return 1

    # Column widths
    w_id = max(len("ID"), max(len(r["id"]) for r in rows))
    w_tgt = max(len("TARGET"), max(len(r["idf_target"]) for r in rows))
    w_disp = max(len("DISPLAY"), max(len(r["display"]) for r in rows))
    print(f"{'':1}  {'ID':<{w_id}}  {'TARGET':<{w_tgt}}  {'DISPLAY':<{w_disp}}  NAME")
    print(f"{'-':1}  {'-' * w_id}  {'-' * w_tgt}  {'-' * w_disp}  ----")
    for r in rows:
        mark = "*" if r["default"] else " "
        line = (
            f"{mark}  {r['id']:<{w_id}}  {r['idf_target']:<{w_tgt}}  "
            f"{r['display']:<{w_disp}}  {r['name']}"
        )
        if r["note"]:
            line += f"  ! {r['note']}"
        print(line)
    print()
    print(f"default: {default_id or '(none)'}")
    print("build:   python3 tools/gen_board_catalog.py --board <id> --build")
    print("   or:   idf.py -DFLINT_BOARD=<id> build  (after matching set-target)")
    return 0


def cmd_set_default(board_id: str | None) -> int:
    """Update board_index.json default (interactive menu if board_id is empty)."""
    index = load_index()
    entries = index_board_entries(index)
    if not entries:
        print("no boards in board_index.json", file=sys.stderr)
        return 1

    known = {e["id"]: e for e in entries}
    current = str(index.get("default") or "").strip()

    if board_id:
        chosen = board_id.strip()
        if chosen not in known:
            ids = ", ".join(e["id"] for e in entries)
            raise SystemExit(f"unknown board {chosen!r}; available: {ids}")
    else:
        if not sys.stdin.isatty():
            raise SystemExit(
                "--set-default needs a TTY for the menu, or pass an id: "
                "--set-default <board_id>"
            )
        print("Select default board:")
        for i, e in enumerate(entries, start=1):
            mark = " (current)" if e["id"] == current else ""
            print(f"  {i}. {e['name']}  [{e['id']}]{mark}")
        print("  0. cancel")
        while True:
            try:
                raw = input("Choice [number]: ").strip()
            except EOFError:
                print()
                print("cancelled")
                return 0
            if raw in ("", "0", "q", "Q"):
                print("cancelled")
                return 0
            if raw.isdigit():
                n = int(raw)
                if 1 <= n <= len(entries):
                    chosen = entries[n - 1]["id"]
                    break
            # Allow typing the board id directly
            if raw in known:
                chosen = raw
                break
            print(f"  invalid choice {raw!r}; enter 1–{len(entries)}, id, or 0 to cancel")

    if chosen == current:
        print(f"default already {chosen} ({known[chosen]['name']})")
        return 0

    index["default"] = chosen
    save_index(index)
    print(f"default: {current or '(none)'} → {chosen} ({known[chosen]['name']})")
    print(f"wrote { (BOARD_DIR / 'board_index.json').relative_to(ROOT) }")
    return 0


def current_idf_target() -> str | None:
    """Best-effort current chip from sdkconfig (None if not configured yet)."""
    sdk = ROOT / "sdkconfig"
    if not sdk.is_file():
        return None
    for line in sdk.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("CONFIG_IDF_TARGET="):
            return line.split("=", 1)[1].strip().strip('"')
    return None


def _espressif_idf_candidates() -> list[Path]:
    """Typical EIM / Espressif installer layouts under ~/.espressif."""
    home = Path.home()
    roots = [
        home / ".espressif",
        home / "esp",
        Path(os.environ["XDG_DATA_HOME"]) / "espressif"
        if os.environ.get("XDG_DATA_HOME")
        else None,
    ]
    found: list[Path] = []
    for root in roots:
        if root is None or not root.is_dir():
            continue
        # ~/.espressif/v6.0.2/esp-idf
        for idf_py in sorted(root.glob("v*/esp-idf/tools/idf.py"), reverse=True):
            found.append(idf_py.parent.parent)
        # ~/esp/esp-idf or ~/.espressif/esp-idf
        direct = root / "esp-idf" / "tools" / "idf.py"
        if direct.is_file():
            found.append(direct.parent.parent)
    # de-dupe preserving order
    seen: set[Path] = set()
    out: list[Path] = []
    for p in found:
        rp = p.resolve()
        if rp not in seen:
            seen.add(rp)
            out.append(rp)
    return out


def discover_idf() -> tuple[Path, Path]:
    """Return (IDF_PATH, idf.py). Uses PATH, IDF_PATH, then ~/.espressif installs."""
    which = shutil.which("idf.py")
    if which:
        idf_py = Path(which).resolve()
        env_path = os.environ.get("IDF_PATH")
        if env_path:
            return Path(env_path).resolve(), idf_py
        # idf.py lives at $IDF_PATH/tools/idf.py
        if idf_py.parent.name == "tools":
            return idf_py.parent.parent, idf_py
        return idf_py.parent, idf_py

    env_path = os.environ.get("IDF_PATH") or os.environ.get("FLINT_IDF_PATH")
    if env_path:
        root = Path(env_path).expanduser().resolve()
        idf_py = root / "tools" / "idf.py"
        if idf_py.is_file():
            return root, idf_py

    for root in _espressif_idf_candidates():
        idf_py = root / "tools" / "idf.py"
        if idf_py.is_file():
            return root, idf_py

    raise SystemExit(
        "idf.py not found.\n"
        "  • Activate ESP-IDF:  source $IDF_PATH/export.sh\n"
        "  • Or set IDF_PATH / FLINT_IDF_PATH to your esp-idf tree\n"
        "  • Expected layout: ~/.espressif/v*/esp-idf/tools/idf.py"
    )


def _idf_version_tag(idf_path: Path) -> str:
    """Best-effort IDF major.minor for python_env dir name (e.g. 6.0)."""
    version_file = idf_path / "tools" / "cmake" / "version.cmake"
    if version_file.is_file():
        text = version_file.read_text(encoding="utf-8", errors="replace")
        major = minor = None
        for line in text.splitlines():
            parts = line.replace("(", " ").replace(")", " ").split()
            upper = [p.upper() for p in parts]
            if "IDF_VERSION_MAJOR" in upper:
                i = upper.index("IDF_VERSION_MAJOR")
                if i + 1 < len(parts):
                    major = parts[i + 1]
            if "IDF_VERSION_MINOR" in upper:
                i = upper.index("IDF_VERSION_MINOR")
                if i + 1 < len(parts):
                    minor = parts[i + 1]
        if major is not None and minor is not None:
            return f"{major}.{minor}"
    for part in idf_path.parts:
        if re.fullmatch(r"v?\d+\.\d+(\.\d+)?", part):
            ver = part.lstrip("v")
            bits = ver.split(".")
            return f"{bits[0]}.{bits[1]}" if len(bits) >= 2 else ver
    return "5.0"


def idf_python_env_dir(idf_path: Path) -> Path:
    """Path Espressif expects: ~/.espressif/python_env/idfX.Y_pyA.B_env."""
    override = os.environ.get("IDF_PYTHON_ENV_PATH")
    if override:
        return Path(override).expanduser()
    tools_path = Path(
        os.environ.get("IDF_TOOLS_PATH", str(Path.home() / ".espressif"))
    ).expanduser()
    py = f"{sys.version_info.major}.{sys.version_info.minor}"
    ver = _idf_version_tag(idf_path)
    return tools_path / "python_env" / f"idf{ver}_py{py}_env"


def ensure_idf_python_env(idf_path: Path) -> Path:
    """Return venv python; create env via idf_tools.py if missing."""
    env_dir = idf_python_env_dir(idf_path)
    venv_python = env_dir / "bin" / "python"
    if venv_python.is_file():
        return venv_python

    tools_py = idf_path / "tools" / "idf_tools.py"
    if not tools_py.is_file():
        raise SystemExit(
            f"ESP-IDF Python env missing ({venv_python}) and idf_tools.py not found.\n"
            f"  Run:  python3 {idf_path}/install.sh"
        )

    print(f"ESP-IDF Python env missing — installing into {env_dir}", flush=True)
    # Hosts often have only `python3` on PATH (no `python`).
    cmd = [sys.executable, str(tools_py), "install-python-env"]
    print("+", " ".join(cmd), flush=True)
    result = subprocess.run(
        cmd, cwd=str(idf_path), env={**os.environ, "IDF_PATH": str(idf_path)}
    )
    if result.returncode != 0 or not venv_python.is_file():
        raise SystemExit(
            "Failed to create ESP-IDF Python env.\n"
            f"  Manual:  python3 {tools_py} install-python-env\n"
            f"  Or:      cd {idf_path} && ./install.sh"
        )
    return venv_python


def run_idf(args: list[str]) -> None:
    """Run idf.py with ESP-IDF env (works without a prior export.sh)."""
    import shlex

    idf_path, idf_py = discover_idf()
    venv_python = ensure_idf_python_env(idf_path)

    env = os.environ.copy()
    env["IDF_PATH"] = str(idf_path)
    env["IDF_PYTHON_ENV_PATH"] = str(venv_python.parent.parent)
    # Prefer venv binaries (provides `python` even when host only has python3).
    env["PATH"] = f"{venv_python.parent}:{env.get('PATH', '')}"

    export_sh = idf_path / "export.sh"
    # Always invoke idf.py with the venv interpreter — avoids `env: python: No such file`.
    idf_cmd = [str(venv_python), str(idf_py), *args]

    if export_sh.is_file() and not (
        shutil.which("idf.py") and os.environ.get("IDF_PATH")
    ):
        inner = " ".join(shlex.quote(x) for x in idf_cmd)
        bash_cmd = f"source {shlex.quote(str(export_sh))} >/dev/null && {inner}"
        print(f"+ IDF_PATH={idf_path} (via export.sh + venv)", flush=True)
        print("+", inner, flush=True)
        result = subprocess.run(["bash", "-lc", bash_cmd], cwd=ROOT, env=env)
    else:
        print(f"+ IDF_PATH={idf_path}", flush=True)
        print("+", " ".join(idf_cmd), flush=True)
        result = subprocess.run(idf_cmd, cwd=ROOT, env=env)

    if result.returncode != 0:
        raise SystemExit(result.returncode)


def ensure_idf_target(wanted: str, *, force: bool) -> None:
    current = current_idf_target()
    if not force and current == wanted:
        print(f"idf target already {wanted}", flush=True)
        return
    if current and current != wanted:
        print(f"idf target {current} → {wanted}", flush=True)
    run_idf(["set-target", wanted])


def cmd_generate(board_arg: str, *, quiet: bool) -> tuple[str, dict]:
    """Validate + write generated artifacts (CMake path). Returns (board_id, board)."""
    index = load_index()
    board_id = resolve_board_id(board_arg, index)
    entry = find_board_entry(index, board_id)
    board = load_json(board_json_path(entry, board_id))

    display_id = board.get("display")
    if not display_id:
        raise SystemExit(f"board {board_id}: missing display")
    display = load_json(BOARD_DIR / "periph" / "display" / f"{display_id}.json")
    if display.get("id") != display_id:
        raise SystemExit(f"display file id {display.get('id')!r} != {display_id!r}")

    if not board.get("idf_target"):
        raise SystemExit(f"board {board_id}: missing idf_target")

    validate(board, display, board_id)
    write_inc(board, display)
    write_impl_include(display)
    write_cmake(board, display)

    # One line for CMake message(STATUS …) — keep stable and parse-friendly.
    status = (
        f"wrote {OUT_INC.relative_to(ROOT)}, {OUT_IMPL.relative_to(ROOT)}, "
        f"{OUT_CMAKE.relative_to(ROOT)} "
        f"(board={board_id}, display={display_id}, impl={display['impl']})"
    )
    if not quiet:
        print(status)
    return board_id, board


def firmware_bin_path() -> Path:
    """ESP-IDF app image for this project (CMake project(flint_os))."""
    return ROOT / "build" / "flint_os.bin"


def cmd_idf_actions(
    board_id: str,
    board: dict,
    *,
    do_build: bool,
    do_flash: bool,
    do_monitor: bool,
    port: str,
    set_target: bool,
) -> int:
    """Run idf.py actions. build/flash/monitor are independent."""
    target = str(board["idf_target"]).strip()
    if not target:
        raise SystemExit(f"board {board_id}: empty idf_target")

    need_target = do_build or do_flash
    if need_target and set_target:
        ensure_idf_target(target, force=False)
    elif need_target and not set_target:
        current = current_idf_target()
        if current and current != target:
            print(
                f"warning: sdkconfig target is {current}, board wants {target} "
                f"(omit --no-set-target to fix)",
                file=sys.stderr,
            )

    # --flash: only build if there is nothing to flash yet.
    if do_flash and not do_build and not firmware_bin_path().is_file():
        print(
            f"no {firmware_bin_path().relative_to(ROOT)} — building once before flash",
            flush=True,
        )
        do_build = True
        if set_target:
            ensure_idf_target(target, force=False)

    idf_args: list[str] = []
    if do_build or do_flash:
        idf_args.append(f"-DFLINT_BOARD={board_id}")
    if port:
        idf_args.extend(["-p", port])

    actions: list[str] = []
    if do_build:
        actions.append("build")
    if do_flash:
        actions.append("flash")
    if do_monitor:
        actions.append("monitor")
    if not actions:
        return 0

    run_idf([*idf_args, *actions])
    return 0


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        prog="gen_board_catalog.py",
        description=PROG_DESCRIPTION,
        epilog=PROG_EPILOG,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument(
        "-b",
        "--board",
        default="",
        metavar="ID",
        help="board id from board_index.json (else FLINT_BOARD, else index default)",
    )
    ap.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="list boards available for -DFLINT_BOARD / --board (no files written)",
    )
    ap.add_argument(
        "--set-default",
        nargs="?",
        const="",
        default=None,
        metavar="ID",
        help="set board_index.json default (interactive numbered menu if ID omitted)",
    )
    ap.add_argument(
        "--json",
        action="store_true",
        help="with --list, print JSON instead of a table",
    )
    ap.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="generate without status line (still writes files; exit 0 on success)",
    )
    ap.add_argument(
        "--build",
        action="store_true",
        help="after generate: idf.py set-target (if needed) + build -DFLINT_BOARD=<id>",
    )
    ap.add_argument(
        "--flash",
        action="store_true",
        help="after generate: idf.py flash only (builds first only if build/*.bin missing)",
    )
    ap.add_argument(
        "--monitor",
        action="store_true",
        help="after generate: idf.py monitor only (does not build or flash)",
    )
    ap.add_argument(
        "-p",
        "--port",
        default="",
        metavar="PORT",
        help="serial port for flash/monitor (passed to idf.py -p)",
    )
    ap.add_argument(
        "--no-set-target",
        action="store_true",
        help="with --build/--flash, do not call idf.py set-target",
    )
    return ap


def main(argv: list[str] | None = None) -> int:
    ap = build_parser()
    args = ap.parse_args(argv)

    if args.list:
        if args.set_default is not None:
            raise SystemExit("--list cannot be combined with --set-default")
        if args.build or args.flash or args.monitor:
            raise SystemExit("--list cannot be combined with --build/--flash/--monitor")
        return cmd_list_boards(as_json=bool(args.json))

    if args.set_default is not None:
        if args.build or args.flash or args.monitor or args.board or args.quiet:
            raise SystemExit(
                "--set-default cannot be combined with --board/--quiet/--build/--flash/--monitor"
            )
        if args.json:
            raise SystemExit("--json is only valid with --list")
        if args.port:
            raise SystemExit("-p/--port is only used with --build/--flash/--monitor")
        # const="" when flag alone → interactive; otherwise explicit id
        return cmd_set_default(args.set_default or None)

    if args.json:
        raise SystemExit("--json is only valid with --list")

    if args.port and not (args.build or args.flash or args.monitor):
        raise SystemExit("-p/--port is only used with --build/--flash/--monitor")

    board_id, board = cmd_generate(args.board, quiet=bool(args.quiet))

    if args.build or args.flash or args.monitor:
        return cmd_idf_actions(
            board_id,
            board,
            do_build=bool(args.build),
            do_flash=bool(args.flash),
            do_monitor=bool(args.monitor),
            port=str(args.port or ""),
            set_target=not bool(args.no_set_target),
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except FileNotFoundError as e:
        print(f"missing {e}", file=sys.stderr)
        raise SystemExit(1)
    except BrokenPipeError:
        # e.g. `… --list | head`
        raise SystemExit(0)
