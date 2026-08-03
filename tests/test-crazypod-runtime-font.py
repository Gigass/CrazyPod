#!/usr/bin/env python3

from pathlib import Path
import re
import struct
import sys


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_HEADER = ROOT / "apps/crazypod/crazypod_runtime_font.h"
RUNTIME_SOURCE = ROOT / "apps/crazypod/crazypod_runtime_font.c"
SCENE_SOURCE = ROOT / "apps/crazypod/ui/features/miniapps/crazypod_miniapp_scene.c"
SCENE_RENDERER = ROOT / (
    "apps/crazypod/ui/features/miniapps/"
    "crazypod_miniapp_scene_renderer.c"
)

SPECS = {
    *(('system', 400, size) for size in
      (6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 22, 24, 28, 32, 40)),
    *(('system', weight, size) for weight, size in
      ((500, 32), (700, 16), (700, 32), (900, 32))),
    *(('serif', 400, size) for size in (11, 12, 14, 16, 28)),
    *(('serif', weight, size) for weight, size in
      ((700, 14), (700, 16), (700, 28), (900, 22))),
    *(('mono', 400, size) for size in (7, 8, 12, 16)),
    *(('mono', 700, size) for size in (8, 14, 22)),
}
LOCALES = ("jp", "kr", "sc", "tc")

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

for license_name in ("OFL-Noto-CJK.txt", "SOURCE"):
    if not (font_dir / license_name).is_file():
        raise SystemExit(f"missing {license_name}")

print("Noto AOT runtime fonts: 3 semantic families, 35 tuples, "
      "4 regional faces, identical regional line metrics")
