#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

# Compile the production volume path with captured hardware calls.
python3 - "$repo_root" "$test_root" <<'PYCODE'
from pathlib import Path
import sys
source = (Path(sys.argv[1]) / "firmware/sound.c").read_text()
start = source.index("static void set_prescaled_volume(void)")
end = source.index("\n}", start) + 2
(Path(sys.argv[2]) / "sound_volume_under_test.c").write_text(source[start:end])
PYCODE
cc -std=c99 -Wall -Wextra -Werror -I"$test_root" \
    "$repo_root/tests/crazypod_dock_volume_host_test.c" \
    -o "$test_root/dock-volume-test"
"$test_root/dock-volume-test"
printf 'Dock volume tests passed\n'
