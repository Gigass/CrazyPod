#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
simulator="$repo_root/build-sim/rockboxui"
music_dir=${1:-${CRAZYPOD_REAL_MUSIC_DIR:-}}
output_dir=${2:-}

if [ ! -x "$simulator" ]; then
    echo "Error: build the simulator before running the real-library test." >&2
    exit 2
fi
if [ -z "$music_dir" ] || [ ! -d "$music_dir" ]; then
    echo "Usage: $0 MUSIC_DIRECTORY [OUTPUT_DIRECTORY]" >&2
    exit 2
fi

python3 - "$simulator" "$repo_root/build-sim/simdisk" \
    "$music_dir" "$output_dir" <<'PY'
import datetime as dt
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

simulator = Path(sys.argv[1]).resolve()
source_simdisk = Path(sys.argv[2]).resolve()
music = Path(sys.argv[3]).resolve()
requested_output = sys.argv[4]
supported = {".aac", ".flac", ".m4a", ".mp3", ".ogg", ".wav"}
media = sorted(
    file for file in music.rglob("*")
    if file.is_file() and file.suffix.lower() in supported
)
if len(media) < 2:
    raise SystemExit("real-library test requires at least two supported tracks")

sdk_header = source_simdisk.parent.parent / "miniapps/sdk/crazypod_miniapp_native.h"
minor_match = re.search(
    r"#define CP_NATIVE_ABI_MINOR (\d+)u", sdk_header.read_text()
)
if minor_match is None:
    raise SystemExit("cannot read Host ABI minor from the firmware SDK")
expected_abi_minor = int(minor_match.group(1))
theme_packages = list((
    source_simdisk / ".rockbox/crazypod/miniapps/packages"
).glob("now-playing-neon-*.cpk"))
if len(theme_packages) != 1:
    raise SystemExit(
        f"expected one staged now-playing theme package, found {len(theme_packages)}"
    )
with zipfile.ZipFile(theme_packages[0]) as package:
    theme_manifest = json.loads(package.read("manifest.json"))
if theme_manifest.get("abiMinor") != expected_abi_minor:
    raise SystemExit(
        f"staged theme ABI {theme_manifest.get('abiMinor')} does not match "
        f"Host ABI {expected_abi_minor}: {theme_packages[0]}"
    )

if requested_output:
    output = Path(requested_output).resolve()
else:
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    output = Path("/tmp") / f"crazypod-real-library-{stamp}"
if output.exists():
    raise SystemExit(f"output directory already exists: {output}")
output.mkdir(parents=True)


