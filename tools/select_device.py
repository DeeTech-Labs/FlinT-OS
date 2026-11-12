#!/usr/bin/env python3
"""Select a supported FlinT model and run ESP-IDF actions.

Hardware modules live under src/modules/; model settings under config/devices/.
Generated board headers belong to the selected build directory.
Only supported model IDs are accepted.
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

if sys.version_info < (3, 11):
    raise SystemExit("FlinT OS device configuration requires Python 3.11 or newer")
import tomllib

ROOT = Path(__file__).resolve().parents[1]
MODULE_DIR = ROOT / "src" / "modules"
CONFIG_DIR = ROOT / "config" / "devices"
DEFAULT_BUILD_DIR = ROOT / "build"
DEFAULT_SDKCONFIG = ROOT / "config" / "sdkconfig"
_REGISTRY = json.loads((ROOT / "config" / "devices.json").read_text(encoding="utf-8"))
DEFAULT_PROFILE = _REGISTRY["default"]
PROG_DESCRIPTION = "Select a FlinT model and build firmware"
PROG_EPILOG = "Example: %(prog)s --device flint_spark --build"


def read_toml(path: Path) -> dict:
    try:
        return tomllib.loads(path.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as error:
        raise ValueError(f"{path}: {error}") from error


def discover_profiles(directory: Path) -> dict:
    profiles = {}
    for path in sorted(directory.glob("*/device.toml")):
        model = path.parent.name
        if not re.fullmatch(r"[a-z][a-z0-9_]*", model):
            raise ValueError(f"{path}: model must be a lowercase identifier")
        data = read_toml(path)
        name = data.get("name")
        if not isinstance(name, str) or not name.strip():
            raise ValueError(f"{path}: name: expected a nonempty string")
        profiles[model] = dict(name=name, model=model, idf_target="esp32s3")
    return profiles


try:
    PROFILES = discover_profiles(CONFIG_DIR)
except ValueError as error:
    raise SystemExit(f"device configuration: {error}") from error


def resolve_device_id(requested: str) -> str:
    selected = (requested or os.environ.get("FLINT_DEVICE") or DEFAULT_PROFILE).strip()
    if selected not in PROFILES:
        raise SystemExit(
            f"unsupported model {selected!r}; available: {', '.join(PROFILES)}"
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


def check_table(value, allowed, required, where):
    if not isinstance(value, dict):
        raise ValueError(f"{where}: expected a table")
    unknown = set(value) - set(allowed)
    missing = set(required) - set(value)
    if unknown or missing:
        raise ValueError(f"{where}: unknown keys {sorted(unknown)}; missing keys {sorted(missing)}")
    return value


def check_int(value, minimum, maximum, where):
    if type(value) is not int or not minimum <= value <= maximum:
        raise ValueError(f"{where}: expected integer {minimum}..{maximum}")
    return value


def check_bool(value, where):
    if type(value) is not bool:
        raise ValueError(f"{where}: expected boolean")
    return value


def load_device(settings: Path) -> tuple[dict, dict, list[str], list[str]]:
    path = settings / "device.toml"
    data = check_table(read_toml(path), {"name", "modules"}, {"name"}, str(path))
    if not isinstance(data["name"], str) or not data["name"].strip():
        raise ValueError(f"{path}: name: expected a nonempty string")
    available = {p.name for p in MODULE_DIR.iterdir() if (p / "module.json").is_file()}
    modules = check_table(data.get("modules", {}), available, {}, f"{path}: modules")
    width, height = 280, 240
    roles, normalized, sources, requires, pins_used = {}, {}, [], [], {}
    bus_signals = {}

    def pins(config, names, where):
        result = check_table(config, set(names), set(names), where)
        for name, pin in result.items():
            field = f"{where}.{name}"
            check_int(pin, 0, 48, field)
            if pin in range(22, 26):
                raise ValueError(f"{field}: GPIO{pin} is unavailable on ESP32-S3")
            if pin in pins_used and name in ("sda", "scl") and bus_signals.get(name) == pin:
                continue
            if pin in pins_used:
                raise ValueError(f"{field}: GPIO{pin} already assigned to {pins_used[pin]}")
            pins_used[pin] = field
            if name in ("sda", "scl"):
                if name in bus_signals and bus_signals[name] != pin:
                    raise ValueError(f"{field}: all I2C modules must use one shared bus")
                bus_signals[name] = pin
        return result

    for module, config in modules.items():
        where = f"{path}: modules.{module}"
        directory = MODULE_DIR / module
        manifest = json.loads((directory / "module.json").read_text(encoding="utf-8"))
        kind = manifest["kind"]
        expected_role = {
            "st7789": "display", "cst816": "touch", "qmi8658": "imu", "pcf85063": "rtc",
            "battery_adc": "battery", "pwm_buzzer": "buzzer", "pwm_backlight": "backlight",
            "power_button": "power_button", "bmp280": "bmp280", "bme280": "bme280",
            "veml7700": "light",
        }.get(module)
        if expected_role is None or kind != expected_role:
            raise ValueError(f"{where}: unsupported module or invalid manifest role")
        if kind in roles:
            raise ValueError(f"{where}: {kind} is already provided by {roles[kind]}")
        roles[kind] = module
        if module == "st7789":
            check_table(config, {"panel", "rotation", "frequency", "pins"}, {"panel", "pins", "frequency", "rotation"}, where)
            panel_where = f"{where}.panel"
            fields = {
                "width", "height", "gram_width", "gram_height", "offset_x", "offset_y",
                "madctl", "invert", "three_wire", "init",
            }
            panel = check_table(config["panel"], fields, fields, panel_where)
            for name in ("width", "height", "gram_width", "gram_height"):
                check_int(panel[name], 1, 320, f"{panel_where}.{name}")
            for name in ("offset_x", "offset_y"):
                check_int(panel[name], 0, 320, f"{panel_where}.{name}")
            rotation = check_int(config["rotation"], 0, 3, f"{where}.rotation")
            if rotation != 3:
                raise ValueError(f"{where}.rotation: only rotation 3 is implemented")
            geometry = tuple(panel[name] for name in ("width", "height", "gram_width", "gram_height"))
            if geometry != (240, 280, 240, 320):
                raise ValueError(f"{panel_where}.only 240x280 glass with 240x320 GRAM is supported")
            width, height = panel["height"], panel["width"]
            column = panel["gram_height"] - panel["height"] - panel["offset_y"]
            row = panel["offset_x"]
            if column < 0 or column + width > panel["gram_height"] or row + height > panel["gram_width"]:
                raise ValueError(f"{panel_where}.offsets place canvas outside GRAM")
            check_int(panel["madctl"], 0, 255, f"{panel_where}.madctl")
            if panel["madctl"] & 0xf0 != 0xb0:
                raise ValueError(f"{panel_where}.madctl: incompatible with rotation 3")
            for name in ("invert", "three_wire"):
                check_bool(panel[name], f"{panel_where}.{name}")
            frequency = check_int(
                config["frequency"], 1, 80000000, f"{where}.frequency"
            )
            if not isinstance(panel["init"], list) or not panel["init"]:
                raise ValueError(f"{panel_where}.init: expected nonempty command list")
            for index, command in enumerate(panel["init"]):
                field = f"{panel_where}.init[{index}]"
                check_table(command, {"command", "data", "delay_ms"}, {"command", "data", "delay_ms"}, field)
                check_int(command["command"], 0, 255, field + ".command")
                check_int(command["delay_ms"], 0, 1000, field + ".delay_ms")
                if not isinstance(command["data"], list) or len(command["data"]) > 14:
                    raise ValueError(f"{field}.data: expected at most 14 bytes")
                for byte in command["data"]:
                    check_int(byte, 0, 255, field + ".data")
            normalized[kind] = dict(
                panel=panel,
                pins=pins(config["pins"], ("mosi", "sclk", "cs", "dc", "reset"), where + ".pins"),
                frequency=frequency,
            )
        elif module == "cst816":
            check_table(
                config, {"pins", "frequency", "address", "raw_size", "swap_xy", "mirror_x", "mirror_y"},
                {"pins", "raw_size"}, where,
            )
            raw = config["raw_size"]
            if not isinstance(raw, list) or len(raw) != 2:
                raise ValueError(f"{where}.raw_size: expected [width, height]")
            for value in raw:
                check_int(value, 1, 4096, where + ".raw_size")
            flags = {
                name: check_bool(config.get(name, False), where + "." + name)
                for name in ("swap_xy", "mirror_x", "mirror_y")
            }
            normalized[kind] = dict(
                pins=pins(config["pins"], ("sda", "scl", "reset", "interrupt"), where + ".pins"),
                raw=raw, **flags,
                frequency=check_int(config.get("frequency", 400000), 10000, 400000, where + ".frequency"),
                address=check_int(config.get("address", 0x15), 8, 0x77, where + ".address"),
            )
        elif module in ("qmi8658", "pcf85063"):
            check_table(config, {"pins", "address"}, {"pins"}, where)
            selected_pins = config["pins"]
            names = ("sda", "scl", "interrupt") if isinstance(selected_pins, dict) and "interrupt" in selected_pins else ("sda", "scl")
            normalized[kind] = dict(
                pins=pins(selected_pins, names, where + ".pins"),
                address=check_int(config.get("address", 0x6b if module == "qmi8658" else 0x51), 8, 0x77, where + ".address"),
            )
        elif module in ("bmp280", "bme280", "veml7700"):
            check_table(config, {"pins", "address"}, {"pins"}, where)
            address = config.get("address", 0x10 if module == "veml7700" else 0x76)
            check_int(address, 8, 0x77, where + ".address")
            if address not in ((0x10,) if module == "veml7700" else (0x76, 0x77)):
                raise ValueError(f"{where}.address: unsupported sensor address")
            normalized[kind] = dict(pins=pins(config["pins"], ("sda", "scl"), where + ".pins"), address=address)
        elif module == "battery_adc":
            check_table(config, {"pins", "divider"}, {"pins", "divider"}, where)
            divider = config["divider"]
            if not isinstance(divider, list) or len(divider) != 2:
                raise ValueError(f"{where}.divider: expected [numerator, denominator]")
            for value in divider:
                check_int(value, 1, 100, where + ".divider")
            if divider[0] < divider[1]:
                raise ValueError(f"{where}.divider: numerator must be >= denominator")
            selected_pins = pins(config["pins"], ("adc",), where + ".pins")
            check_int(selected_pins["adc"], 1, 10, where + ".pins.adc")
            normalized[kind] = dict(pins=selected_pins, divider=divider)
        elif module in ("pwm_buzzer", "pwm_backlight"):
            check_table(config, {"pins"}, {"pins"}, where)
            normalized[kind] = dict(pins=pins(config["pins"], ("output",), where + ".pins"))
        elif module == "power_button":
            check_table(config, {"pins"}, {"pins"}, where)
            normalized[kind] = dict(pins=pins(config["pins"], ("hold", "button", "boot"), where + ".pins"))
        else:
            raise ValueError(f"{where}: module has no configuration validator")
        for name in manifest["sources"]:
            source = (directory / name).resolve()
            if not source.is_relative_to(MODULE_DIR.parent.resolve()) or not source.is_file():
                raise ValueError(f"{where}: invalid module source {name!r}")
            sources.append(source.relative_to(MODULE_DIR.parent.resolve()).as_posix())
        for dependency in manifest["idf_requires"]:
            if not isinstance(dependency, str) or not re.fullmatch(r"[a-zA-Z0-9_]+", dependency):
                raise ValueError(f"{where}: invalid IDF dependency")
            if dependency not in requires:
                requires.append(dependency)
    if bus_signals:
        normalized["i2c"] = bus_signals
        sources.append("hardware/i2c.cpp")
    if "bmp280" in normalized or "bme280" in normalized:
        sources.append("hardware/bosch280.cpp")
    if "buzzer" in normalized or "backlight" in normalized:
        sources.append("hardware/pwm.cpp")
    addresses = [normalized[kind]["address"] for kind in ("touch", "imu", "rtc", "bmp280", "bme280", "light") if kind in normalized]
    if len(set(addresses)) != len(addresses):
        raise ValueError(f"{path}: modules: duplicate I2C address")
    touch = normalized.get("touch")
    if touch and tuple(reversed(touch["raw"]) if touch["swap_xy"] else touch["raw"]) != (width, height):
        raise ValueError(f"{path}: modules.cst816.raw_size: transformed size must match canvas {width}x{height}")
    for kind, source in (("display", "display/disabled.cpp"), ("touch", "input/disabled.cpp")):
        if kind not in normalized:
            sources.append(source)
    return data, dict(width=width, height=height, **normalized), sources, requires


def generated_headers(profile, config):
    def literal(value):
        return str(value).lower() if type(value) is bool else json.dumps(value, ensure_ascii=True)

    constants = {
        "MODEL_ID": profile["model"],
        "DEVICE_ID": profile["model"], "IDF_TARGET": "esp32s3",
        "DESIGN_W": config["width"], "DESIGN_H": config["height"],
        "FB_MAX_W": config["width"], "FB_MAX_H": config["height"],
    }
    header = "// Generated from device.toml. Do not edit.\n#pragma once\n#include <cstdint>\n"
    for role in ("DISPLAY", "TOUCH", "I2C", "IMU", "RTC", "BATTERY", "BUZZER", "BACKLIGHT", "POWER_BUTTON", "BMP280", "BME280", "LIGHT"):
        header += f"#define FLINT_HAS_{role} {int(role.lower() in config)}\n"
    for role in ("imu", "rtc", "battery", "buzzer", "backlight", "power_button", "bmp280", "bme280", "light"):
        selected = config.get(role)
        if not selected:
            continue
        for name, pin in selected["pins"].items():
            constants[f"{role.upper()}_{name.upper()}"] = pin
        if role in ("imu", "rtc", "bmp280", "bme280", "light"):
            constants[f"{role.upper()}_ADDRESS"] = selected["address"]
        if role == "battery":
            constants["BATTERY_DIVIDER_NUMERATOR"], constants["BATTERY_DIVIDER_DENOMINATOR"] = selected["divider"]
    if "i2c" in config:
        constants.update(I2C_SDA=config["i2c"]["sda"], I2C_SCL=config["i2c"]["scl"])
    display = config.get("display")
    result = {}
    if display:
        p = display["panel"]
        signals = (("MOSI", "mosi"), ("SCLK", "sclk"), ("CS", "cs"), ("DC", "dc"), ("RST", "reset"))
        constants.update({f"TFT_{key}": display["pins"][name] for key, name in signals})
        constants.update(
            TFT_MISO=-1, SPI_FREQ_WRITE=display["frequency"],
            SPI_3WIRE=p["three_wire"], PANEL_INVERT=p["invert"],
        )
        commands = ""
        for command in p["init"]:
            values = ", ".join(map(str, command["data"]))
            commands += (
                f'    {{{command["command"]}, {len(command["data"])}, '
                f'{command["delay_ms"]}, {{{values}}}}},\n'
            )
        result["device_display.h"] = (
            '#pragma once\n#include "modules/st7789/config.h"\nnamespace flint::board {\n'
            f'inline constexpr int St7789ColumnOffset = {p["gram_height"] - p["height"] - p["offset_y"]};\n'
            f'inline constexpr int St7789RowOffset = {p["offset_x"]};\n'
            f'inline constexpr uint8_t St7789Madctl = {p["madctl"]};\n'
            'inline constexpr flint::modules::St7789InitCommand St7789Init[]{\n' + commands + '};\n}\n'
        )
    for key, value in constants.items():
        typename = "const char *" if isinstance(value, str) else "bool" if type(value) is bool else "int"
        header += f"inline constexpr {typename} FLINT_{key} = {literal(value)};\n"
    result["device_config.h"] = header
    touch = config.get("touch")
    if touch:
        pins = touch["pins"]
        transform = ", ".join(literal(touch[key]) for key in ("swap_xy", "mirror_x", "mirror_y"))
        result["device_touch.h"] = (
            '#pragma once\n#include "modules/cst816/cst816.h"\n#include "input/touch_state.h"\n'
            'inline constexpr flint::modules::Cst816Config FLINT_TOUCH_CONFIG{\n'
            f'.port = -1, .sda = {pins["sda"]}, .scl = {pins["scl"]}, '
            f'.reset = {pins["reset"]}, .interrupt = {pins["interrupt"]}, '
            f'.frequency = {touch["frequency"]}, .address = {touch["address"]}' + '};\n'
            'inline constexpr flint::input::TouchTransform FLINT_TOUCH_TRANSFORM{'
            f'{config["width"]}, {config["height"]}, {transform}' + '};\n'
        )
    return result


def cmd_generate(device_arg: str, *, quiet: bool, build_dir: Path = DEFAULT_BUILD_DIR) -> tuple[str, dict]:
    selected = resolve_device_id(device_arg)
    profile = PROFILES[selected]
    settings = CONFIG_DIR / profile["model"]
    data, config, sources, requires = load_device(settings)
    inputs = generated_headers(profile, config)
    cmake = '# Generated from device.toml. Do not edit.\nset(FLINT_DEVICE_IDF_TARGET "esp32s3")\n'
    for name, values in (("FLINT_MODULE_SOURCES", sources), ("FLINT_MODULE_IDF_REQUIRES", requires)):
        cmake += f"set({name}\n" + "".join(f'    "{value}"\n' for value in values) + ")\n"
    inputs["device.cmake"] = cmake
    output = build_dir / "generated" / "board"
    output.mkdir(parents=True, exist_ok=True)
    for name in ("device_display.h", "device_touch.h"):
        if name not in inputs:
            (output / name).unlink(missing_ok=True)
    for name, content in inputs.items():
        target = output / name
        encoded = content.encode("utf-8")
        if not target.exists() or target.read_bytes() != encoded:
            target.write_bytes(encoded)
    if not quiet:
        print(f"{data['name']} ({selected}, {profile['idf_target']})")
    return selected, dict(profile)


def current_idf_target(sdkconfig: Path = DEFAULT_SDKCONFIG) -> str | None:
    """Read the selected persistent configuration, falling back to the legacy root file."""
    sdk = sdkconfig
    if not sdk.is_file() and sdkconfig.resolve() == DEFAULT_SDKCONFIG.resolve():
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
        # ~/.espressif/v6.1.0/esp-idf
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
    """Return the SDK and its Python script, not a Windows PATH executable wrapper."""
    env_path = os.environ.get("IDF_PATH") or os.environ.get("FLINT_IDF_PATH")
    if env_path:
        root = Path(env_path).expanduser().resolve()
        idf_py = root / "tools" / "idf.py"
        if idf_py.is_file():
            return root, idf_py

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
    venv_python = env_dir / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
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
    env["PATH"] = f"{venv_python.parent}{os.pathsep}{env.get('PATH', '')}"

    export_sh = idf_path / "export.sh"
    # Always invoke idf.py with the venv interpreter — avoids `env: python: No such file`.
    idf_cmd = [str(venv_python), str(idf_py), *args]

    if os.name != "nt" and export_sh.is_file() and not (
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


def ensure_idf_target(wanted: str, *, force: bool, build_dir: Path = DEFAULT_BUILD_DIR,
                      sdkconfig: Path = DEFAULT_SDKCONFIG) -> None:
    current = current_idf_target(sdkconfig)
    if not force and current is None:
        # A fresh configure selects the target without the destructive fullclean action.
        return
    if not force and current == wanted:
        print(f"idf target already {wanted}", flush=True)
        return
    if current and current != wanted:
        print(f"idf target {current} → {wanted}", flush=True)
    run_idf(["-B", str(build_dir), f"-DSDKCONFIG={sdkconfig}", "set-target", wanted])


def firmware_bin_path(build_dir: Path = DEFAULT_BUILD_DIR) -> Path:
    """ESP-IDF app image for this project (CMake project(flint_os))."""
    return build_dir / "flint_os.bin"


def cmd_idf_actions(
    device_id: str,
    board: dict,
    *,
    do_build: bool,
    do_flash: bool,
    do_monitor: bool,
    port: str,
    set_target: bool,
    build_dir: Path = DEFAULT_BUILD_DIR,
    sdkconfig: Path = DEFAULT_SDKCONFIG,
) -> int:
    """Run idf.py actions. build/flash/monitor are independent."""
    target = str(board["idf_target"]).strip()
    if not target:
        raise SystemExit(f"board {device_id}: empty idf_target")

    need_target = do_build or do_flash
    if need_target and set_target:
        ensure_idf_target(target, force=False, build_dir=build_dir, sdkconfig=sdkconfig)
    elif need_target and not set_target:
        current = current_idf_target(sdkconfig)
        if current and current != target:
            print(
                f"warning: sdkconfig target is {current}, board wants {target} "
                f"(omit --no-set-target to fix)",
                file=sys.stderr,
            )

    # --flash: only build if there is nothing to flash yet.
    if do_flash and not do_build and not firmware_bin_path(build_dir).is_file():
        print(
            f"no {firmware_bin_path(build_dir)} — building once before flash",
            flush=True,
        )
        do_build = True
        if set_target:
            ensure_idf_target(target, force=False, build_dir=build_dir, sdkconfig=sdkconfig)

    idf_args: list[str] = ["-B", str(build_dir), f"-DSDKCONFIG={sdkconfig}"]
    if do_build or do_flash:
        idf_args.append(f"-DFLINT_DEVICE={device_id}")
        if set_target:
            idf_args.append(f"-DIDF_TARGET={target}")
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
    ap.add_argument("-B", "--build-dir", type=Path, default=DEFAULT_BUILD_DIR,
                    help="build directory (default: build/); owns generated headers and images")
    ap.add_argument("--sdkconfig", type=Path, default=DEFAULT_SDKCONFIG,
                    help="persistent local configuration (default: config/sdkconfig)")
    ap.add_argument(
        "-d",
        "--device",
        default="",
        metavar="ID",
        help="supported model (else FLINT_DEVICE, else release default)",
    )
    ap.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="list models available for -DFLINT_DEVICE / --device (no files written)",
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
        help="configure and build the selected device; set-target only for a target mismatch",
    )
    ap.add_argument(
        "--flash",
        action="store_true",
        help="flash the selected build (builds first if its firmware image is missing)",
    )
    ap.add_argument(
        "--monitor",
        action="store_true",
        help="monitor the selected build (does not generate, build or flash)",
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

    build_dir = args.build_dir.expanduser().resolve()
    sdkconfig = args.sdkconfig.expanduser().resolve()
    device_id = resolve_device_id(args.device)
    board = PROFILES[device_id]

    if args.build or args.flash or args.monitor:
        return cmd_idf_actions(
            device_id,
            board,
            do_build=bool(args.build),
            do_flash=bool(args.flash),
            do_monitor=bool(args.monitor),
            port=str(args.port or ""),
            set_target=not bool(args.no_set_target),
            build_dir=build_dir,
            sdkconfig=sdkconfig,
        )
    cmd_generate(args.device, quiet=bool(args.quiet), build_dir=build_dir)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError) as e:
        print(f"device configuration: {e}", file=sys.stderr)
        raise SystemExit(1)
    except BrokenPipeError:
        # e.g. `… --list | head`
        raise SystemExit(0)
