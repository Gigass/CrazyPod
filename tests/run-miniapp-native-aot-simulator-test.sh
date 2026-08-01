#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
simulator="$repo_root/build-sim/rockboxui"
build="$repo_root/build-sim"

if [ ! -x "$simulator" ]; then
    echo "Error: build the simulator before running Native AOT tests." >&2
    exit 2
fi

python3 - "$simulator" "$build" <<'PY'
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

simulator = Path(sys.argv[1])
build = Path(sys.argv[2])
simdisk = build / "simdisk"


def run(screen):
    before = set(simdisk.glob("dump *.bmp"))
    environment = os.environ.copy()
    environment.update({
        "SDL_AUDIODRIVER": "dummy",
        "CRAZYPOD_SIM_DUMP": "1",
        "CRAZYPOD_SIM_EXIT_AFTER_DUMP": "1",
        "CRAZYPOD_SIM_DUMP_SETTLE_MS": "500",
        "CRAZYPOD_SIM_SCREEN": screen,
    })
    result = subprocess.run(
        [str(simulator)],
        cwd=build,
        env=environment,
        timeout=20,
        check=False,
    )
    if result.returncode != 0:
        raise SystemExit(
            f"Native AOT simulator exited with {result.returncode}"
        )
    created = set(simdisk.glob("dump *.bmp")) - before
    if len(created) != 1:
        raise SystemExit(
            f"expected one Native AOT dump, found {len(created)}"
        )
    return created.pop()


initial = run("native-reference")
clicked = run("native-reference-clicked")
initial_hash = hashlib.sha256(initial.read_bytes()).hexdigest()
clicked_hash = hashlib.sha256(clicked.read_bytes()).hexdigest()
if initial_hash == clicked_hash:
    raise SystemExit(
        "Native AOT event did not change the rendered framebuffer"
    )

required = {
    "manifest.json",
    "app.dylib",
    "profile.bin",
    "assets.bin",
    "icon.bin",
    ".install.bin",
}
for app in ("native-reference", "game2048", "capability-lab"):
    installed = simdisk / ".crazypod/miniapps" / app
    actual = {item.name for item in installed.iterdir()}
    missing = required - actual
    if missing:
        raise SystemExit(
            f"{app} Native AOT installation misses {sorted(missing)}"
        )
    manifest = json.loads(
        (installed / "manifest.json").read_text(encoding="utf-8")
    )
    if (
        manifest["format"] != 5
        or manifest["runtime"] != "native-aot"
        or manifest["abiMajor"] != 1
        or manifest["entry"] != "app.dylib"
    ):
        raise SystemExit(f"{app} installed an invalid Native profile")
    for forbidden in ("app.js", "app.qbc", "styles.bin"):
        if (installed / forbidden).exists():
            raise SystemExit(
                f"{app} unexpectedly contains {forbidden}"
            )
print(
    "CrazyPod Native AOT simulator path verified: "
    "CPK5 install, app.dylib load, direct UI event and rerender"
)
PY
