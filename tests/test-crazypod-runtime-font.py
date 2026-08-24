#!/usr/bin/env python3

from pathlib import Path
import re
import struct
import sys


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_HEADER = ROOT / "apps/crazypod/crazypod_runtime_font.h"
RUNTIME_SOURCE = ROOT / "apps/crazypod/crazypod_runtime_font.c"
SPEC_FILE = ROOT / "tools/crazypod-runtime-font-specs.txt"
SCENE_SOURCE = ROOT / "apps/crazypod/ui/features/miniapps/crazypod_miniapp_scene.c"
SCENE_RENDERER = ROOT / (
    "apps/crazypod/ui/features/miniapps/"
    "crazypod_miniapp_scene_renderer.c"
)

SPECS = set()
for line_number, raw_line in enumerate(
        SPEC_FILE.read_text(encoding="ascii").splitlines(), 1):
    line = raw_line.strip()
    if not line or line.startswith("#"):
        continue
    match = re.fullmatch(r"(system|serif|mono):(\d{3}):(\d{1,2})", line)
    if match is None:
        raise SystemExit(
            f"invalid runtime font spec at {SPEC_FILE}:{line_number}: {line}"
        )
    spec = (match.group(1), int(match.group(2)), int(match.group(3)))
    if spec in SPECS:
        raise SystemExit(f"duplicate runtime font spec: {line}")
    SPECS.add(spec)
LOCALES = ("jp", "kr", "sc", "tc")
CJK_ADVANCE_SAMPLES = {
    "sc": "设置正在播放中文",
    "tc": "設定正在播放中文",
    "jp": "設定再生日本語かなカナ",
    "kr": "설정재생한국어",
}
CJK_ADVANCE_UNITS = {"sc": 1000, "tc": 1000, "jp": 1000, "kr": 920}
CJK_UNITS_PER_EM = 1000
CONVTTF_DPI = 60


