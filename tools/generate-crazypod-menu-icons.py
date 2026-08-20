#!/usr/bin/env python3
"""Generate the 14 px CrazyPod menu icon A8 atlas.

The icon IDs were selected with better-icons. The source collection is
Google Material Symbols (Apache-2.0):
https://github.com/google/material-design-icons
"""

from __future__ import annotations

import io
import json
import sys
import urllib.parse
import urllib.request
import ctypes.util
from pathlib import Path

from PIL import Image

# Xcode's Python does not include Homebrew in ctypes' lookup path.
_find_library = ctypes.util.find_library
_homebrew_cairo = Path("/opt/homebrew/lib/libcairo.2.dylib")
if _homebrew_cairo.exists():
    ctypes.util.find_library = lambda name: (
        str(_homebrew_cairo)
        if name in ("cairo", "cairo-2", "libcairo-2")
        else _find_library(name)
    )

try:
    import cairosvg
except ImportError as error:
    raise SystemExit(
        "CairoSVG is required: python3 -m pip install cairosvg"
    ) from error


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = (
    ROOT
    / "apps/crazypod/ui/presentation/crazypod_menu_icon_data.inc"
)
ASSET_SOURCE = (
    ROOT
    / "apps/crazypod/ui/presentation/crazypod_menu_icon_assets.c"
)
COLLECTION = "material-symbols"
SOURCE_SIZE = 56
OUTPUT_SIZE = 14

# Keep this in exactly the same order as enum crazypod_menu_icon, excluding
# NONE. Similar meanings intentionally reuse a semantic icon at the route
# mapping layer rather than duplicating bitmap data here.
ICONS = {
    "MUSIC": "music-note-rounded",
    "NOW_PLAYING": "play-circle-rounded",
    "ALBUM_FLOW": "view-carousel",
    "MUSIC_LIBRARY": "library-music-rounded",
    "PLAYLIST": "queue-music-rounded",
    "ARTIST": "artist-rounded",
    "ALBUM": "album",
    "SONG": "audio-file-rounded",
    "SEARCH": "search-rounded",
    "QUEUE": "playlist-play-rounded",
    "PODCAST": "podcasts-rounded",
    "PHOTO": "photo-library-outline-rounded",
    "VIDEO": "video-library-rounded",
    "FAVORITE": "favorite-rounded",
    "TRASH": "delete-rounded",
    "SOUND": "volume-up-rounded",
    "DISPLAY": "display-settings-rounded",
    "PLAYBACK": "play-circle-rounded",
    "POWER": "power-settings-new-rounded",
    "CONTROLS": "tune-rounded",
    "MENU": "menu-rounded",
    "LANGUAGE": "language",
    "EQUALIZER": "graphic-eq-rounded",
    "BASS": "equalizer-rounded",
    "TREBLE": "multiline-chart-rounded",
    "BALANCE": "balance-rounded",
    "BRIGHTNESS": "brightness-6-rounded",
    "BACKLIGHT": "light-mode-rounded",
    "CHARGING": "battery-charging-full-rounded",
    "DISPLAY_SLEEP": "bedtime-rounded",
    "MOTION_OFF": "visibility-off-rounded",
    "SHUFFLE": "shuffle-rounded",
    "REPEAT": "repeat-rounded",
    "POWER_TIMER": "settings-power-rounded",
    "USB": "usb-rounded",
    "STORAGE": "hard-drive",
    "SLEEP_TIMER": "bedtime-rounded",
    "TIMER_BOOT": "alarm-on-rounded",
    "RESET_TIMER": "restart-alt-rounded",
    "BEEP": "notifications-active-rounded",
    "KEYCLICK": "touch-app-rounded",
    "SPEAKER": "speaker",
    "REPEAT_CLICKS": "repeat-on-rounded",
    "APPS": "apps",
    "LOCK": "lock",
    "CUSTOMIZE": "palette",
    "WORKOUT": "fitness-center-rounded",
    "BOOK": "menu-book-rounded",
    "NOTE": "edit-note-rounded",
    "CLOCK": "schedule-rounded",
    "CONTACT": "contacts-rounded",
    "CALENDAR": "calendar-month-rounded",
    "STOPWATCH": "timer-rounded",
    "MORE": "grid-view-rounded",
    "SETTINGS": "settings-rounded",
    "VISIBILITY": "visibility-rounded",
    "MOVE_UP": "arrow-upward-rounded",
    "MOVE_DOWN": "arrow-downward-rounded",
    "ADD_NOTE": "note-add-rounded",
    "DRAFT": "draft-rounded",
    "PIN": "keep-rounded",
    "DUPLICATE": "content-copy-rounded",
    "RESTORE": "restore-from-trash-rounded",
    "ERASE": "delete-forever-rounded",
    "RECENTS": "history-rounded",
    "READING": "auto-stories-rounded",
    "STATS": "monitoring-rounded",
    "BOOKMARK": "bookmark-rounded",
    "CHAPTERS": "account-tree-rounded",
    "TEXT_SIZE": "format-size-rounded",
    "PAGE_THEME": "colors-rounded",
    "IMPORT": "sync-rounded",
    "START": "play-arrow-rounded",
    "HISTORY": "history-rounded",
    "SUMMARY": "monitoring-rounded",
    "RUN": "directions-run-rounded",
    "WALK": "directions-walk-rounded",
    "CYCLING": "directions-bike-rounded",
    "HIKING": "hiking-rounded",
    "STAIRS": "stairs-rounded",
    "ROWING": "rowing-rounded",
    "STRENGTH": "exercise",
    "YOGA": "self-improvement-rounded",
    "TENNIS": "sports-tennis-rounded",
    "BASKETBALL": "sports-basketball",
    "SOCCER": "sports-soccer",
    "COOLDOWN": "ac-unit-rounded",
    "TODAY": "calendar-today-rounded",
    "UPCOMING": "upcoming-rounded",
    "MONTH": "calendar-view-month",
    "ADD_EVENT": "event-available-rounded",
    "EVENT": "event-rounded",
    "TITLE": "title-rounded",
    "DATE": "calendar-month-rounded",
    "TIME": "schedule-rounded",
    "SAVE": "save-rounded",
    "PRESETS": "bookmark-manager-rounded",
    "ICONS": "shapes-rounded",
    "DETAILS": "tune-rounded",
    "BACKGROUNDS": "wallpaper-rounded",
    "THEMES": "auto-awesome-rounded",
    "LAYOUT": "dashboard-customize-rounded",
    "EXPORT": "upload-file-rounded",
    "EDIT": "edit-rounded",
    "APPLY": "check-circle-rounded",
    "ICON_SIZE": "resize-rounded",
    "WAVE": "graphic-eq-rounded",
    "GLOW": "flare-rounded",
    "HIGHLIGHT": "ink-highlighter-rounded",
    "PRIMARY_COLOR": "format-color-fill-rounded",
    "SECONDARY_COLOR": "colors-rounded",
    "HOME": "home-rounded",
    "TOP": "vertical-align-top-rounded",
    "BOTTOM": "vertical-align-bottom-rounded",
    "WALLPAPER": "wallpaper-rounded",
    "CHECK": "check-rounded",
}


