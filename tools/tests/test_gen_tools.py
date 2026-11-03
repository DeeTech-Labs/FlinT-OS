"""Smoke tests for tools/gen_* — real repo JSON / .po / headers (stdlib unittest)."""

from __future__ import annotations

import copy
import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"


def load_tool(name: str):
    path = TOOLS / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


class GenBoardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gen = load_tool("select_device")

    def test_only_supported_revision_can_be_selected(self):
        self.assertEqual(self.gen.resolve_device_id("flint_spark_c3_dev"), "flint_spark_c3_dev")
        for name in ("my_desk_s3", "../flint_spark", "waveshare_s3"):
            with self.subTest(name=name), self.assertRaises(SystemExit):
                self.gen.resolve_device_id(name)

    def test_selected_hardware_matches_fixed_revision(self):
        import tempfile
        from unittest.mock import patch
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / name for name in
                     ("device_config.h", "device_display.h", "device.cmake")]
            with patch.multiple(self.gen, OUT_INC=paths[0], OUT_IMPL=paths[1], OUT_CMAKE=paths[2]):
                selected, profile = self.gen.cmd_generate("flint_spark_c3_dev", quiet=True)
                self.assertEqual(profile["idf_target"], "esp32c3")
                source = self.gen.DEVICE_DIR / profile["directory"]
                for path in paths:
                    self.assertEqual(path.read_bytes(), (source / path.name).read_bytes())
                modified = paths[0].stat().st_mtime_ns
                self.gen.cmd_generate(selected, quiet=True)
                self.assertEqual(paths[0].stat().st_mtime_ns, modified)
                self.assertIn('FLINT_TFT_RST = 3', paths[0].read_text())
                self.assertIn('FLINT_STATUS_LED_GPIO = 8', paths[0].read_text())



class GenI18nTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gen = load_tool("gen_i18n_catalog")

    def test_pot_msgids_unique(self):
        pot = ROOT / "components" / "flint" / "i18n" / "lang" / "flint.pot"
        ids = self.gen.msgids_from_pot(pot)
        self.assertIn("Waiting for time", ids)
        self.assertEqual(len(ids), len(set(ids)))

    def test_po_covers_pot(self):
        pot = ROOT / "components" / "flint" / "i18n" / "lang" / "flint.pot"
        msgids = self.gen.msgids_from_pot(pot)
        for locale in ("en_US", "ru_RU"):
            po = ROOT / "components" / "flint" / "i18n" / "lang" / locale / "flint.po"
            entries = self.gen.parse_po(po)
            missing = [m for m in msgids if m not in entries]
            self.assertFalse(missing, f"{locale} missing {missing[:5]}")


class GenAssetPackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gen = load_tool("gen_asset_pack")

    def test_versions_and_icon_ids(self):
        fmt, api_min, pack_ver = self.gen.load_pack_versions()
        self.assertGreaterEqual(fmt, 1)
        self.assertGreaterEqual(api_min, 1)
        self.assertGreaterEqual(pack_ver, 1)
        ids = self.gen.load_icon_ids()
        self.assertNotIn("WifiOff", ids)
        self.assertIn("BrandMark", ids)
        assets = ROOT / "assets"
        for png in assets.rglob("*.png"):
            if "fs_root" in png.parts:
                continue
            m = self.gen.NAME_RE.match(png.name)
            self.assertIsNotNone(m, f"bad asset name {png.relative_to(ROOT)}")
            assert m is not None
            self.assertIn(m.group(1), ids, f"{png.name} id not in FlintIconId")


if __name__ == "__main__":
    unittest.main()
