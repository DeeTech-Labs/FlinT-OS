#!/usr/bin/env python3
"""Compile an Ignite JSON project, UI and optional font/image assets into v1 binary."""

import argparse
import json
import re
import struct
from pathlib import Path
from build_package import build

COMPONENTS = {
    "app": 1,
    "watchface": 2,
    "theme": 3,
    "locale": 4,
    "resource": 5,
    "service": 6,
    "widget": 7,
    "complication": 8,
}
ROLES = {"none": 0, "clock": 1, "launcher": 2, "settings": 3, "theme": 4, "locale": 5}


def text(value):
    value = value.encode("utf-8")
    if len(value) > 65535 or b"\0" in value:
        raise ValueError("invalid string")
    return struct.pack("<H", len(value)) + value


def identifier(value):
    if (
        not re.fullmatch(r"[a-z0-9_-][a-z0-9._-]{0,63}", value)
        or ".." in value
        or value.endswith(".")
    ):
        raise ValueError("invalid identifier")
    return value


def color(value):
    if value in ("theme.background", "theme.text", "theme.accent"):
        return {
            "theme.background": 0xFFFD,
            "theme.text": 0xFFFC,
            "theme.accent": 0xFFFE,
        }[value]
    n = int(value.lstrip("#"), 16)
    return ((n >> 19) << 11) | (((n >> 10) & 63) << 5) | ((n >> 3) & 31)


def scene(nodes):
    ops, strings = [], bytearray()
    for node in nodes:
        kind = {"rect": 1, "rounded": 2, "line": 3, "text": 4, "bitmap": 5, "image": 6}[
            node["kind"]
        ]
        data = (
            node.get("text", "").encode()
            if kind == 4
            else bytes.fromhex(node.get("bits", ""))
        )
        x, y = node.get("x", 0), node.get("y", 0)
        w, h = node.get("w", 0), node.get("h", 0)
        extra = node.get("resource", 0) if kind == 6 else node.get("extra", 0)
        if kind == 4:
            extra = int(node.get("center", False))
        if kind == 5:
            extra = node.get("scale", 1)
            w = h = 16 * extra
        if kind == 4 and len(data) > 192:
            raise ValueError("text too long")
        ops.append(
            struct.pack(
                ">BBhhhhHHHH",
                kind,
                node.get("font", 0),
                x,
                y,
                w,
                h,
                color(node.get("color", "#ffffff")),
                len(strings),
                len(data),
                extra,
            )
        )
        strings.extend(data)
    if not 1 <= len(ops) <= 32 or len(strings) > 512:
        raise ValueError("UI budget exceeded")
    if (
        nodes[0]["kind"] != "rect"
        or nodes[0].get("x", 0)
        or nodes[0].get("y", 0)
        or nodes[0].get("w") != 280
        or nodes[0].get("h") != 240
    ):
        raise ValueError("first node must cover the screen")
    return (
        struct.pack(">2sBBHHH", b"FS", 1, len(ops), 280, 240, len(strings))
        + b"".join(ops)
        + strings
    )


def atlas(path, size, chars):
    from PIL import Image, ImageDraw, ImageFont

    font = ImageFont.truetype(str(path), size)
    records = []
    payload = bytearray()
    chars = sorted(set(chars + "?"))
    offset = 12 + 24 * len(chars)
    for char in chars:
        left, top, right, bottom = font.getbbox(char, anchor="ls")
        w, h = right - left, bottom - top
        if w > 64 or h > 64:
            raise ValueError("glyph exceeds scratch budget")
        image = Image.new("L", (max(1, w), max(1, h)))
        ImageDraw.Draw(image).text(
            (-left, -top), char, font=font, fill=255, anchor="ls"
        )
        pixels = image.tobytes()[: w * h]
        records.append(
            struct.pack(
                "<IHHhhHHII",
                ord(char),
                w,
                h,
                left,
                -top,
                round(font.getlength(char)),
                0,
                offset,
                len(pixels),
            )
        )
        payload.extend(pixels)
        offset += len(pixels)
    ascent, descent = font.getmetrics()
    return (
        struct.pack("<4sHHHH", b"FLGF", len(chars), ascent, descent, 0)
        + b"".join(records)
        + payload
    )


def compile_project(project, root):
    identifier(project["id"])
    identifier(project["publisher"])
    budget = project.get("budget", {})
    permissions = project.get("permissions", [])
    components = project["components"]
    if len(permissions) > 64 or not 1 <= len(components) <= 32:
        raise ValueError("manifest budget exceeded")
    name = project["name"]
    if not name or len(name.encode()) > 96:
        raise ValueError("name too long")
    manifest = bytearray(
        b"FLM1" + text(project["id"]) + text(name) + text(project["publisher"])
    )
    manifest.extend(
        struct.pack(
            "<IIIIQ",
            project.get("version", 1),
            budget.get("internal", 24 * 1024),
            budget.get("external", 0),
            budget.get("instructions", 4096),
            project.get("capabilities", 1),
        )
    )
    manifest.extend(struct.pack("<H", len(permissions)))
    for permission in permissions:
        if isinstance(permission, str):
            permission = {"id": permission, "scope": ""}
        manifest.extend(text(permission["id"]) + text(permission.get("scope", "")))
    manifest.extend(struct.pack("<H", len(components)))
    sections = []
    used = {1}
    for index, component in enumerate(components):
        section = component.get("section", index + 2)
        if section in used:
            raise ValueError("duplicate section")
        used.add(section)
        manifest.extend(
            text(identifier(component["id"]))
            + struct.pack(
                "<HBI",
                COMPONENTS[component["type"]],
                ROLES[component.get("role", "none")],
                section,
            )
        )
        if "ui" in component:
            kind = "ui"
            data = scene(component["ui"])
        elif "values" in component:
            kind = "locale" if component["type"] == "locale" else "component"
            values = component["values"]
            data = struct.pack("<H", len(values)) + b"".join(
                text(k) + text(v) for k, v in sorted(values.items())
            )
        else:
            raise ValueError("component requires declarative UI or values")
        sections.append((section, kind, data))
    for resource in project.get("resources", []):
        section = resource["id"]
        if section in used:
            raise ValueError("duplicate resource section")
        used.add(section)
        path = (root / resource["path"]).resolve()
        if not path.is_relative_to(root.resolve()):
            raise ValueError("resource escapes project")
        if resource.get("format") == "font":
            data = atlas(path, resource.get("size", 20), resource["characters"])
        elif resource.get("format") == "image":
            from PIL import Image

            image = Image.open(path).convert("RGB")
            w, h = image.size
            if w > 320 or h > 320:
                raise ValueError("image too large")
            data = struct.pack("<4sHH", b"FLIM", w, h) + b"".join(
                struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
                for r, g, b in image.getdata()
            )
        else:
            data = path.read_bytes()
        sections.append((section, "resource", data))
    return build([(1, "manifest", bytes(manifest)), *sections])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_bytes(
        compile_project(
            json.loads(args.project.read_text(encoding="utf-8")),
            args.project.resolve().parent,
        )
    )


if __name__ == "__main__":
    main()
