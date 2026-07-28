#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/apps/crazypod/ui" \
    "$repo_root/apps/crazypod/ui/features/organizer/crazypod_calendar_model.c" \
    "$repo_root/apps/crazypod/ui/presentation/crazypod_ui_menu_layout.c" \
    "$repo_root/apps/crazypod/ui/presentation/crazypod_ui_text.c" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_feature_dispatcher.c" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_navigation_command.c" \
    "$repo_root/apps/crazypod/ui/navigation/crazypod_route_registry.c" \
    "$repo_root/tests/crazypod_ui_pure_host_test.c" \
    -o "$test_root/crazypod_ui_pure_host_test"

"$test_root/crazypod_ui_pure_host_test"
