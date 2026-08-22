#!/usr/bin/env python3
"""Download, normalize, and play MP4/M4V/MOV fixtures in CrazyPod."""

from __future__ import annotations

import datetime as dt
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
import urllib.request


REPO = Path(__file__).resolve().parents[1]
SIMULATOR = REPO / "build-sim/rockboxui"
SOURCE_SIMDISK = REPO / "build-sim/simdisk"
SOURCE_CACHE = Path(os.environ.get(
    "CRAZYPOD_VIDEO_FIXTURE_CACHE",
    str(REPO / "build-sim/video-test-sources"),
)).resolve()
SOURCES = {
    "mp4": (
        "https://samples.ffmpeg.org/archive/video/h264/"
        "mov%2Bh264%2Baac%2B%2BH264memleak.mp4"
    ),
    "m4v": (
        "https://raw.githubusercontent.com/mifi/"
        "lossless-cut-fixtures/master/sample_iPod.m4v"
    ),
    "mov": (
        "https://samples.ffmpeg.org/archive/video/h264/"
        "mov%2Bh264%2Baac%2B%2BDemo_FlagOfOurFathers.mov"
    ),
}
START_OFFSETS = {"mp4": "0", "m4v": "25", "mov": "0"}
DIAGNOSTIC = re.compile(
    r"CrazyPod FFmpeg: decoded_video=(\d+) presented_video=(\d+) "
    r"decoded_audio_frames=(\d+) position_ms=(\d+)"
)


def fail(message: str) -> None:
    raise SystemExit(message)


def run(command: list[str], **kwargs) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(command, check=False, **kwargs)


def require_tools() -> None:
    if not SIMULATOR.is_file():
        fail("build-sim/rockboxui is missing; run ./build-sim.sh first")
    if not (SOURCE_SIMDISK / ".rockbox").is_dir():
        fail("build-sim/simdisk/.rockbox is missing; run ./build-sim.sh first")
    for tool in ("ffmpeg", "ffprobe"):
        if shutil.which(tool) is None:
            fail(f"required tool is missing: {tool}")


def download(extension: str, url: str) -> Path:
    destination = SOURCE_CACHE / f"source.{extension}"
    if destination.exists() and destination.stat().st_size > 0:
        return destination
    SOURCE_CACHE.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".download")
    try:
        with urllib.request.urlopen(url, timeout=60) as response:
            with temporary.open("wb") as output:
                shutil.copyfileobj(response, output)
        if temporary.stat().st_size == 0:
            fail(f"downloaded empty fixture: {url}")
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)
    return destination


