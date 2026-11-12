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
        self.assertIn("flint_spark_s3_supermini", selector.PROFILES)
        self.assertTrue(all(p["idf_target"] == "esp32s3" for p in selector.PROFILES.values()))
        for name in ("flint_spark_c3_dev", "../s3", "esp32c3"):
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
            self.assertFalse((selector.DEVICE_DIR / "device_config.h").exists())

    def test_separate_builds_do_not_share_generated_files(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory) / "a", Path(directory) / "b"
            selector.cmd_generate("", quiet=True, build_dir=a)
            selector.cmd_generate("", quiet=True, build_dir=b)
            header = Path("generated/board/device_config.h")
            (a / header).write_text("local test")
            self.assertNotEqual((a / header).read_bytes(), (b / header).read_bytes())

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
