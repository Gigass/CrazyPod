#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

node "$repo_root/tools/miniapp-builder/node_modules/typescript/bin/tsc" \
    --project "$repo_root/miniapps/tsconfig.native.json"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/miniapps/sdk" \
    "$repo_root/tests/crazypod_miniapp_native_abi_host_test.c" \
    -o "$test_root/crazypod_miniapp_native_abi_host_test"

"$test_root/crazypod_miniapp_native_abi_host_test"

for app in game2048 capability-lab; do
    node "$repo_root/tools/miniapp-builder/src/cli.mjs" generate \
        "$repo_root/miniapps/apps/$app" \
        --out "$test_root/$app.c"
    cc -std=c99 -Wall -Wextra -Werror \
        -I"$repo_root/miniapps/sdk" \
        -c "$test_root/$app.c" \
        -o "$test_root/$app.o"
    node "$repo_root/tools/miniapp-builder/src/cli.mjs" build \
        "$repo_root/miniapps/apps/$app" \
        --target simulator \
        --binary "$test_root/$app.o" \
        --out "$test_root/$app.cpk"
done

cc -std=c99 -Wall -Wextra -Werror \
    -I"$test_root" \
    -I"$repo_root/miniapps/sdk" \
    "$repo_root/tests/game2048_native_engine_host_test.c" \
    -o "$test_root/game2048_native_engine_host_test"

"$test_root/game2048_native_engine_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/runtime" \
    -I"$repo_root/apps/crazypod/miniapps/catalog" \
    -I"$repo_root/apps/crazypod/miniapps/files" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/crazypod_miniapps.c" \
    "$repo_root/tests/crazypod_miniapps_lifecycle_host_test.c" \
    -o "$test_root/crazypod_miniapps_lifecycle_host_test"

"$test_root/crazypod_miniapps_lifecycle_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -D_DARWIN_C_SOURCE \
    -DMINIAPP_SYSTEM_ROOT="\"$test_root/storage\"" \
    -DMINIAPP_DATA_ROOT="\"$test_root/storage/miniapp-data\"" \
    -DMINIAPP_USER_ROOT="\"$test_root/user\"" \
    -DMINIAPP_EXPORT_PARENT="\"$test_root/user\"" \
    -DMINIAPP_EXPORT_ROOT="\"$test_root/user/Export\"" \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/crazypod_miniapp_storage.c" \
    "$repo_root/tests/crazypod_miniapp_storage_host_test.c" \
    -o "$test_root/crazypod_miniapp_storage_host_test"

"$test_root/crazypod_miniapp_storage_host_test"

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod/miniapps/catalog" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_manifest.c" \
    "$repo_root/apps/crazypod/miniapps/catalog/crazypod_miniapp_catalog.c" \
    "$repo_root/tests/crazypod_miniapp_manifest_host_test.c" \
    -o "$test_root/crazypod_miniapp_manifest_host_test"

"$test_root/crazypod_miniapp_manifest_host_test"

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

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_reader.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_verifier.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_resource_validator.c" \
    "$repo_root/tests/crazypod_cpk_reader_host_test.c" \
    -o "$test_root/crazypod_cpk_reader_host_test"

"$test_root/crazypod_cpk_reader_host_test" "$test_root/game2048.cpk"

node "$repo_root/tools/miniapp-builder/src/cli.mjs" generate \
    "$repo_root/miniapps/apps/native-reference" \
    --out "$test_root/native-reference.c"
cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/miniapps/sdk" \
    -c "$test_root/native-reference.c" \
    -o "$test_root/native-reference.o"
node "$repo_root/tools/miniapp-builder/src/cli.mjs" build \
    "$repo_root/miniapps/apps/native-reference" \
    --target simulator \
    --binary "$test_root/native-reference.o" \
    --out "$test_root/native-reference.cpk"
"$test_root/crazypod_cpk_reader_host_test" \
    "$test_root/native-reference.cpk"

mkdir "$test_root/cpk"
python3 - "$test_root/game2048.cpk" "$test_root/cpk" <<'PY'
import pathlib
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as archive:
    archive.extractall(pathlib.Path(sys.argv[2]))
PY

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/runtime" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_reader.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_manifest.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_resource_validator.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_install_record.c" \
    "$repo_root/tests/crazypod_miniapp_install_record_host_test.c" \
    -o "$test_root/crazypod_miniapp_install_record_host_test"

"$test_root/crazypod_miniapp_install_record_host_test" \
    "$test_root/game2048.cpk" "$test_root/cpk"

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
"$test_root/crazypod_miniapp_installer_lifecycle_host_test" same

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
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/files" \
    -I"$repo_root/apps/crazypod/ui/features/miniapps" \
    -I"$repo_root/apps/crazypod/ui/navigation" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/crazypod_miniapp_input.c" \
    "$repo_root/apps/crazypod/ui/features/miniapps/crazypod_miniapp_runtime_controller.c" \
    "$repo_root/tests/crazypod_miniapp_runtime_controller_host_test.c" \
    -o "$test_root/crazypod_miniapp_runtime_controller_host_test"

"$test_root/crazypod_miniapp_runtime_controller_host_test"

python3 "$repo_root/tests/test_crazypod_miniapp_certify.py"
