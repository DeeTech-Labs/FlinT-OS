import importlib.util
import unittest
import tempfile
import os
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "select_device", ROOT / "tools/select_device.py"
)
selector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selector)


class DeviceTests(unittest.TestCase):
    def test_idf_environment_uses_script_instead_of_windows_wrapper(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / "tools" / "idf.py"
            script.parent.mkdir()
            script.write_text("# SDK entry point\n")
            with patch.dict(os.environ, {"IDF_PATH": str(root)}), \
                 patch.object(selector.shutil, "which", return_value="idf.py.exe"):
                self.assertEqual(selector.discover_idf(), (root.resolve(), script.resolve()))

    def test_s3_only(self):
        self.assertIn("flint_spark", selector.PROFILES)
        self.assertTrue(all(p["idf_target"] == "esp32s3" for p in selector.PROFILES.values()))
        for name in ("flint_spark_s3_supermini", "flint_spark_c3_dev", "../s3", "esp32c3"):
            with self.assertRaises(SystemExit):
                selector.resolve_device_id(name)

    def test_generated_profile_is_isolated_and_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            id, profile = selector.cmd_generate("", quiet=True, build_dir=build)
            self.assertEqual(profile["idf_target"], "esp32s3")
            output = build / "generated" / "board"
            text = (output / "device_config.h").read_text()
            for signal, pin in [("SCLK", 12), ("MOSI", 11), ("CS", 10), ("DC", 4), ("RST", 5)]:
                self.assertIn(f"FLINT_TFT_{signal} = {pin}", text)
            before = {p.name: p.stat().st_mtime_ns for p in output.iterdir()}
            selector.cmd_generate("", quiet=True, build_dir=build)
            self.assertEqual(before, {p.name: p.stat().st_mtime_ns for p in output.iterdir()})
            self.assertFalse((selector.MODULE_DIR / "device_config.h").exists())

    def test_separate_builds_do_not_share_generated_files(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / "a", Path(directory) / "b"
            selector.cmd_generate("", quiet=True, build_dir=a)
            selector.cmd_generate("", quiet=True, build_dir=b)
            header = Path("generated/board/device_config.h")
            (a / header).write_text("local test")
            self.assertNotEqual((a / header).read_bytes(), (b / header).read_bytes())

    def test_spark_build_excludes_touch_driver_and_i2c(self):
        settings = selector.CONFIG_DIR / "flint_spark"
        _, config, sources, requires = selector.load_device(settings)
        self.assertIn("display", config)
        self.assertNotIn("touch", config)
        self.assertIn("modules/st7789/st7789.cpp", sources)
        self.assertIn("input/disabled.cpp", sources)
        self.assertFalse(any("cst816" in source for source in sources))
        self.assertNotIn("esp_driver_i2c", requires)

    def test_module_switch_removes_stale_headers_and_dependencies(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            settings = root / "config" / "flint_spark"
            settings.mkdir(parents=True)
            original = (selector.CONFIG_DIR / "flint_spark/device.toml").read_text(encoding="utf-8")
            touch = """
[modules.cst816]
raw_size = [240, 280]
swap_xy = true
pins = { sda = 1, scl = 2, reset = 6, interrupt = 7 }
"""
            build = root / "build"
            with patch.object(selector, "CONFIG_DIR", root / "config"):
                (settings / "device.toml").write_text(original + touch, encoding="utf-8")
                selector.cmd_generate("", quiet=True, build_dir=build)
                output = build / "generated" / "board"
                cmake = (output / "device.cmake").read_text()
                self.assertIn("modules/cst816/cst816.cpp", cmake)
                self.assertIn("esp_driver_i2c", cmake)
                self.assertIn("280, 240, true, false, false", (output / "device_touch.h").read_text())
                (settings / "device.toml").write_text('name = "Headless"\n', encoding="utf-8")
                selector.cmd_generate("", quiet=True, build_dir=build)
                cmake = (output / "device.cmake").read_text()
                self.assertNotIn("st7789.cpp", cmake)
                self.assertNotIn("cst816.cpp", cmake)
                self.assertNotIn("esp_driver_i2c", cmake)
                self.assertNotIn("esp_driver_spi", cmake)
                self.assertIn("display/disabled.cpp", cmake)
                self.assertIn("#define FLINT_HAS_TOUCH 0", (output / "device_config.h").read_text())
                self.assertIn("#define FLINT_HAS_DISPLAY 0", (output / "device_config.h").read_text())
                self.assertFalse((output / "device_touch.h").exists())
                self.assertFalse((output / "device_display.h").exists())

    def test_invalid_device_configuration_preserves_previous_output(self):
        original = (selector.CONFIG_DIR / "flint_spark/device.toml").read_text(encoding="utf-8")
        variants = [
            (original.replace("rotation = 3", "rotaton = 3"), "rotaton"),
            (original.replace("rotation = 3", "rotation = 1"), "rotation"),
            (original.replace("cs = 10", "cs = 11"), "already assigned"),
            (original.replace("cs = 10", "cs = 22"), "unavailable"),
            (original.replace("cs = 10", "cs = true"), "integer"),
            (original.split("[modules.st7789.panel]")[0] + 'panel = "../escape"\n', "expected a table"),
            (original.replace("modules.st7789", "modules.unknown"), "unknown"),
            (original + "\n[canvas]\nwidth = 240\nheight = 280\n", "canvas"),
            (original + "\n[modules.cst816]\nraw_size = [240, 280]\npins = { sda = 1, scl = 2, reset = 6, interrupt = 7 }\n", "raw_size"),
        ]
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory) / "config"
            settings = config / "flint_spark"
            settings.mkdir(parents=True)
            build = Path(directory) / "build"
            with patch.object(selector, "CONFIG_DIR", config):
                (settings / "device.toml").write_text(original, encoding="utf-8")
                selector.cmd_generate("", quiet=True, build_dir=build)
                output = build / "generated/board"
                before = {p.name: p.read_bytes() for p in output.iterdir()}
                for text, message in variants:
                    with self.subTest(message=message):
                        (settings / "device.toml").write_text(text, encoding="utf-8")
                        with self.assertRaisesRegex(ValueError, message):
                            selector.cmd_generate("", quiet=True, build_dir=build)
                        self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})

    def test_discovery_ignores_future_config_and_uses_model_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            config = Path(directory)
            model = config / "example"
            model.mkdir()
            (model / "device.toml").write_text('name = "Example"\n', encoding="utf-8")
            (model / "device.future.toml").write_text('name = "Future"\n', encoding="utf-8")
            profiles = selector.discover_profiles(config)
            self.assertEqual(list(profiles), ["example"])
            self.assertEqual(profiles["example"]["model"], "example")
            self.assertNotIn("revision", profiles["example"])
            invalid = config / "Invalid"
            invalid.mkdir()
            (invalid / "device.toml").write_text('name = "Invalid"\n', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "lowercase identifier"):
                selector.discover_profiles(config)

    def test_inline_panel_rejects_oversized_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            settings = Path(directory)
            text = (selector.CONFIG_DIR / "flint_spark/device.toml").read_text(encoding="utf-8")
            text = text.replace("data = [0x35]", "data = [" + ",".join(["0"] * 15) + "]")
            (settings / "device.toml").write_text(text, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "14 bytes"):
                selector.load_device(settings)

    def test_future_modules_share_bus_and_disabled_modules_are_excluded(self):
        future = (selector.CONFIG_DIR / "flint_spark/device.future.toml").read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as directory:
            settings = Path(directory)
            (settings / "device.toml").write_text(future, encoding="utf-8")
            _, config, sources, requirements = selector.load_device(settings)
            self.assertTrue(all(role in config for role in ("display", "touch", "imu", "rtc", "battery", "buzzer", "backlight", "power_button")))
            self.assertEqual(config["i2c"], {"sda": 11, "scl": 10})
            self.assertEqual(sources.count("hardware/i2c.cpp"), 1)
            self.assertEqual(sources.count("hardware/pwm.cpp"), 1)
            self.assertTrue(all(dep in requirements for dep in ("esp_adc", "esp_driver_i2c", "esp_driver_ledc")))
            for old, new, message in (("address = 0x51", "address = 0x15", "duplicate I2C address"),
                                      ("sda = 11, scl = 10, interrupt = 38", "sda = 12, scl = 10, interrupt = 38", "one shared bus"),
                                      ("output = 42", "output = 15", "already assigned")):
                (settings / "device.toml").write_text(future.replace(old, new), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, message):
                    selector.load_device(settings)
        _, config, sources, requirements = selector.load_device(selector.CONFIG_DIR / "flint_spark")
        self.assertFalse(any(role in config for role in ("imu", "rtc", "battery", "buzzer", "backlight", "power_button", "i2c")))
        self.assertFalse(any(dep in requirements for dep in ("esp_adc", "esp_driver_i2c", "esp_driver_ledc")))
        self.assertEqual(set(selector.PROFILES), {"flint_spark"})

    def test_optional_external_sensors_share_bus_and_reject_address_conflicts(self):
        future = (selector.CONFIG_DIR / "flint_spark/device.future.toml").read_text(encoding="utf-8")
        enabled = "\n".join(line[2:] if line.startswith(("# [modules.", "# pins =", "# address =")) else line for line in future.splitlines())
        with tempfile.TemporaryDirectory() as directory:
            settings = Path(directory)
            (settings / "device.toml").write_text(enabled, encoding="utf-8")
            _, config, sources, dependencies = selector.load_device(settings)
            self.assertTrue(all(role in config for role in ("bmp280", "bme280", "light")))
            self.assertEqual(sources.count("hardware/bosch280.cpp"), 1)
            self.assertEqual(sources.count("hardware/i2c.cpp"), 1)
            self.assertIn("modules/veml7700/veml7700.cpp", sources)
            self.assertEqual(config["light"]["address"], 0x10)
            (settings / "device.toml").write_text(enabled.replace("address = 0x77", "address = 0x76"), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate I2C address"):
                selector.load_device(settings)
            (settings / "device.toml").write_text(enabled.replace("address = 0x10", "address = 0x11"), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unsupported sensor address"):
                selector.load_device(settings)

    def test_target_reads_selected_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / "sdkconfig").write_text('CONFIG_IDF_TARGET="esp32s3"\n')
            self.assertEqual(selector.current_idf_target(build / "sdkconfig"), "esp32s3")

    def test_idf_actions_use_selected_build(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            profile = selector.PROFILES[selector.DEFAULT_PROFILE]
            with patch.object(selector, "run_idf") as run:
                selector.cmd_idf_actions(selector.DEFAULT_PROFILE, profile,
                    do_build=True, do_flash=False, do_monitor=False, port="",
                    set_target=False, build_dir=build, sdkconfig=build / "sdkconfig")
            self.assertEqual(run.call_args.args[0],
                ["-B", str(build), f"-DSDKCONFIG={build / 'sdkconfig'}",
                 f"-DFLINT_DEVICE={selector.DEFAULT_PROFILE}", "build"])

    def test_fresh_build_does_not_set_target_or_generate_before_configure(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory) / "build"
            sdkconfig = Path(directory) / "config" / "sdkconfig"
            with patch.object(selector, "ROOT", Path(directory)), \
                 patch.object(selector, "run_idf") as run, \
                 patch.object(selector, "cmd_generate") as generate, \
                 patch("sys.argv", ["select_device.py", "--build", "-B", str(build),
                                    "--sdkconfig", str(sdkconfig)]):
                self.assertEqual(selector.main(), 0)
            generate.assert_not_called()
            run.assert_called_once()
            args = run.call_args.args[0]
            self.assertNotIn("set-target", args)
            self.assertIn("-DIDF_TARGET=esp32s3", args)
            self.assertFalse(build.exists())


if __name__ == "__main__":
    unittest.main()
