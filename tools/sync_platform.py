#!/usr/bin/env python3
"""Export the canonical portable runtime into the independently buildable Flare repo."""

import argparse
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "platform"
TARGET = ROOT.parent / "flint_flare/platform"


def inventory(root):
    return {
        p.relative_to(root)
        for p in root.rglob("*")
        if p.is_file() and "__pycache__" not in p.parts and p.suffix != ".pyc"
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected, actual = inventory(SOURCE), inventory(TARGET)
    if args.check:
        if expected != actual or any(
            (SOURCE / p).read_bytes() != (TARGET / p).read_bytes() for p in expected
        ):
            raise SystemExit("Platform export differs; run tools/sync_platform.py")
    else:
        for p in actual - expected:
            (TARGET / p).unlink()
        for p in expected:
            out = TARGET / p
            out.parent.mkdir(parents=True, exist_ok=True)
            if not out.exists() or out.read_bytes() != (SOURCE / p).read_bytes():
                shutil.copyfile(SOURCE / p, out)


if __name__ == "__main__":
    main()
