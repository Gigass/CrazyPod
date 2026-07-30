#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod/miniapps/catalog" \
    -I"$repo_root/apps/crazypod/miniapps/modal" \
    -I"$repo_root/apps/crazypod/miniapps/notification" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_manifest.c" \
    "$repo_root/apps/crazypod/miniapps/catalog/crazypod_miniapp_catalog.c" \
    "$repo_root/apps/crazypod/miniapps/modal/crazypod_miniapp_modal.c" \
    "$repo_root/apps/crazypod/miniapps/notification/crazypod_miniapp_notification.c" \
    "$repo_root/tests/crazypod_miniapp_manifest_host_test.c" \
    -o "$test_root/crazypod_miniapp_manifest_host_test"

"$test_root/crazypod_miniapp_manifest_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/apps/crazypod/miniapps/alarm" \
    -I"$repo_root/apps/crazypod/miniapps/catalog" \
    -I"$repo_root/apps/crazypod/miniapps" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/catalog/crazypod_miniapp_catalog.c" \
    "$repo_root/apps/crazypod/miniapps/alarm/crazypod_miniapp_alarm_service.c" \
    "$repo_root/tests/crazypod_miniapp_alarm_service_host_test.c" \
    -o "$test_root/crazypod_miniapp_alarm_service_host_test"

"$test_root/crazypod_miniapp_alarm_service_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -D_DARWIN_C_SOURCE \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/runtime" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/runtime/crazypod_miniapp_resource_host.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_resource_validator.c" \
    "$repo_root/tests/crazypod_miniapp_resource_host_test.c" \
    -o "$test_root/crazypod_miniapp_resource_host_test"

"$test_root/crazypod_miniapp_resource_host_test"

mkdir "$test_root/cpk"
printf x > "$test_root/cpk/manifest.ini"
printf x > "$test_root/cpk/app.dylib"
dd if=/dev/zero of="$test_root/cpk/icon.bmp" bs=102454 count=1 2>/dev/null
dd if=/dev/zero of="$test_root/cpk/signature.ed25519" bs=64 count=1 2>/dev/null
(
    cd "$test_root/cpk"
    zip -0 -X -q "$test_root/test.cpk" \
        manifest.ini app.dylib icon.bmp signature.ed25519
)

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_reader.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_verifier.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_resource_validator.c" \
    "$repo_root/tests/crazypod_cpk_reader_host_test.c" \
    -o "$test_root/crazypod_cpk_reader_host_test"

"$test_root/crazypod_cpk_reader_host_test" "$test_root/test.cpk"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_reader.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_manifest.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_resource_validator.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_install_record.c" \
    "$repo_root/tests/crazypod_miniapp_install_record_host_test.c" \
    -o "$test_root/crazypod_miniapp_install_record_host_test"

"$test_root/crazypod_miniapp_install_record_host_test" \
    "$test_root/test.cpk" "$test_root/cpk"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-installer-host-stubs" \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod/miniapps/catalog" \
    -I"$repo_root/apps/crazypod/miniapps/runtime" \
    -I"$repo_root/apps/crazypod/miniapps" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_installer.c" \
    "$repo_root/tests/crazypod_miniapp_installer_lifecycle_host_test.c" \
    -o "$test_root/crazypod_miniapp_installer_lifecycle_host_test"

"$test_root/crazypod_miniapp_installer_lifecycle_host_test" lazy
"$test_root/crazypod_miniapp_installer_lifecycle_host_test" usb

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/crazypod-miniapp-input-stubs" \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/ui/features/miniapps" \
    -I"$repo_root/apps/crazypod/ui/navigation" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/ui/features/miniapps/crazypod_miniapp_input.c" \
    "$repo_root/tests/crazypod_miniapp_input_host_test.c" \
    -o "$test_root/crazypod_miniapp_input_host_test"

"$test_root/crazypod_miniapp_input_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/miniapps/game2048" \
    "$repo_root/miniapps/game2048/engine.c" \
    "$repo_root/miniapps/tests/test_game2048.c" \
    -o "$test_root/game2048_test"

"$test_root/game2048_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/miniapps/sdk" \
    -I"$repo_root/miniapps/game2048" \
    "$repo_root/miniapps/game2048/app.c" \
    "$repo_root/miniapps/game2048/engine.c" \
    "$repo_root/miniapps/tests/test_game2048_app.c" \
    -o "$test_root/game2048_app_test"

"$test_root/game2048_app_test"
