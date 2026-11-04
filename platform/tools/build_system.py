#!/usr/bin/env python3
import argparse, json
from pathlib import Path
from compile_ignite import compile_project

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / "factory")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    expected = set()
    for path in sorted((ROOT / "system").glob("*.json")):
        output = args.output / (path.stem + ".ignit")
        expected.add(output.name)
        data = compile_project(
            json.loads(path.read_text(encoding="utf-8")), path.parent
        )
        if not output.exists() or output.read_bytes() != data:
            output.write_bytes(data)
    for project in sorted((ROOT / "examples").glob("*/project.json")):
        output = project.parent / "sample.ignit"
        data = compile_project(
            json.loads(project.read_text(encoding="utf-8")), project.parent
        )
        if not output.exists() or output.read_bytes() != data:
            output.write_bytes(data)
    for path in args.output.glob("*.ignit"):
        if path.name not in expected:
            path.unlink()


if __name__ == "__main__":
    main()
