#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
LV_CONF = ROOT / "lib/lvgl/lv_conf.h"

SCREEN_WIDTH = 320
SCREEN_HEIGHT = 240
TRANSFORM_MARGIN = 5
ARGB_BYTES_PER_PIXEL = 4
MINIMUM_TRANSFORM_BUDGET = (
    (SCREEN_WIDTH + 2 * TRANSFORM_MARGIN)
    * (SCREEN_HEIGHT + 2 * TRANSFORM_MARGIN)
    * ARGB_BYTES_PER_PIXEL
)

source = LV_CONF.read_text(encoding="ascii")
match = re.search(
    r"#define\s+LV_DRAW_LAYER_MAX_MEMORY\s+\\\s*\n"
    r"\s*\(\(320U\s*\+\s*10U\)\s*\*\s*"
    r"\(240U\s*\+\s*10U\)\s*\*\s*4U\)",
    source,
)
if match is None:
    raise SystemExit(
        "LV_DRAW_LAYER_MAX_MEMORY must cover a full 320x240 ARGB transform "
        "layer including LVGL's transform margin"
    )

configured_budget = (320 + 10) * (240 + 10) * 4
if configured_budget < MINIMUM_TRANSFORM_BUDGET:
    raise SystemExit(
        f"transform layer budget {configured_budget} is below required "
        f"{MINIMUM_TRANSFORM_BUDGET}"
    )

print(
    "LVGL transform layer budget: "
    f"{configured_budget} bytes covers full-screen ARGB rendering"
)
