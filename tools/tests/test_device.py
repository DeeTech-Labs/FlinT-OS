import importlib.util
import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "select_device", ROOT / "tools/select_device.py"
)
selector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selector)


class DeviceTests(unittest.TestCase):
    def test_s3_only(self):
        self.assertEqual(list(selector.PROFILES), ["flint_spark_s3_supermini"])
        for name in ("flint_spark_c3_dev", "../s3", "esp32c3"):
            with self.assertRaises(SystemExit):
                selector.resolve_device_id(name)

    def test_generated_profile(self):
        with tempfile.TemporaryDirectory() as directory:
            paths = [
                Path(directory) / name
                for name in ("device_config.h", "device_display.h", "device.cmake")
            ]
            with patch.multiple(
                selector, OUT_INC=paths[0], OUT_IMPL=paths[1], OUT_CMAKE=paths[2]
            ):
                id, profile = selector.cmd_generate("", quiet=True)
                self.assertEqual(profile["idf_target"], "esp32s3")
                text = paths[0].read_text()
                for signal, pin in [
                    ("SCLK", 12),
                    ("MOSI", 11),
                    ("CS", 10),
                    ("DC", 4),
                    ("RST", 5),
                ]:
                    self.assertIn(f"FLINT_TFT_{signal} = {pin}", text)


if __name__ == "__main__":
    unittest.main()
