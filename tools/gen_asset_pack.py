#!/usr/bin/env python3
"""Build LittleFS pack from assets/<category>/<FlintIconId>_<size>.png → assets/fs_root/core.flintpack."""

from __future__ import annotations

import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

PROG_DESCRIPTION = "Build LittleFS pack from assets/<category>/*.png"

FI_MAGIC = 0x00314946
FLPK_MAGIC = 0x4B504C46
FI_L1 = 0

ROOT = Path(__file__).resolve().parents[1]
RESOURCES_H = ROOT / "components" / "flint" / "sdk" / "resources.h"
VERSION_H = ROOT / "components" / "flint" / "config" / "flint_version.h"
NAME_RE = re.compile(r"^([A-Za-z][A-Za-z0-9]*)_(\d+)\.png$")
SKIP_DIRS = frozenset({"fs_root"})
ENUM_RE = re.compile(
    r"enum\s+class\s+FlintIconId\s*:\s*uint8_t\s*\{([^}]*)\}",
    re.MULTILINE,
)
ENUM_ITEM_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*(\d+))?")
DEFINE_INT_RE = re.compile(r"^#define\s+(FLINT_\w+)\s+(\d+)\s*(?://.*)?$", re.MULTILINE)


def load_pack_versions(path: Path = VERSION_H) -> tuple[int, int, int]:
    """Parse pack versions from flint_version.h — single source with firmware."""
    text = path.read_text(encoding="utf-8")
    vals = {m.group(1): int(m.group(2)) for m in DEFINE_INT_RE.finditer(text)}
    try:
        return (
            vals["FLINT_PACK_FORMAT_VER"],
            vals["FLINT_PACK_API_MIN"],
            vals["FLINT_PACK_VER"],
        )
    except KeyError as exc:
        raise SystemExit(f"{path}: missing {exc.args[0]}") from exc


def load_icon_ids(path: Path = RESOURCES_H) -> dict[str, int]:
    """Parse FlintIconId from resources.h — single source of truth with firmware."""
    text = path.read_text(encoding="utf-8")
    m = ENUM_RE.search(text)
    if not m:
        raise SystemExit(f"{path}: FlintIconId enum not found")
    ids: dict[str, int] = {}
    next_id = 0
    for line in m.group(1).splitlines():
        line = line.split("//", 1)[0].strip().rstrip(",")
        if not line:
            continue
        item = ENUM_ITEM_RE.fullmatch(line)
        if not item:
            raise SystemExit(f"{path}: bad FlintIconId entry {line!r}")
        name, explicit = item.group(1), item.group(2)
        value = int(explicit) if explicit is not None else next_id
        ids[name] = value
        next_id = value + 1
    if not ids:
        raise SystemExit(f"{path}: empty FlintIconId enum")
    return ids


