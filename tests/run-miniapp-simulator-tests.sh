#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

sh "$repo_root/tests/run-miniapp-native-aot-simulator-test.sh"
sh "$repo_root/tests/run-miniapp-e2e-reproduction.sh"