def fetch_collection() -> dict[str, object]:
    names = ",".join(ICONS.values())
    query = urllib.parse.urlencode({"icons": names})
    url = f"https://api.iconify.design/{COLLECTION}.json?{query}"
    request = urllib.request.Request(
        url, headers={"User-Agent": "CrazyPod-menu-icon-generator/1.0"}
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def render_alpha(body: str, width: int, height: int) -> bytes:
    svg = (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{SOURCE_SIZE}" '
        f'height="{SOURCE_SIZE}" viewBox="0 0 {width} {height}">'
        f'<g fill="white">{body}</g></svg>'
    )
    png = cairosvg.svg2png(
        bytestring=svg.encode("utf-8"),
        output_width=SOURCE_SIZE,
        output_height=SOURCE_SIZE,
    )
    image = Image.open(io.BytesIO(png)).convert("RGBA")
    alpha = image.getchannel("A").resize(
        (OUTPUT_SIZE, OUTPUT_SIZE), Image.Resampling.LANCZOS
    )
    return bytes(alpha.getdata())


def format_bytes(data: bytes) -> str:
    lines = []
    for offset in range(0, len(data), 14):
        values = ", ".join(f"0x{value:02x}" for value in data[offset:offset + 14])
        lines.append(f"    {values},")
    return "\n".join(lines)


def generate() -> str:
    collection = fetch_collection()
    source_icons = collection.get("icons", {})
    default_width = int(collection.get("width", 24))
    default_height = int(collection.get("height", 24))
    missing = sorted(set(ICONS.values()) - set(source_icons))
    if missing:
        raise RuntimeError("Missing Iconify icons: " + ", ".join(missing))

    chunks = [
        "/* Auto-generated by tools/generate-crazypod-menu-icons.py. */",
        "/* Material Symbols is licensed under Apache-2.0. */",
        "",
    ]
    asset_names = []
    source_assets = {}
    for semantic, source_name in ICONS.items():
        if source_name in source_assets:
            asset_names.append((semantic, source_assets[source_name]))
            chunks.extend([
                f"/* CRAZYPOD_MENU_ICON_{semantic} reuses "
                f"{COLLECTION}:{source_name}. */",
                "",
            ])
            continue
        icon = source_icons[source_name]
        width = int(icon.get("width", default_width))
        height = int(icon.get("height", default_height))
        data = render_alpha(str(icon["body"]), width, height)
        symbol = semantic.lower()
        source_assets[source_name] = symbol
        asset_names.append((semantic, symbol))
        chunks.extend([
            f"/* {COLLECTION}:{source_name} */",
            f"static LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST",
            f"const uint8_t menu_icon_{symbol}_data[] = {{",
            format_bytes(data),
            "};",
            f"static const lv_image_dsc_t menu_icon_{symbol} = {{",
            "    .header.magic = LV_IMAGE_HEADER_MAGIC,",
            "    .header.cf = LV_COLOR_FORMAT_A8,",
            f"    .header.w = {OUTPUT_SIZE},",
            f"    .header.h = {OUTPUT_SIZE},",
            f"    .header.stride = {OUTPUT_SIZE},",
            f"    .data_size = sizeof(menu_icon_{symbol}_data),",
            f"    .data = menu_icon_{symbol}_data,",
            "};",
            "",
        ])

    chunks.extend([
        "static const lv_image_dsc_t *const",
        "crazypod_menu_icon_assets[CRAZYPOD_MENU_ICON_COUNT] = {",
    ])
    for semantic, symbol in asset_names:
        chunks.append(
            f"    [CRAZYPOD_MENU_ICON_{semantic}] = &menu_icon_{symbol},"
        )
    chunks.extend(["};", ""])
    return "\n".join(chunks)


def main() -> int:
    try:
        generated = generate()
    except Exception as error:
        print(f"menu icon generation failed: {error}", file=sys.stderr)
        return 1
    OUTPUT.write_text(generated, encoding="utf-8")
    # Rockbox's incremental build does not track included .inc files.
    ASSET_SOURCE.touch()
    print(f"generated {len(ICONS)} menu icons in {OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
