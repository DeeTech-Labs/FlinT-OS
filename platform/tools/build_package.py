#!/usr/bin/env python3
"""Build an indexed v1 Ignite container. JSON sources never execute on the watch."""

import argparse
import hashlib
import json
import struct
from pathlib import Path

REVISION = 0x20261104
TYPES = {
    "manifest": 1,
    "component": 2,
    "resource": 3,
    "ui": 4,
    "wasm": 5,
    "locale": 6,
    "signature": 7,
}


def build(sections):
    sections = sorted(sections, key=lambda e: e[0])
    if (
        not 1 <= len(sections) <= 1024
        or sum(t == "manifest" for _, t, _ in sections) != 1
    ):
        raise ValueError("exactly one manifest and 1..1024 sections required")
    offset = 64 + 24 * len(sections)
    entries, payload, previous = bytearray(), bytearray(), 0
    for ident, kind, data in sections:
        if not previous < ident <= 0xFFFFFFFF or not data or kind not in TYPES:
            raise ValueError(
                "unique positive IDs, known types and nonempty sections required"
            )
        if offset + len(data) > 0xFFFFFFFF:
            raise ValueError("package too large")
        entries.extend(
            struct.pack("<IHHIIII", ident, TYPES[kind], 0, offset, len(data), 0, 0)
        )
        payload.extend(data)
        offset += len(data)
        previous = ident
    header = struct.pack(
        "<8sHHIIIII32s",
        b"FLIGNIT\0",
        1,
        64,
        REVISION,
        offset,
        len(sections),
        64,
        0,
        bytes(32),
    )
    result = bytearray(header + entries + payload)
    result[32:64] = hashlib.sha256(result).digest()
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "project", type=Path, help="JSON section list: [{id,type,path}]"
    )
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = args.project.resolve().parent
    sections = []
    for entry in json.loads(args.project.read_text(encoding="utf-8")):
        path = (root / entry["path"]).resolve()
        if not path.is_relative_to(root):
            raise ValueError("section path escapes project")
        sections.append((entry["id"], entry["type"], path.read_bytes()))
    result = build(sections)
    args.output.write_bytes(result)


if __name__ == "__main__":
    main()
