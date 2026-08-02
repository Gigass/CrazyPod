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
import atexit
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

simulator = Path(sys.argv[1])
build = Path(sys.argv[2])
simdisk = build / "simdisk"
repo = build.parent
frame_root = Path(tempfile.mkdtemp(prefix="crazypod-native-aot-frames-"))
atexit.register(shutil.rmtree, frame_root, ignore_errors=True)


def run(screen, settle_ms=500):
    for previous in simdisk.glob("dump *.bmp"):
        previous.unlink()
    environment = os.environ.copy()
    environment.update({
        "SDL_AUDIODRIVER": "dummy",
        "CRAZYPOD_SIM_DUMP": "1",
        "CRAZYPOD_SIM_EXIT_AFTER_DUMP": "1",
        "CRAZYPOD_SIM_DUMP_SETTLE_MS": str(settle_ms),
        "CRAZYPOD_SIM_SCREEN": screen,
    })
    result = subprocess.run(
        [str(simulator)],
        cwd=build,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=20,
        check=False,
    )
    log = result.stdout.decode("utf-8", errors="replace")
    sys.stdout.write(log)
    if result.returncode != 0:
        raise SystemExit(
            f"Native AOT simulator exited with {result.returncode}\n{log}"
        )
    if "Invalid base64 char:" in log:
        raise SystemExit(
            f"Native AOT screen {screen} crossed an embedded-artwork "
            f"Base64 boundary\n{log}"
        )
    created = set(simdisk.glob("dump *.bmp"))
    if len(created) != 1:
        raise SystemExit(
            f"screen {screen} expected one Native AOT dump, "
            f"found {len(created)}"
        )
    destination = frame_root / f"{screen}.bmp"
    shutil.move(created.pop(), destination)
    return destination


