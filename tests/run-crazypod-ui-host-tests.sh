#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/apps/crazypod" \
    -I"$repo_root/apps/crazypod/ui" \
    "$repo_root/apps/crazypod/crazypod_collation.c" \
    "$repo_root/apps/crazypod/ui/features/organizer/crazypod_calendar_model.c" \
    "$repo_root/apps/crazypod/ui/presentation/crazypod_ui_menu_layout.c" \
    "$repo_root/apps/crazypod/ui/presentation/crazypod_ui_text.c" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_alpha_jump.c" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_feature_dispatcher.c" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_navigation_command.c" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_route_registry.c" \
    "$repo_root/tests/crazypod_ui_pure_host_test.c" \
    -o "$test_root/crazypod_ui_pure_host_test"

"$test_root/crazypod_ui_pure_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/crazypod-frameclock-stubs" \
    -I"$repo_root/apps/crazypod" \
    -I"$repo_root/apps/crazypod/ui" \
    "$repo_root/apps/crazypod/crazypod_apps.c" \
    "$repo_root/apps/crazypod/ui/features/settings/crazypod_settings_controller.c" \
    "$repo_root/tests/crazypod_settings_reorder_host_test.c" \
    -o "$test_root/crazypod_settings_reorder_host_test"

"$test_root/crazypod_settings_reorder_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/crazypod-lyrics-stubs" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/crazypod_lyrics.c" \
    "$repo_root/tests/crazypod_lyrics_host_test.c" \
    -o "$test_root/crazypod_lyrics_host_test"

"$test_root/crazypod_lyrics_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/crazypod-home-input-stubs" \
    -I"$repo_root/apps/crazypod/ui" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_input_event.c" \
    "$repo_root/apps/crazypod/ui/shell/crazypod_home_input.c" \
    "$repo_root/tests/crazypod_home_input_host_test.c" \
    -o "$test_root/crazypod_home_input_host_test"

"$test_root/crazypod_home_input_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/apps/crazypod/ui" \
    "$repo_root/apps/crazypod/ui/app/crazypod_screen_off_policy.c" \
    "$repo_root/tests/crazypod_screen_off_policy_host_test.c" \
    -o "$test_root/crazypod_screen_off_policy_host_test"

"$test_root/crazypod_screen_off_policy_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/crazypod-playlist-stubs" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/tests/crazypod_core_alloc_host_stub.c" \
    "$repo_root/apps/crazypod/crazypod_playlist.c" \
    "$repo_root/tests/crazypod_playlist_host_test.c" \
    -o "$test_root/crazypod_playlist_host_test"

"$test_root/crazypod_playlist_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/crazypod-playlist-stubs" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/tests/crazypod_core_alloc_host_stub.c" \
    "$repo_root/apps/crazypod/crazypod_music_storage.c" \
    "$repo_root/tests/crazypod_music_storage_host_test.c" \
    -o "$test_root/crazypod_music_storage_host_test"

"$test_root/crazypod_music_storage_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/crazypod-frameclock-stubs" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/crazypod_frameclock.c" \
    "$repo_root/tests/crazypod_frameclock_host_test.c" \
    -o "$test_root/crazypod_frameclock_host_test"

"$test_root/crazypod_frameclock_host_test"

python3 "$repo_root/tests/test-crazypod-menu-icons.py"