def rb12_glyph_width(data, codepoint):
    max_width = struct.unpack_from("<H", data, 4)[0]
    first, _, count = struct.unpack_from("<III", data, 12)
    bitmap_size, offset_count, width_count = struct.unpack_from(
        "<III", data, 24
    )
    if not first <= codepoint < first + count:
        raise ValueError(f"U+{codepoint:04X} is outside the RB12 range")
    if width_count == 0:
        return max_width

    offset_size = 4 if bitmap_size >= 0xFFDB else 2
    bitmap_end = 36 + bitmap_size
    table_start = ((bitmap_end + offset_size - 1) // offset_size) * offset_size
    width_start = table_start + offset_count * offset_size
    width_index = codepoint - first
    if width_index >= width_count or width_start + width_index >= len(data):
        raise ValueError(f"missing width for U+{codepoint:04X}")
    return data[width_start + width_index]

if len(sys.argv) != 2:
    raise SystemExit("usage: test-crazypod-runtime-font.py FONT_DIR")
font_dir = Path(sys.argv[1])

header = RUNTIME_HEADER.read_text(encoding="ascii")
source = RUNTIME_SOURCE.read_text(encoding="ascii")
scene_source = SCENE_SOURCE.read_text(encoding="ascii")
scene_renderer = SCENE_RENDERER.read_text(encoding="ascii")
for family in ("SYSTEM", "SERIF", "MONO"):
    if f"CRAZYPOD_FONT_FAMILY_{family}" not in header:
        raise SystemExit(f"runtime font family {family} is missing")
if "crazypod_runtime_font_resolve" not in header:
    raise SystemExit("runtime font resolver contract is missing")
if "crazypod_runtime_font_last_error" not in header:
    raise SystemExit("runtime font error contract is missing")
for property_name in ("size", "weight", "line_height"):
    if not re.search(rf"\bunsigned {property_name}\b", header):
        raise SystemExit(f"runtime font resolver lacks {property_name}")
if "enum crazypod_font_style style" not in header:
    raise SystemExit("runtime font resolver lacks style")
if 'FONT_DIR "/crazypod-aot/' not in source:
    raise SystemExit("runtime resolver does not use the shared AOT font store")
if "lv_tiny_ttf" in source:
    raise SystemExit("runtime resolver must not rasterize outlines on-device")
for reason in (
        "invalid font request", "font slots exhausted", "font load failed"):
    if reason not in source:
        raise SystemExit(f"runtime resolver does not record {reason}")
if "return font != NULL ? font : LV_FONT_DEFAULT" in scene_renderer:
    raise SystemExit("semantic fonts must not silently fall back to 12px")
if not re.search(
        r"if\(property == CP_UI_PROP_FONT\)\s+"
        r"return crazypod_miniapp_scene_text_font_apply",
        scene_source):
    raise SystemExit("font load failure does not propagate through set_i32")

expected = {
    f"{locale}-{family}-{weight}-{size}.fnt"
    for family, weight, size in SPECS
    for locale in LOCALES
}
actual = {path.name for path in font_dir.glob("*.fnt")}
if actual != expected:
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    raise SystemExit(
        f"AOT font set mismatch; missing={missing}, unexpected={unexpected}"
    )

metrics = {}
for path in sorted(font_dir.glob("*.fnt")):
    data = path.read_bytes()
    if len(data) < 36 or data[:4] != b"RB12":
        raise SystemExit(f"invalid RB12 font {path.name}")
    max_width, height, ascent, depth = struct.unpack_from("<HHHH", data, 4)
    first, default, count = struct.unpack_from("<III", data, 12)
    if not (0 < max_width <= 128 and 0 < height <= 64 and
            ascent <= height and depth <= 1 and first == 32 and
            default >= first and default < first + count and
            first + count >= 0xFF00):
        raise SystemExit(f"invalid RB12 metrics or BMP range in {path.name}")
    locale, family, weight, size_ext = path.name.split("-", 3)
    size = int(size_ext.removesuffix(".fnt"))
    key = (family, int(weight), size)
    metrics.setdefault(key, set()).add((max_width, height, ascent, depth))

for key, regional_metrics in metrics.items():
    if len(regional_metrics) != 1:
        raise SystemExit(
            f"regional fonts for {key} do not share the same line metrics: "
            f"{sorted(regional_metrics)}"
        )

# These pinned Noto faces use a 1000-unit em.  CJK ideographs/kana advance by
# 1000 units and Hangul by 920.  A stored width below the scaled advance means
# convttf has discarded the font's side bearings.  One extra pixel is allowed
# where hinted ink overhangs the nominal cell and must not be clipped.
for size in sorted(spec_size for family, weight, spec_size in SPECS
                   if family == "system" and weight == 400):
    for locale, sample in CJK_ADVANCE_SAMPLES.items():
        path = font_dir / f"{locale}-system-400-{size}.fnt"
        data = path.read_bytes()
        widths = {char: rb12_glyph_width(data, ord(char)) for char in sample}
        numerator = size * CONVTTF_DPI * CJK_ADVANCE_UNITS[locale]
        denominator = 72 * CJK_UNITS_PER_EM
        nominal_advance = (numerator + denominator // 2) // denominator
        if any(not nominal_advance <= width <= nominal_advance + 1
               for width in widths.values()):
            raise SystemExit(
                f"{path.name} does not preserve CJK advances "
                f"({nominal_advance}px nominal): {widths}"
            )
        space_width = rb12_glyph_width(data, ord(" "))
        if not 0 < space_width < min(widths.values()):
            raise SystemExit(
                f"{path.name} has invalid space/CJK advances: "
                f"space={space_width}, CJK={widths}"
            )

for license_name in ("OFL-Noto-CJK.txt", "SOURCE"):
    if not (font_dir / license_name).is_file():
        raise SystemExit(f"missing {license_name}")

print(f"Noto AOT runtime fonts: 3 semantic families, {len(SPECS)} tuples, "
      "4 regional faces, identical regional line metrics")
