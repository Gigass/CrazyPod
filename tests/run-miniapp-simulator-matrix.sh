#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
simulator="$repo_root/build-sim/rockboxui"

if [ ! -x "$simulator" ]; then
    echo "Error: build the simulator before running the matrix." >&2
    exit 2
fi

python3 - "$simulator" "$repo_root/build-sim" <<'PY'
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

simulator = Path(sys.argv[1])
build = Path(sys.argv[2])
simdisk = build / "simdisk"
output = build / "miniapp-matrix"
output.mkdir(exist_ok=True)

scenes = [
    "miniapps-list",
    "notes",
    "native-reference",
    "native-reference-clicked",
    "game2048",
    "game2048-exit-prompt",
    "game2048-exit-list",
    "game2048-exit-notes",
    "game2048-game",
    "game2048-moved",
    "game2048-pause",
    "capability-lab",
    "capability-lab-controls",
    "capability-lab-assets",
    "capability-lab-assets-later",
    "capability-lab-lifecycle",
    "capability-lab-modal",
]
selected = os.environ.get("CRAZYPOD_MATRIX_SCENE")
if selected:
    if selected not in scenes:
        raise SystemExit(f"Error: unknown matrix scene {selected}")
    scenes = [selected]


def dump_state():
    return {
        path: (path.stat().st_mtime_ns, path.stat().st_size)
        for path in simdisk.glob("dump *.bmp")
    }


def validate_bitmap(path, scene):
    data = path.read_bytes()
    if len(data) < 66 or data[:2] != b"BM":
        raise SystemExit(f"{scene}: invalid BMP")
    file_size = struct.unpack_from("<I", data, 2)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    planes, bits = struct.unpack_from("<HH", data, 26)
    if (
        file_size != len(data)
        or width != 320
        or abs(height) != 240
        or planes != 1
        or bits not in (16, 24, 32)
    ):
        raise SystemExit(
            f"{scene}: malformed framebuffer "
            f"{width}x{height}x{bits}, size={len(data)}"
        )
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    row_stride = ((width * bits + 31) // 32) * 4
    pixel_bytes = row_stride * abs(height)
    if pixel_offset < 54 or pixel_offset + pixel_bytes > len(data):
        raise SystemExit(f"{scene}: invalid BMP pixel bounds")

    # The status bar contains a live clock. Exclude its top 16 display rows so
    # scene comparisons prove that app content changed, not merely the time.
    normalized = bytearray(data)
    for display_y in range(16):
        file_row = display_y if height < 0 else abs(height) - 1 - display_y
        start = pixel_offset + file_row * row_stride
        normalized[start:start + row_stride] = b"\0" * row_stride
    return hashlib.sha256(normalized).hexdigest()


def pixel16(path, x, y):
    data = path.read_bytes()
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    if bits != 16:
        raise SystemExit(f"{path.stem}: expected RGB565 framebuffer")
    row_stride = ((width * bits + 31) // 32) * 4
    file_row = y if height < 0 else abs(height) - 1 - y
    return struct.unpack_from(
        "<H", data, pixel_offset + file_row * row_stride + x * 2
    )[0]


def bright_pixel_count(path, left, top, right, bottom):
    count = 0
    for y in range(top, bottom):
        for x in range(left, right):
            value = pixel16(path, x, y)
            red = (value >> 11) & 31
            green = (value >> 5) & 63
            blue = value & 31
            if red >= 24 and green >= 48 and blue >= 24:
                count += 1
    return count


hashes = {}
for scene in scenes:
    before = dump_state()
    environment = os.environ.copy()
    environment.update({
        "SDL_AUDIODRIVER": "dummy",
        "CRAZYPOD_SIM_DUMP": "1",
        "CRAZYPOD_SIM_EXIT_AFTER_DUMP": "1",
        "CRAZYPOD_SIM_DUMP_SETTLE_MS": (
            "700" if scene == "capability-lab-assets"
            else "950" if scene == "capability-lab-assets-later"
            else "900"
        ),
        "CRAZYPOD_SIM_SCREEN": (
            "capability-lab-assets"
            if scene == "capability-lab-assets-later" else scene
        ),
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
            f"{scene}: simulator exited with {result.returncode}"
        )
    after = dump_state()
    created = [
        path for path, state in after.items()
        if path not in before or before[path] != state
    ]
    if len(created) != 1:
        raise SystemExit(
            f"{scene}: expected one dump, found {len(created)}"
        )
    destination = output / f"{scene}.bmp"
    shutil.move(created[0], destination)
    hashes[scene] = validate_bitmap(destination, scene)

for left, right in (
    ("native-reference", "native-reference-clicked"),
    ("game2048", "game2048-game"),
    ("game2048", "game2048-exit-prompt"),
    ("game2048-game", "game2048-moved"),
    ("game2048-game", "game2048-pause"),
    ("capability-lab", "capability-lab-controls"),
    ("capability-lab-controls", "capability-lab-assets"),
    ("capability-lab-assets", "capability-lab-assets-later"),
    ("capability-lab-lifecycle", "capability-lab-modal"),
):
    if left in hashes and right in hashes and hashes[left] == hashes[right]:
        raise SystemExit(f"{left} and {right} rendered identically")

for after_exit in (
    "game2048-exit-list",
    "game2048-exit-notes",
):
    if after_exit in hashes:
        bitmap = output / f"{after_exit}.bmp"
        if bright_pixel_count(bitmap, 0, 32, 320, 240) < 100:
            raise SystemExit(
                f"{after_exit}: host content is black after mini app exit"
            )

prompt = output / "game2048-exit-prompt.bmp"
if "game2048-exit-prompt" in hashes:
    if pixel16(prompt, 100, 70) == pixel16(prompt, 10, 50):
        raise SystemExit("game2048-exit-prompt: host panel is not visible")

for scene, point in (
    ("game2048-game", (160, 230)),
    ("capability-lab-controls", (160, 200)),
    ("capability-lab-assets", (160, 205)),
):
    if scene in hashes:
        bitmap = output / f"{scene}.bmp"
        x, y = point
        if pixel16(bitmap, x, y) == pixel16(bitmap, 10, y):
            raise SystemExit(f"{scene}: bottom action is clipped")

pause = output / "game2048-pause.bmp"
if "game2048-pause" in hashes:
    if bright_pixel_count(pause, 80, 150, 240, 175) < 80:
        raise SystemExit("game2048-pause: RESTART label is clipped")
    if bright_pixel_count(pause, 80, 190, 240, 215) < 60:
        raise SystemExit("game2048-pause: HOME label is clipped")

print(
    "CrazyPod Native AOT simulator matrix passed: "
    f"{len(scenes)} current scenes"
)
PY
