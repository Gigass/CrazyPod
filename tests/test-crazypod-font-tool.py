#!/usr/bin/env python3

import importlib.util
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
TOOL_PATH = REPO_ROOT / "tools" / "crazypod_font_tool.py"
SPEC = importlib.util.spec_from_file_location("crazypod_font_tool", TOOL_PATH)
FONT_TOOL = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(FONT_TOOL)


class FontManifestTest(unittest.TestCase):
    def test_spacing_separators_are_kept_but_layout_controls_are_not(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "strings.txt"
            source.write_text(
                "USB DATA\tMODE\nCrazyPod\u00a0storage\u3000active",
                encoding="utf-8",
            )

            codepoints = FONT_TOOL.required_codepoints([source])

        self.assertIn(0x0020, codepoints)
        self.assertIn(0x00A0, codepoints)
        self.assertIn(0x3000, codepoints)
        self.assertNotIn(0x0009, codepoints)
        self.assertNotIn(0x000A, codepoints)

    def test_manifest_reader_preserves_space_advance_glyphs(self):
        with tempfile.TemporaryDirectory() as directory:
            manifest = Path(directory) / "chars.txt"
            manifest.write_text(" AB\n", encoding="utf-8")

            codepoints = FONT_TOOL.read_chars(manifest)

        self.assertEqual(codepoints, {0x0020, ord("A"), ord("B")})

    def test_lvgl_spacing_glyph_requires_positive_advance(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path(directory) / "font.c"
            artifact.write_text(
                """
                /* U+0020 " " */
                /* U+0041 "A" */
                static const glyph glyph_dsc[] = {
                    {.bitmap_index = 0, .adv_w = 0},
                    {.bitmap_index = 0, .adv_w = 36},
                    {.bitmap_index = 0, .adv_w = 80},
                };
                """,
                encoding="utf-8",
            )

            advances = FONT_TOOL.lvgl_glyph_advances(artifact)

        self.assertEqual(advances[0x0020], 36)
        self.assertEqual(advances[ord("A")], 80)


if __name__ == "__main__":
    unittest.main()
