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
        cls.gen = load_tool("gen_board_catalog")

    def test_board_index_ids_match_files(self):
        index = self.gen.load_index()
        self.assertTrue(index.get("default"))
        boards_dir = ROOT / "components" / "flint" / "board" / "boards"
        for entry in index.get("boards") or []:
            bid = entry["id"]
            path = self.gen.board_json_path(entry, bid)
            self.assertTrue(path.is_file(), path)
            data = self.gen.load_json(path)
            self.assertEqual(data["id"], bid)
            self.assertEqual(path.parent, boards_dir)

    def test_flint_spark_validates(self):
        index = self.gen.load_index()
        board_id = "flint_spark"
        entry = self.gen.find_board_entry(index, board_id)
        board = self.gen.load_json(self.gen.board_json_path(entry, board_id))
        display_id = board["display"]
        display_path = (
            ROOT / "components" / "flint" / "board" / "periph" / "display" / f"{display_id}.json"
        )
        display = self.gen.load_json(display_path)
        self.gen.validate(board, copy.deepcopy(display), board_id)

    def test_board_validate_rejects_id_mismatch(self):
        index = self.gen.load_index()
        entry = self.gen.find_board_entry(index, "flint_spark")
        board = self.gen.load_json(self.gen.board_json_path(entry, "flint_spark"))
        display = self.gen.load_json(
            ROOT
            / "components"
            / "flint"
            / "board"
            / "periph"
            / "display"
            / f"{board['display']}.json"
        )
        with self.assertRaises(SystemExit):
            self.gen.validate(board, copy.deepcopy(display), "not_spark")

    def test_normalize_panel_alias(self):
        self.assertEqual(self.gen.normalize_panel("GC9A01A"), "GC9A01")
        self.assertEqual(self.gen.normalize_panel("st7789"), "ST7789")


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
        self.assertIn("WifiOff", ids)
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
