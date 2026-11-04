import json
import hashlib
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from compile_ignite import compile_project
from build_system import main as unused


def main():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "factory").mkdir()
        (root / "store").mkdir()
        for path in (ROOT / "system").glob("*.json"):
            if path.stem != "monitor":
                (root / "factory" / f"{path.stem}.ignit").write_bytes(
                    compile_project(
                        json.loads(path.read_text(encoding="utf-8")), path.parent
                    )
                )
        project = json.loads(
            (ROOT / "examples/pc-monitor/project.json").read_text(encoding="utf-8")
        )
        project["id"] = "test.monitor"
        (root / "v1.ignit").write_bytes(compile_project(project, root))
        malformed = bytearray((root / "v1.ignit").read_bytes())
        ui_offset = struct.unpack_from("<I", malformed, 96)[0]
        struct.pack_into(">H", malformed, ui_offset + 4, 320)
        struct.pack_into(">H", malformed, ui_offset + 16, 320)
        malformed[32:64] = bytes(32)
        malformed[32:64] = hashlib.sha256(malformed).digest()
        (root / "bad-canvas.ignit").write_bytes(malformed)
        project["version"] = 2
        (root / "v2.ignit").write_bytes(compile_project(project, root))
        project["id"] = "system.clock"
        project["version"] = 100
        (root / "spoof.ignit").write_bytes(compile_project(project, root))
        project["id"] = "test.monitor"
        project["version"] = 3
        project["components"][0]["ui"][0]["w"] = 279
        try:
            compile_project(project, root)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid UI accepted")
        # User font and image fixtures exercise lazy resource reading and custom rendering.
        from PIL import Image

        image = Image.new("RGB", (16, 16), (240, 40, 20))
        image.save(root / "icon.png")
        font = ROOT / "tests/assets/Lato-Regular.ttf"
        custom = {
            "id": "test.resources",
            "name": "Resources",
            "publisher": "tests",
            "version": 1,
            "capabilities": 3,
            "permissions": ["ui.render", "ui.resources"],
            "components": [
                {
                    "id": "main",
                    "type": "app",
                    "ui": [
                        {"kind": "rect", "w": 280, "h": 240, "color": "#000000"},
                        {
                            "kind": "text",
                            "x": 50,
                            "y": 80,
                            "font": 4,
                            "text": "Test",
                            "color": "#ffffff",
                        },
                        {
                            "kind": "image",
                            "x": 4,
                            "y": 4,
                            "w": 16,
                            "h": 16,
                            "resource": 5,
                        },
                    ],
                }
            ],
            "resources": [
                {
                    "id": 4,
                    "path": "font.ttf",
                    "format": "font",
                    "size": 20,
                    "characters": "Test",
                },
                {"id": 5, "path": "icon.png", "format": "image"},
            ],
        }
        (root / "font.ttf").write_bytes(font.read_bytes())
        (root / "resources.ignit").write_bytes(compile_project(custom, root))
        subprocess.run([sys.argv[1], str(root)], check=True)


if __name__ == "__main__":
    main()