def read_rgba_png(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos = 8
    w = h = color_type = None
    idat = bytearray()
    while pos + 8 <= len(data):
        (length,) = struct.unpack_from(">I", data, pos)
        tag = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if tag == b"IHDR":
            w, h, bit_depth, color_type, *_ = struct.unpack(">IIBBBBB", chunk)
            if bit_depth != 8 or color_type not in (2, 6):
                raise ValueError(f"{path}: need 8-bit RGB/RGBA, filter None")
        elif tag == b"IDAT":
            idat.extend(chunk)
        elif tag == b"IEND":
            break
    if w is None or h is None or color_type is None:
        raise ValueError(f"{path}: bad PNG")
    raw = zlib.decompress(bytes(idat))
    bpp = 4 if color_type == 6 else 3
    if len(raw) != (1 + w * bpp) * h:
        raise ValueError(f"{path}: bad IDAT")
    rgba = bytearray(w * h * 4)
    src = dst = 0
    for _ in range(h):
        if raw[src] != 0:
            raise ValueError(f"{path}: export PNG with filter None")
        src += 1
        row = raw[src : src + w * bpp]
        src += w * bpp
        if bpp == 4:
            rgba[dst : dst + w * 4] = row
            dst += w * 4
        else:
            for i in range(w):
                rgba[dst : dst + 3] = row[i * 3 : i * 3 + 3]
                rgba[dst + 3] = 255
                dst += 4
    return w, h, bytes(rgba)


def rgba_to_l1(w: int, h: int, rgba: bytes) -> bytes:
    stride = (w + 7) // 8
    out = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            if rgba[(y * w + x) * 4 + 3] < 128:
                continue
            out[y * stride + (x // 8)] |= 0x80 >> (x & 7)
    return bytes(out)


def encode_fi_l1(w: int, h: int, pixels: bytes) -> bytes:
    return struct.pack("<IHHBBHI", FI_MAGIC, w, h, FI_L1, 0, 0, len(pixels)) + pixels


def iter_pngs(assets_root: Path) -> list[Path]:
    paths: list[Path] = []
    for child in sorted(assets_root.iterdir()):
        if not child.is_dir() or child.name in SKIP_DIRS or child.name.startswith("."):
            continue
        paths.extend(sorted(child.glob("*.png")))
    return paths


def collect_icons(
    assets_root: Path, icon_ids: dict[str, int]
) -> list[tuple[str, int, int, bytes]]:
    if not assets_root.is_dir():
        raise SystemExit(f"missing {assets_root}")
    blobs: list[tuple[str, int, int, bytes]] = []
    seen: set[str] = set()
    for path in iter_pngs(assets_root):
        m = NAME_RE.match(path.name)
        if not m:
            raise SystemExit(
                f"{path.relative_to(assets_root)}: rename to <FlintIconId>_<size>.png "
                f"(e.g. icons/Wifi3_16.png)"
            )
        name, size_s = m.group(1), int(m.group(2))
        if name not in icon_ids:
            known = ", ".join(icon_ids)
            raise SystemExit(f"{path.name}: unknown id {name!r}; expected one of: {known}")
        if name in seen:
            raise SystemExit(f"duplicate icon id {name!r} ({path})")
        seen.add(name)
        w, h, rgba = read_rgba_png(path)
        if w != size_s or h != size_s or w != h:
            raise SystemExit(f"{path.name}: pixels {w}x{h} must be {size_s}x{size_s}")
        blobs.append((name, icon_ids[name], size_s, encode_fi_l1(w, h, rgba_to_l1(w, h, rgba))))
    if not blobs:
        raise SystemExit(f"no PNGs under {assets_root}/<category>/")
    missing = [n for n in icon_ids if n not in seen]
    if missing:
        raise SystemExit(f"missing icons: {', '.join(missing)}")
    return blobs


def build_pack(assets_root: Path, out_dir: Path) -> None:
    pack_fmt, api_min, pack_ver = load_pack_versions()
    icon_ids = load_icon_ids()
    blobs = collect_icons(assets_root, icon_ids)
    out_dir.mkdir(parents=True, exist_ok=True)
    offset = 16 + 12 * len(blobs)
    entries = []
    payloads = bytearray()
    for _name, icon_id, size_px, fi in blobs:
        entries.append(struct.pack("<HBBII", icon_id, size_px, FI_L1, offset, len(fi)))
        payloads.extend(fi)
        offset += len(fi)
    pack = (
        struct.pack(
            "<IHHHHI",
            FLPK_MAGIC,
            pack_fmt,
            api_min,
            pack_ver,
            len(blobs),
            0,
        )
        + b"".join(entries)
        + payloads
    )
    out = out_dir / "core.flintpack"
    out.write_bytes(pack)
    print(
        f"wrote {out} ({len(pack)} bytes, {len(blobs)} icons, "
        f"fmt={pack_fmt} api_min={api_min} pack_ver={pack_ver})"
    )


def main() -> int:
    ap = argparse.ArgumentParser(description=PROG_DESCRIPTION)
    ap.add_argument(
        "-a",
        "--assets",
        type=Path,
        default=Path("assets"),
        help="Assets root with category folders (default: assets)",
    )
    ap.add_argument(
        "-o",
        "--out",
        type=Path,
        default=Path("assets/fs_root"),
        help="LittleFS root (default: assets/fs_root)",
    )
    args = ap.parse_args()
    assets = args.assets if args.assets.is_absolute() else ROOT / args.assets
    out = args.out if args.out.is_absolute() else ROOT / args.out
    build_pack(assets, out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
