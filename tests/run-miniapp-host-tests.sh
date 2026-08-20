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

cc -std=c99 -Wall -Wextra -Werror \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_sha256.c" \
    "$repo_root/tests/crazypod_sha256_host_test.c" \
    -o "$test_root/crazypod_sha256_host_test"

"$test_root/crazypod_sha256_host_test"

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

python3 - "$test_root/game2048.cpk" \
    "$test_root/game2048-signed.cpk" \
    "$test_root/trusted-miniapp-keys.txt" <<'PY'
import hashlib
import hmac
import json
import struct
import sys
import zipfile

source, output, trust_path = sys.argv[1:]
secret = bytes(range(32))
key_id = hashlib.sha256(secret).hexdigest()[:16]
with zipfile.ZipFile(source) as archive:
    entries = [(info.filename, archive.read(info.filename))
               for info in archive.infolist()]
manifest = json.loads(entries[0][1])
manifest["signingKeyId"] = key_id
manifest["signature"] = "0" * 64

def encoded_manifest(value):
    return json.dumps(value, ensure_ascii=False,
                      separators=(",", ":")).encode()

def frame(name, data):
    return name.encode() + b"\0" + struct.pack("<I", len(data)) + data

normalized = encoded_manifest(manifest)
message = b"CPK5-HMAC-SHA256-V1" + frame("manifest.json", normalized)
for name, data in entries[1:]:
    message += frame(name, data)
manifest["signature"] = hmac.new(secret, message, hashlib.sha256).hexdigest()
entries[0] = (entries[0][0], encoded_manifest(manifest))
with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
    for name, data in entries:
        archive.writestr(name, data)
with open(trust_path, "w", encoding="ascii") as trust:
    trust.write(f"{key_id}:{secret.hex()}\n")
PY

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
    -DTRUST_KEYS_PATH="\"$test_root/trusted-miniapp-keys.txt\"" \
    -DDEVELOPER_MODE_PATH="\"$test_root/developer-mode.flag\"" \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_reader.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_cpk_verifier.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_manifest.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_sha256.c" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_resource_validator.c" \
    "$repo_root/tests/crazypod_cpk_reader_host_test.c" \
    -o "$test_root/crazypod_cpk_reader_host_test"

"$test_root/crazypod_cpk_reader_host_test" "$test_root/game2048.cpk"
"$test_root/crazypod_cpk_reader_host_test" \
    "$test_root/game2048-signed.cpk"

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
    -DCRAZYPOD_MINIAPP_PACKAGE_INDEX_PATH='"package-index.bin"' \
    -DCRAZYPOD_MINIAPP_PACKAGE_INDEX_TEMP='"package-index.tmp"' \
    -I"$repo_root/tests/miniapp-host-stubs" \
    -I"$repo_root/apps/crazypod/miniapps/installer" \
    -I"$repo_root/apps/crazypod" \
    "$repo_root/apps/crazypod/miniapps/installer/crazypod_miniapp_package_index.c" \
    "$repo_root/tests/crazypod_miniapp_package_index_host_test.c" \
    -o "$test_root/crazypod_miniapp_package_index_host_test"

(
    cd "$test_root"
    ./crazypod_miniapp_package_index_host_test
)

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

"$test_root/crazypod_miniapp_installer_lifecycle_host_test" boot
"$test_root/crazypod_miniapp_installer_lifecycle_host_test" usb
"$test_root/crazypod_miniapp_installer_lifecycle_host_test" same
"$test_root/crazypod_miniapp_installer_lifecycle_host_test" identical
"$test_root/crazypod_miniapp_installer_lifecycle_host_test" incremental

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
