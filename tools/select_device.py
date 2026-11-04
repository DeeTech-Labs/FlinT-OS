#!/usr/bin/env python3
"""Select a supported FlinT product revision and run ESP-IDF actions.

Hardware is maintained as fixed C++/CMake files under board/models/.
Only supported model/revision IDs are accepted.
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
DEVICE_DIR = ROOT / "components" / "flint" / "board"
LGFX_SRC = ROOT / "third_party" / "LovyanGFX" / "src" / "lgfx" / "v1"
OUT_INC = DEVICE_DIR / "device_config.h"
OUT_IMPL = DEVICE_DIR / "device_display.h"
OUT_CMAKE = DEVICE_DIR / "device.cmake"

# Supported product revisions. No external hardware descriptions are loaded.
PROFILES = {
    "flint_spark_s3_supermini": {
        "name": "FlinT Spark / S3 Super Mini development",
        "model": "flint_spark",
        "revision": "s3_supermini",
        "idf_target": "esp32s3",
        "display": "st7789_spi_240x280",
        "directory": "models/flint_spark/s3_supermini",
    },
}
DEFAULT_PROFILE = "flint_spark_s3_supermini"
PROG_DESCRIPTION = "Select a fixed FlinT model/revision and build firmware"
PROG_EPILOG = "Example: %(prog)s --device flint_spark_s3_supermini --build"


def resolve_device_id(requested: str) -> str:
    selected = (requested or os.environ.get("FLINT_DEVICE") or DEFAULT_PROFILE).strip()
    if selected not in PROFILES:
        raise SystemExit(
            f"unsupported product revision {selected!r}; available: {', '.join(PROFILES)}"
        )
    return selected


def cmd_list_devices(*, as_json: bool) -> int:
    rows = [
        {"id": key, "default": key == DEFAULT_PROFILE, **value}
        for key, value in PROFILES.items()
    ]
    if as_json:
        print(json.dumps({"default": DEFAULT_PROFILE, "devices": rows}, indent=2))
    else:
        for row in rows:
            print(f"{row['id']}: {row['name']} ({row['idf_target']})")
    return 0


def cmd_generate(device_arg: str, *, quiet: bool) -> tuple[str, dict]:
    selected = resolve_device_id(device_arg)
    profile = PROFILES[selected]
    source = DEVICE_DIR / profile["directory"]
    # Select complete, reviewed product hardware.
    for output in (OUT_INC, OUT_IMPL, OUT_CMAKE):
        content = (source / output.name).read_bytes()
        if not output.exists() or output.read_bytes() != content:
            output.write_bytes(content)
    if not quiet:
        print(f"{profile['name']} ({selected}, {profile['idf_target']})")
    return selected, dict(profile)


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
        (
            Path(os.environ["XDG_DATA_HOME"]) / "espressif"
            if os.environ.get("XDG_DATA_HOME")
            else None
        ),
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


def firmware_bin_path() -> Path:
    """ESP-IDF app image for this project (CMake project(flint_os))."""
    return ROOT / "build" / "flint_os.bin"


def cmd_idf_actions(
    device_id: str,
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
        raise SystemExit(f"board {device_id}: empty idf_target")

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
        idf_args.append(f"-DFLINT_DEVICE={device_id}")
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
        prog="select_device.py",
        description=PROG_DESCRIPTION,
        epilog=PROG_EPILOG,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument(
        "-d",
        "--device",
        default="",
        metavar="ID",
        help="supported model/revision (else FLINT_DEVICE, else release default)",
    )
    ap.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="list product revisions available for -DFLINT_DEVICE / --device (no files written)",
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
        help="after generate: idf.py set-target (if needed) + build -DFLINT_DEVICE=<id>",
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
        if args.build or args.flash or args.monitor:
            raise SystemExit("--list cannot be combined with --build/--flash/--monitor")
        return cmd_list_devices(as_json=bool(args.json))

    if args.json:
        raise SystemExit("--json is only valid with --list")

    if args.port and not (args.build or args.flash or args.monitor):
        raise SystemExit("-p/--port is only used with --build/--flash/--monitor")

    device_id, board = cmd_generate(args.device, quiet=bool(args.quiet))

    if args.build or args.flash or args.monitor:
        return cmd_idf_actions(
            device_id,
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