def prepare_music_fixture():
    fixtures = (
        ("ThemeRegressionA", "track-a.wav", 220,
         repo / "utils/rbutilqt/icons/wizard.jpg"),
        ("ThemeRegressionB", "track-b.wav", 440,
         repo / "utils/rbutilqt/icons/wizard.jpg"),
    )
    for directory, filename, frequency, artwork in fixtures:
        root = simdisk / "Music" / directory
        root.mkdir(parents=True, exist_ok=True)
        with wave.open(str(root / filename), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(44100)
            frames = bytearray()
            for sample in range(44100 * 4):
                value = 9000 if (sample * frequency // 44100) % 2 == 0 else -9000
                frames.extend(struct.pack("<hh", value, value))
            output.writeframes(frames)
        shutil.copyfile(artwork, root / "folder.jpg")
    cache = simdisk / ".crazypod/cache"
    for name in ("music-library.bin", "music-library.tmp", "media.invalid"):
        try:
            (cache / name).unlink()
        except FileNotFoundError:
            pass


def artwork_has_image(bitmap):
    data = bitmap.read_bytes()
    if data[:2] != b"BM":
        raise SystemExit("simulator dump is not a BMP")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    bits = struct.unpack_from("<H", data, 28)[0]
    if width != 320 or abs(height) != 240 or bits != 16:
        raise SystemExit("simulator dump has an unexpected pixel format")
    stride = ((width * bits + 31) // 32) * 4
    rows = []
    for y in range(62, 150, 8):
        source_y = abs(height) - 1 - y if height > 0 else y
        start = pixel_offset + source_y * stride
        row = {
            data[start + x * 2:start + x * 2 + 2]
            for x in range(30, 124, 4)
        }
        rows.append(len(row))
    return max(rows) >= 8


def pixel_at(bitmap, x, y):
    data = bitmap.read_bytes()
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    stride = ((width * 16 + 31) // 32) * 4
    source_y = abs(height) - 1 - y if height > 0 else y
    start = pixel_offset + source_y * stride + x * 2
    return data[start:start + 2]


def rgb565(color):
    value = color.removeprefix("#")
    red = int(value[0:2], 16)
    green = int(value[2:4], 16)
    blue = int(value[4:6], 16)
    packed = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3)
    return struct.pack("<H", packed)


def count_colors(bitmap, colors, left, top, right, bottom):
    data = bitmap.read_bytes()
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    stride = ((width * 16 + 31) // 32) * 4
    count = 0
    for y in range(top, bottom):
        source_y = abs(height) - 1 - y if height > 0 else y
        row = pixel_offset + source_y * stride
        for x in range(left, right):
            start = row + x * 2
            if data[start:start + 2] in colors:
                count += 1
    return count


def verify_audio_driven_theme():
    with tempfile.TemporaryDirectory(
            prefix="crazypod-audio-meter-") as temporary:
        root = Path(temporary)
        isolated = root / "simdisk"
        shutil.copytree(simdisk / ".rockbox", isolated / ".rockbox")
        music = isolated / "Music" / "AudioMeter"
        music.mkdir(parents=True)
        with wave.open(str(music / "levels.wav"), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(44100)
            frames = bytearray()
            amplitudes = (2000, 2000, 2000, 18000, 18000,
                          18000, 18000, 18000, 18000, 18000)
            for sample in range(44100 * len(amplitudes)):
                amplitude = amplitudes[sample // 44100]
                value = amplitude if (sample * 220 // 44100) % 2 == 0 \
                    else -amplitude
                frames.extend(struct.pack("<hh", value, value))
            output.writeframes(frames)
        shutil.copyfile(music / "levels.wav", music / "auxiliary.wav")
        shutil.copyfile(
            repo / "utils/rbutilqt/icons/wizard.jpg",
            music / "folder.jpg",
        )
        cache = isolated / ".crazypod/cache"
        for name in ("music-library.bin", "music-library.tmp", "media.invalid"):
            try:
                (cache / name).unlink()
            except FileNotFoundError:
                pass

        def isolated_run(screen, settle_ms, name):
            before = set(isolated.glob("dump *.bmp"))
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
                timeout=20, check=False,
            )
            log = result.stdout.decode("utf-8", errors="replace")
            if result.returncode != 0:
                raise SystemExit(
                    f"isolated audio screen failed with {result.returncode}\n"
                    f"{log}"
                )
            created = set(isolated.glob("dump *.bmp")) - before
            if len(created) != 1:
                raise SystemExit(
                    f"isolated audio screen produced {len(created)} dumps"
                )
            destination = frame_root / name
            shutil.move(created.pop(), destination)
            return destination

        isolated_run("now-playing-theme-media-catalog", 100, "meter-catalog.bmp")
        early = isolated_run("now-playing-theme-media", 1000, "meter-low.bmp")
        late = isolated_run("now-playing-theme-media", 6000, "meter-high.bmp")
        amber = rgb565("#d89142")
        hot = rgb565("#e9683e")
        early_progress = count_colors(
            early, {amber}, 39, 184, 278, 193)
        late_progress = count_colors(
            late, {amber}, 39, 184, 278, 193)
        if late_progress <= early_progress + 60:
            raise SystemExit(
                "theme-drawn playback progress did not grow with elapsed time: "
                f"early={early_progress} late={late_progress}"
            )
        low_meter = count_colors(
            early, {amber, hot}, 39, 194, 278, 203)
        high_meter = count_colors(
            late, {amber, hot}, 39, 194, 278, 203)
        if high_meter <= low_meter + 200:
            raise SystemExit(
                "VU segments did not respond to PCM amplitude: "
                f"low={low_meter} high={high_meter}"
            )


prepare_music_fixture()
run("now-playing-theme-media-catalog", settle_ms=100)
if not (simdisk / ".crazypod/cache/music-library.bin").is_file():
    raise SystemExit("simulator did not build the music fixture catalog")


initial = run("native-reference")
clicked = run("native-reference-clicked")
capability_controls = run("capability-lab-controls")
theme = run("now-playing-theme")
theme_home_hold = run("now-playing-theme-home-hold")
theme_rerender = run("now-playing-theme-rerender")
theme_controls = run("now-playing-theme-controls")
theme_panel = run("now-playing-theme-panel")
theme_panel_repeat = run("now-playing-theme-panel-repeat")
theme_seek_menu_back = run("now-playing-theme-seek-menu-back")
theme_queue = run("now-playing-theme-queue", settle_ms=750)
theme_menu_exit = run("now-playing-theme-menu-exit")
theme_media = run("now-playing-theme-media", settle_ms=2500)
theme_media_next = run("now-playing-theme-media-next", settle_ms=2500)
signal_theme = run("now-playing-signal")
signal_controls = run("now-playing-signal-controls")
signal_all_panels = run("now-playing-signal-all-panels")
signal_panel = run("now-playing-signal-panel")
signal_queue = run("now-playing-signal-queue", settle_ms=750)
signal_mode = run("now-playing-signal-mode")
signal_lyrics = run("now-playing-signal-lyrics")
signal_seek = run("now-playing-signal-seek")
default_theme = run("now-playing-default")
initial_hash = hashlib.sha256(initial.read_bytes()).hexdigest()
clicked_hash = hashlib.sha256(clicked.read_bytes()).hexdigest()
if initial_hash == clicked_hash:
    raise SystemExit(
        "Native AOT event did not change the rendered framebuffer"
    )
if pixel_at(capability_controls, 70, 75) == pixel_at(
        capability_controls, 200, 75):
    raise SystemExit(
        "ProgressBar indicator is not visible against its track"
    )
if hashlib.sha256(theme.read_bytes()).digest() == hashlib.sha256(
        default_theme.read_bytes()).digest():
    raise SystemExit(
        "Now Playing theme did not replace the native default framebuffer"
    )
if hashlib.sha256(theme_rerender.read_bytes()).digest() == hashlib.sha256(
        default_theme.read_bytes()).digest():
    raise SystemExit(
        "Now Playing theme host rerender fell back to the native page"
    )
if hashlib.sha256(theme_controls.read_bytes()).digest() == hashlib.sha256(
        default_theme.read_bytes()).digest():
    raise SystemExit(
        "Now Playing theme controls closed or replaced the theme surface"
    )
if hashlib.sha256(theme.read_bytes()).digest() == hashlib.sha256(
        theme_panel.read_bytes()).digest():
    raise SystemExit("theme-owned options panel did not render")
if hashlib.sha256(theme_panel.read_bytes()).digest() == hashlib.sha256(
        theme_panel_repeat.read_bytes()).digest():
    raise SystemExit(
        "repeated click-wheel input did not move the theme panel selection"
    )
if hashlib.sha256(theme_panel.read_bytes()).digest() == hashlib.sha256(
        theme_seek_menu_back.read_bytes()).digest():
    raise SystemExit(
        "Menu after saving seek did not return to the outer theme"
    )
if hashlib.sha256(theme_panel.read_bytes()).digest() == hashlib.sha256(
        theme_queue.read_bytes()).digest():
    raise SystemExit("theme-owned queue panel did not render")
if hashlib.sha256(theme_menu_exit.read_bytes()).digest() == hashlib.sha256(
        theme.read_bytes()).digest():
    raise SystemExit("firmware-owned Menu did not exit the theme")
if not artwork_has_image(theme_media):
    raise SystemExit(
        "Now Playing theme did not render asynchronously decoded artwork"
    )
if not artwork_has_image(theme_media_next):
    raise SystemExit(
        "Now Playing theme lost artwork after the next-track command"
    )
if hashlib.sha256(theme_media.read_bytes()).digest() == hashlib.sha256(
        theme_media_next.read_bytes()).digest():
    raise SystemExit(
        "Now Playing theme next-track command did not update the framebuffer"
    )

verify_audio_driven_theme()

required = {
    "manifest.json",
    "app.dylib",
    "profile.bin",
    "assets.bin",
    "icon.bin",
    ".install.bin",
}
for app in (
    "native-reference", "game2048", "capability-lab", "now-playing-neon",
    "now-playing-signal"
):
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
    expected_kind = (
        "now-playing-theme"
        if app in ("now-playing-neon", "now-playing-signal") else "miniapp"
    )
    if manifest.get("kind", "miniapp") != expected_kind:
        raise SystemExit(f"{app} installed with the wrong package kind")
    for forbidden in ("app.js", "app.qbc", "styles.bin"):
        if (installed / forbidden).exists():
            raise SystemExit(
                f"{app} unexpectedly contains {forbidden}"
            )
print(
    "CrazyPod Native AOT simulator path verified: "
    "CPK5 install, app.dylib load, direct UI event, theme override, "
    "host rerender, real playback controls, asynchronous artwork and default"
)
PY
