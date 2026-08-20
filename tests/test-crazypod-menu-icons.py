#!/usr/bin/env python3
"""Structural and generated-data checks for CrazyPod menu row icons."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "apps/crazypod/ui"
HEADER = UI / "crazypod_menu_icon.h"
DATA = UI / "presentation/crazypod_menu_icon_data.inc"


def enum_names() -> list[str]:
    text = HEADER.read_text(encoding="utf-8")
    body = re.search(
        r"enum crazypod_menu_icon\s*\{(.*?)\};", text, re.S
    )
    assert body is not None
    return re.findall(r"CRAZYPOD_MENU_ICON_([A-Z0-9_]+)", body.group(1))


def test_generated_assets() -> None:
    names = enum_names()
    assert names[0] == "NONE"
    assert names[-1] == "COUNT"
    expected = names[1:-1]
    text = DATA.read_text(encoding="utf-8")
    arrays = re.findall(
        r"const uint8_t menu_icon_([a-z0-9_]+)_data\[\] = \{"
        r"(.*?)\n\};",
        text,
        re.S,
    )
    assert 90 <= len(arrays) <= len(expected)
    generated_symbols = {name for name, _ in arrays}
    for name, values_text in arrays:
        values = re.findall(r"0x([0-9a-f]{2})", values_text)
        assert len(values) == 14 * 14, name
        alpha = [int(value, 16) for value in values]
        assert max(alpha) >= 64, name
        assert sum(value > 0 for value in alpha) >= 12, name

    table = re.findall(
        r"\[CRAZYPOD_MENU_ICON_([A-Z0-9_]+)\]\s*=\s*"
        r"&menu_icon_([a-z0-9_]+)",
        text,
    )
    assert [name for name, _ in table] == expected
    assert all(symbol in generated_symbols for _, symbol in table)
    assert len(set(symbol for _, symbol in table)) == len(arrays)


def test_semantic_route_coverage() -> None:
    mapping_bodies = []
    for path in (UI / "features").glob("*/crazypod_*_feature.c"):
        text = path.read_text(encoding="utf-8")
        marker = "_feature_item_icon("
        start = text.find(marker)
        if start < 0:
            continue
        brace = text.find("{", start)
        assert brace >= 0, path
        depth = 0
        for end in range(brace, len(text)):
            if text[end] == "{":
                depth += 1
            elif text[end] == "}":
                depth -= 1
                if depth == 0:
                    mapping_bodies.append(text[brace:end + 1])
                    break
        else:
            raise AssertionError(f"unterminated icon mapper: {path}")
    mappings = "\n".join(mapping_bodies)
    required_routes = {
        # Music and queue lists.
        "MUSIC_ROUTE_MENU", "MUSIC_ROUTE_ALBUM_FLOW", "MUSIC_ROUTE_ALL",
        "MUSIC_ROUTE_PLAYLISTS", "MUSIC_ROUTE_PLAYLIST_SONGS",
        "MUSIC_ROUTE_ARTISTS", "MUSIC_ROUTE_ARTIST_SONGS",
        "MUSIC_ROUTE_ALBUMS", "MUSIC_ROUTE_ALBUM_SONGS",
        "MUSIC_ROUTE_SONGS", "MUSIC_ROUTE_SEARCH_RESULTS",
        "MUSIC_ROUTE_QUEUE", "PODCASTS_ROUTE_MENU",
        # Media lists.
        "PHOTOS_ROUTE_MENU", "PHOTOS_ROUTE_LIBRARY",
        "PHOTOS_ROUTE_VIDEOS", "PHOTOS_ROUTE_FAVORITES",
        "PHOTOS_ROUTE_DELETE_MENU", "PHOTOS_ROUTE_DELETE_PHOTOS",
        "PHOTOS_ROUTE_DELETE_VIDEOS",
        # Settings lists.
        "SETTINGS_ROUTE_MENU", "SETTINGS_ROUTE_SOUND",
        "SETTINGS_ROUTE_DISPLAY", "SETTINGS_ROUTE_PLAYBACK",
        "SETTINGS_ROUTE_POWER", "SETTINGS_ROUTE_CONTROLS",
        "SETTINGS_ROUTE_MAIN_MENU", "SETTINGS_ROUTE_MAIN_MENU_ACTIONS",
        # Notes and books lists.
        "NOTES_ROUTE_MENU", "NOTES_ROUTE_EXIT_ACTIONS",
        "NOTES_ROUTE_SEARCH_RESULTS", "NOTES_ROUTE_ACTIONS",
        "NOTES_ROUTE_DELETED", "NOTES_ROUTE_DELETED_ACTIONS",
        "BOOKS_ROUTE_MENU", "BOOKS_ROUTE_RECENTS", "BOOKS_ROUTE_LIBRARY",
        "BOOKS_ROUTE_FAVORITES", "BOOKS_ROUTE_ACTIONS",
        "BOOKS_ROUTE_CHAPTERS", "BOOKS_ROUTE_BOOKMARKS",
        "BOOKS_ROUTE_READING_SETTINGS",
        # Organizer lists.
        "CLOCK_ROUTE_MENU", "CLOCK_ROUTE_SLEEP_TIMER",
        "WORKOUT_ROUTE_MENU", "WORKOUT_ROUTE_TYPES",
        "WORKOUT_ROUTE_HISTORY", "CALENDAR_ROUTE_MENU",
        "CALENDAR_ROUTE_TODAY", "CALENDAR_ROUTE_UPCOMING",
        "CALENDAR_ROUTE_DAY_EVENTS", "CALENDAR_ROUTE_EDITOR",
        "CALENDAR_ROUTE_ACTIONS", "CONTACTS_ROUTE_LIST",
        # Customize and Mini Apps lists.
        "DIY_ROUTE_MENU", "DIY_ROUTE_PRESETS", "DIY_ROUTE_PRESET_LIBRARY",
        "DIY_ROUTE_PRESET_ACTIONS", "DIY_ROUTE_PRESET_EDIT",
        "DIY_ROUTE_ICONS", "DIY_ROUTE_DETAILS", "DIY_ROUTE_CHOICES",
        "DIY_ROUTE_BACKGROUNDS", "DIY_ROUTE_BACKGROUND_CHOICES",
        "DIY_ROUTE_LAYOUT", "DIY_ROUTE_NOW_PLAYING_THEMES",
        "DIY_ROUTE_HEADPHONE_POPUP",
        "UTILITIES_ROUTE_MENU",
    }
    missing = sorted(route for route in required_routes if route not in mappings)
    assert not missing, f"semantic routes missing icon mappings: {missing}"


def test_renderer_has_no_route_fallback() -> None:
    screen = (UI / "presentation/crazypod_menu_screen.c").read_text()
    rows = (UI / "app/crazypod_menu_rows.c").read_text()
    for forbidden in (
        "music_menu_symbols", "photos_menu_symbols", "music_symbols",
        "photo_symbols", "miniapp_symbol", "route_app(",
        "crazypod_customize_feature_menu_symbol",
        "crazypod_settings_feature_menu_symbol",
    ):
        assert forbidden not in screen
        assert forbidden not in rows
    assert "crazypod_route_query_item_icon(state, index)" in screen
    assert "crazypod_route_query_item_icon(state, index)" in rows


if __name__ == "__main__":
    test_generated_assets()
    test_semantic_route_coverage()
    test_renderer_has_no_route_fallback()
    print("CrazyPod menu icon checks passed")