def bmp_pixels(path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise SystemExit(f"not a BMP: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    bits = struct.unpack_from("<H", data, 28)[0]
    if width != 320 or abs(height) != 240 or bits != 16:
        raise SystemExit(f"unexpected framebuffer format: {path}")
    stride = ((width * bits + 31) // 32) * 4

    def pixel(x, y):
        source_y = abs(height) - 1 - y if height > 0 else y
        start = offset + source_y * stride + x * 2
        return data[start:start + 2]

    return data, pixel


def validate_frame(path):
    data, pixel = bmp_pixels(path)
    cover = [
        pixel(x, y)
        for y in range(57, 153, 4)
        for x in range(34, 130, 4)
    ]
    colors = set(cover)
    dominant = max(cover.count(color) for color in colors) / len(cover)
    horizontal_diversity = max(
        len({pixel(x, y) for x in range(34, 130, 4)})
        for y in range(57, 153, 4)
    )
    if len(colors) < 32 or horizontal_diversity < 8:
        raise SystemExit(
            f"decoded cover is missing or visually flat: {path.name} "
            f"colors={len(colors)} horizontal={horizontal_diversity}"
        )
    title_colors = {
        pixel(x, y)
        for y in range(76, 111)
        for x in range(154, 282)
    }
    if len(title_colors) < 2:
        raise SystemExit(f"track title is blank: {path.name}")
    panel_background = Counter(
        pixel(x, y)
        for y in range(55, 160, 4)
        for x in range(150, 290, 4)
    ).most_common(1)[0][0]
    for name, top, bottom in (
        ("title/artist", 111, 112),
        ("album/volume", 145, 148),
    ):
        band = [
            pixel(x, y)
            for y in range(top, bottom + 1)
            for x in range(154, 270)
        ]
        clear_ratio = band.count(panel_background) / len(band)
        if clear_ratio < 0.92:
            raise SystemExit(
                f"metadata overlaps {name}: {path.name} "
                f"clear={clear_ratio:.3f}"
            )
    return (
        hashlib.sha256(data).hexdigest(), len(colors),
        horizontal_diversity, dominant,
    )


def validate_controls(path):
    _, pixel = bmp_pixels(path)
    panel = {
        pixel(x, y)
        for y in range(38, 211, 3)
        for x in range(35, 285, 3)
    }
    labels = {
        pixel(x, y)
        for y in range(48, 202)
        for x in range(48, 274)
    }
    if len(panel) < 12 or len(labels) < 8:
        raise SystemExit(
            f"theme controls are blank: {path.name} "
            f"panel={len(panel)} labels={len(labels)}"
        )


with tempfile.TemporaryDirectory(prefix="crazypod-real-library-work-") as temp:
    root = Path(temp)
    simdisk = root / "simdisk"
    shutil.copytree(source_simdisk / ".rockbox", simdisk / ".rockbox")
    (simdisk / "Music").mkdir(parents=True)
    (simdisk / "Music" / music.name).symlink_to(music, target_is_directory=True)

    def run(screen, settle_ms):
        before = set(simdisk.glob("dump *.bmp"))
        environment = os.environ.copy()
        environment.update({
            "SDL_AUDIODRIVER": "dummy",
            "CRAZYPOD_SIM_DUMP": "1",
            "CRAZYPOD_SIM_EXIT_AFTER_DUMP": "1",
            "CRAZYPOD_SIM_DUMP_SETTLE_MS": str(settle_ms),
            "CRAZYPOD_SIM_SCREEN": screen,
        })
        result = subprocess.run(
            [str(simulator)], cwd=root, env=environment,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=30, check=False,
        )
        log = result.stdout.decode("utf-8", errors="replace")
        if result.returncode != 0:
            raise SystemExit(
                f"simulator screen {screen} exited with {result.returncode}\n"
                f"{log}"
            )
        if "Invalid base64 char:" in log:
            raise SystemExit(
                f"screen {screen} crossed an embedded-artwork Base64 boundary\n"
                f"{log}"
            )
        created = set(simdisk.glob("dump *.bmp")) - before
        if len(created) != 1:
            raise SystemExit(
                f"screen {screen} produced {len(created)} framebuffers"
            )
        return created.pop(), log

    run("now-playing-theme-media-catalog", 100)
    catalog = simdisk / ".crazypod/cache/music-library.bin"
    header = catalog.read_bytes()[:64]
    if len(header) < 24:
        raise SystemExit("music catalog header is truncated")
    track_count = struct.unpack_from("<I", header, 20)[0]
    if track_count != len(media):
        raise SystemExit(
            f"catalog has {track_count} tracks, source has {len(media)}"
        )

    frames = []
    hashes = []
    for index in range(track_count):
        frame, log = run(f"now-playing-theme-media-step-{index}", 4000)
        destination = output / f"track-{index:02}.bmp"
        shutil.copy2(frame, destination)
        (output / f"track-{index:02}.log").write_text(log, encoding="utf-8")
        digest, colors, horizontal, dominant = validate_frame(destination)
        hashes.append(digest)
        frames.append({
            "index": index,
            "frame": destination.name,
            "sha256": digest,
            "coverSampleColors": colors,
            "coverHorizontalDiversity": horizontal,
            "coverDominantRatio": round(dominant, 4),
        })

    control_frame, control_log = run(
        "now-playing-theme-media-controls", 4000)
    control_destination = output / "all-controls.bmp"
    shutil.copy2(control_frame, control_destination)
    (output / "all-controls.log").write_text(
        control_log, encoding="utf-8")
    validate_controls(control_destination)

    unique_frames = len(set(hashes))
    if unique_frames < max(2, track_count * 3 // 4):
        raise SystemExit(
            f"only {unique_frames}/{track_count} track frames are distinct"
        )
    report = {
        "musicDirectory": str(music),
        "trackCount": track_count,
        "uniqueFrameCount": unique_frames,
        "controls": "passed",
        "frames": frames,
    }
    (output / "report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps({
        "status": "passed",
        "output": str(output),
        "trackCount": track_count,
        "uniqueFrameCount": unique_frames,
        "controls": "passed",
    }, ensure_ascii=False))
PY