def probe(path: Path) -> dict:
    result = run([
        "ffprobe", "-v", "error", "-show_streams", "-show_format",
        "-of", "json", str(path),
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        fail(f"ffprobe failed for {path}:\n{result.stderr.decode()}")
    return json.loads(result.stdout)


def require_h264_aac(path: Path, metadata: dict, baseline: bool) -> None:
    streams = metadata.get("streams", [])
    video = next((item for item in streams if item.get("codec_type") == "video"), None)
    audio = next((item for item in streams if item.get("codec_type") == "audio"), None)
    if video is None or video.get("codec_name") != "h264":
        fail(f"fixture does not contain H.264 video: {path}")
    if audio is None or audio.get("codec_name") != "aac":
        fail(f"fixture does not contain AAC audio: {path}")
    if baseline and "Baseline" not in video.get("profile", ""):
        fail(f"normalized fixture is not H.264 Baseline: {path}")
    if baseline and (video.get("width"), video.get("height")) != (320, 180):
        fail(f"normalized fixture is not 320x180: {path}")
    if baseline and (audio.get("sample_rate"), audio.get("channels")) != ("48000", 2):
        fail(f"normalized fixture audio is not 48 kHz stereo: {path}")


def normalize(source: Path, destination: Path, start_offset: str) -> dict:
    result = run([
        "ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
        "-ss", start_offset, "-i", str(source), "-t", "5",
        "-map", "0:v:0", "-map", "0:a:0",
        "-vf",
        "scale=320:180:force_original_aspect_ratio=decrease,"
        "pad=320:180:(ow-iw)/2:(oh-ih)/2:black,setsar=1",
        "-r", "24", "-c:v", "libx264", "-profile:v", "baseline",
        "-level:v", "3.0", "-pix_fmt", "yuv420p", "-preset", "fast",
        "-b:v", "500k", "-maxrate", "600k", "-bufsize", "1200k",
        "-g", "48", "-c:a", "aac", "-profile:a", "aac_low",
        "-b:a", "96k", "-ar", "48000", "-ac", "2",
        "-movflags", "+faststart", str(destination),
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        fail(f"ffmpeg failed for {source}:\n{result.stderr.decode()}")
    metadata = probe(destination)
    require_h264_aac(destination, metadata, baseline=True)
    return metadata


def bmp_pixel_reader(path: Path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        fail(f"simulator dump is not BMP: {path}")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = struct.unpack_from("<i", data, 22)[0]
    bits = struct.unpack_from("<H", data, 28)[0]
    if width != 320 or abs(height) != 240 or bits != 16:
        fail(f"unexpected framebuffer format: {path}")
    stride = ((width * bits + 31) // 32) * 4

    def pixel(x: int, y: int) -> bytes:
        source_y = abs(height) - 1 - y if height > 0 else y
        start = offset + source_y * stride + x * 2
        return data[start:start + 2]

    return data, pixel


def validate_frame(path: Path) -> dict:
    data, pixel = bmp_pixel_reader(path)
    pixels = [
        pixel(x, y)
        for y in range(30, 200, 3)
        for x in range(40, 280, 3)
    ]
    colors = set(pixels)
    black = b"\x00\x00"
    nonblack_ratio = sum(value != black for value in pixels) / len(pixels)
    if len(colors) < 64 or nonblack_ratio < 0.25:
        fail(
            f"decoded video frame is blank: {path.name} "
            f"colors={len(colors)} nonblack={nonblack_ratio:.3f}"
        )
    return {
        "sha256": hashlib.sha256(data).hexdigest(),
        "colors": len(colors),
        "nonblack_ratio": round(nonblack_ratio, 3),
    }


def play_fixture(root: Path, simdisk: Path, extension: str, output: Path) -> dict:
    media_path = f"/Videos/crazypod-smoke.{extension}"
    before = set(simdisk.glob("dump *.bmp"))
    environment = os.environ.copy()
    environment.update({
        "SDL_AUDIODRIVER": "dummy",
        "CRAZYPOD_SIM_DUMP": "1",
        "CRAZYPOD_SIM_SCREEN": "play-video-0",
        "CRAZYPOD_SIM_VIDEO_PATH": media_path,
        "CRAZYPOD_SIM_VIDEO_DUMP": "1",
        "CRAZYPOD_SIM_VIDEO_DUMP_AFTER": "2",
        "CRAZYPOD_SIM_VIDEO_DIAGNOSTICS": "1",
        "CRAZYPOD_SIM_EXIT_AFTER_DUMP": "1",
    })
    result = run(
        [str(SIMULATOR)], cwd=root, env=environment,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30,
    )
    log = result.stdout.decode("utf-8", errors="replace")
    (output / f"{extension}.log").write_text(log, encoding="utf-8")
    if result.returncode != 0:
        fail(f"simulator failed for {extension} ({result.returncode}):\n{log}")
    if f"path={media_path} result=0" not in log:
        fail(f"simulator did not report successful {extension} playback:\n{log}")
    match = DIAGNOSTIC.search(log)
    if match is None:
        fail(f"simulator did not report FFmpeg diagnostics for {extension}")
    decoded_video, presented_video, decoded_audio, position_ms = map(int, match.groups())
    if min(decoded_video, presented_video, decoded_audio, position_ms) <= 0:
        fail(f"simulator did not decode video and audio for {extension}: {match.group(0)}")
    created = set(simdisk.glob("dump *.bmp")) - before
    if len(created) != 1:
        fail(f"simulator created {len(created)} framebuffers for {extension}")
    frame = output / f"{extension}.bmp"
    shutil.copy2(created.pop(), frame)
    visual = validate_frame(frame)
    return {
        "decoded_video": decoded_video,
        "presented_video": presented_video,
        "decoded_audio_frames": decoded_audio,
        "position_ms": position_ms,
        **visual,
    }


def main() -> None:
    require_tools()
    sources: dict[str, Path] = {}
    for extension, url in SOURCES.items():
        source = download(extension, url)
        require_h264_aac(source, probe(source), baseline=False)
        sources[extension] = source

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    output = Path(os.environ.get(
        "CRAZYPOD_VIDEO_TEST_OUTPUT",
        f"/tmp/crazypod-video-simulator-{stamp}",
    )).resolve()
    if output.exists():
        fail(f"output directory already exists: {output}")
    output.mkdir(parents=True)

    report = {"sources": {}, "playback": {}, "output": str(output)}
    with tempfile.TemporaryDirectory(prefix="crazypod-video-work-") as temporary:
        root = Path(temporary)
        simdisk = root / "simdisk"
        shutil.copytree(SOURCE_SIMDISK / ".rockbox", simdisk / ".rockbox")
        videos = simdisk / "Videos"
        videos.mkdir(parents=True)
        for extension, source in sources.items():
            normalized = videos / f"crazypod-smoke.{extension}"
            metadata = normalize(
                source, normalized, START_OFFSETS[extension])
            shutil.copy2(normalized, output / normalized.name)
            report["sources"][extension] = {
                "download": str(source),
                "bytes": source.stat().st_size,
                "normalized_bytes": normalized.stat().st_size,
                "format": metadata["format"].get("format_name"),
            }
        for extension in SOURCES:
            report["playback"][extension] = play_fixture(
                root, simdisk, extension, output)

    (output / "report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
