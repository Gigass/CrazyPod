#!/usr/bin/env python3
"""Build deterministic, signed CrazyPod mini-app packages."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import zipfile
import zlib


ROOT = Path(__file__).resolve().parent.parent
APP_IDS = ("calculator", "pomodoro", "game2048")
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)
RESOURCE_MAGIC = 0x53525043
RESOURCE_VERSION = 1
RESOURCE_COUNT_MAX = 32
RESOURCE_TOTAL_MAX = 512 * 1024
RESOURCE_ITEM_MAX = 128 * 1024
RESOURCE_BITMAP = re.compile(
    r"^(?P<id>[a-z][a-z0-9_.-]*)\.(?P<width>[0-9]+)x"
    r"(?P<height>[0-9]+)\.rgb565$"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--target", required=True)
    parser.add_argument(
        "--binary",
        required=True,
        choices=("app.arm", "app.dylib"),
        help="payload name selected for hardware or simulator packages",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "dist" / "miniapps",
    )
    parser.add_argument(
        "--key",
        type=Path,
        default=ROOT / "miniapps" / "keys" / "development_ed25519.pem",
    )
    return parser.parse_args()


def find_openssl() -> str:
    configured = os.environ.get("CRAZYPOD_OPENSSL")
    candidates = [
        configured,
        shutil.which("openssl"),
        "/opt/homebrew/opt/openssl@3/bin/openssl",
        "/usr/local/opt/openssl@3/bin/openssl",
    ]
    checked: set[str] = set()
    for candidate in candidates:
        if not candidate or candidate in checked:
            continue
        checked.add(candidate)
        try:
            result = subprocess.run(
                [candidate, "version"],
                check=True,
                capture_output=True,
                text=True,
            )
        except (OSError, subprocess.CalledProcessError):
            continue
        if result.stdout.startswith("OpenSSL 3."):
            return candidate
    raise SystemExit(
        "OpenSSL 3 is required for Ed25519 signing. "
        "Set CRAZYPOD_OPENSSL to its executable."
    )


def put_pixel(
    pixels: bytearray,
    width: int,
    height: int,
    x: int,
    y: int,
    color: tuple[int, int, int],
) -> None:
    if x < 0 or y < 0 or x >= width or y >= height:
        return
    row = height - 1 - y
    offset = (row * width + x) * 4
    red, green, blue = color
    pixels[offset : offset + 4] = bytes((blue, green, red, 255))


def fill_rect(
    pixels: bytearray,
    width: int,
    height: int,
    x: int,
    y: int,
    rect_width: int,
    rect_height: int,
    color: tuple[int, int, int],
) -> None:
    for py in range(y, y + rect_height):
        for px in range(x, x + rect_width):
            put_pixel(pixels, width, height, px, py, color)


def fill_rounded_rect(
    pixels: bytearray,
    width: int,
    height: int,
    x: int,
    y: int,
    rect_width: int,
    rect_height: int,
    radius: int,
    color: tuple[int, int, int],
) -> None:
    for py in range(y, y + rect_height):
        for px in range(x, x + rect_width):
            dx = min(px - x, x + rect_width - 1 - px)
            dy = min(py - y, y + rect_height - 1 - py)
            if dx >= radius or dy >= radius:
                put_pixel(pixels, width, height, px, py, color)
                continue
            corner_x = radius - dx - 1
            corner_y = radius - dy - 1
            if corner_x * corner_x + corner_y * corner_y <= radius * radius:
                put_pixel(pixels, width, height, px, py, color)


def draw_calculator_icon(pixels: bytearray, width: int, height: int) -> None:
    panel = (24, 24, 32)
    white = (244, 244, 248)
    cyan = (38, 207, 245)
    fill_rounded_rect(pixels, width, height, 30, 17, 100, 126, 20, panel)
    fill_rounded_rect(pixels, width, height, 42, 32, 76, 25, 7, white)
    for row in range(3):
        for column in range(3):
            color = cyan if column == 2 else white
            fill_rounded_rect(
                pixels,
                width,
                height,
                42 + column * 28,
                72 + row * 22,
                20,
                14,
                5,
                color,
            )


def draw_pomodoro_icon(pixels: bytearray, width: int, height: int) -> None:
    white = (244, 244, 248)
    rose = (255, 69, 104)
    muted = (59, 59, 70)
    center_x = 80
    center_y = 85
    for y in range(height):
        for x in range(width):
            distance_sq = (x - center_x) ** 2 + (y - center_y) ** 2
            if 45 * 45 <= distance_sq <= 60 * 60:
                put_pixel(pixels, width, height, x, y, muted)
            if 45 * 45 <= distance_sq <= 55 * 55 and y <= center_y:
                put_pixel(pixels, width, height, x, y, rose)
    fill_rounded_rect(pixels, width, height, 68, 12, 24, 18, 7, white)
    fill_rounded_rect(pixels, width, height, 55, 21, 50, 10, 5, white)
    fill_rect(pixels, width, height, 75, 58, 10, 34, white)
    fill_rect(pixels, width, height, 80, 82, 28, 10, white)

def draw_game2048_icon(pixels: bytearray, width: int, height: int) -> None:
    panel = (38, 38, 48)
    empty = (66, 66, 78)
    amber = (242, 177, 121)
    rose = (244, 92, 89)
    green = (48, 209, 88)
    colors = (
        amber, empty, rose, amber,
        empty, green, amber, empty,
        rose, amber, green, rose,
        amber, empty, rose, green,
    )
    fill_rounded_rect(pixels, width, height, 18, 18, 124, 124, 18, panel)
    for row in range(4):
        for column in range(4):
            fill_rounded_rect(
                pixels,
                width,
                height,
                28 + column * 27,
                28 + row * 27,
                22,
                22,
                5,
                colors[row * 4 + column],
            )


def make_icon(app_id: str) -> bytes:
    width = 160
    height = 160
    background = (8, 8, 13)
    pixels = bytearray()
    for _ in range(width * height):
        red, green, blue = background
        pixels.extend((blue, green, red, 255))
    if app_id == "calculator":
        draw_calculator_icon(pixels, width, height)
    elif app_id == "pomodoro":
        draw_pomodoro_icon(pixels, width, height)
    else:
        draw_game2048_icon(pixels, width, height)

    pixel_offset = 14 + 40
    file_size = pixel_offset + len(pixels)
    file_header = b"BM" + struct.pack("<IHHI", file_size, 0, 0, pixel_offset)
    dib_header = struct.pack(
        "<IIIHHIIIIII",
        40,
        width,
        height,
        1,
        32,
        0,
        len(pixels),
        2835,
        2835,
        0,
        0,
    )
    return file_header + dib_header + bytes(pixels)


def render_manifest(
    app_id: str,
    target: str,
    binary_name: str,
    binary: bytes,
    icon: bytes,
    resources: bytes,
) -> bytes:
    template_path = ROOT / "miniapps" / app_id / "manifest.ini.in"
    manifest = template_path.read_text(encoding="utf-8")
    manifest = manifest.replace("@TARGET@", target)
    manifest = manifest.replace("@BINARY@", binary_name)
    if "@" in manifest:
        raise SystemExit(f"unresolved manifest placeholder in {template_path}")
    manifest += f"binary_sha256={hashlib.sha256(binary).hexdigest()}\n"
    manifest += f"icon_sha256={hashlib.sha256(icon).hexdigest()}\n"
    manifest += (
        f"resources_sha256={hashlib.sha256(resources).hexdigest()}\n"
    )
    return manifest.encode("utf-8")


def build_resources(app_id: str) -> bytes:
    resource_dir = ROOT / "miniapps" / app_id / "resources"
    resources: list[tuple[str, int, int, int, bytes]] = []
    if resource_dir.is_dir():
        for path in sorted(resource_dir.iterdir()):
            if not path.is_file():
                raise SystemExit(f"resource is not a file: {path}")
            match = RESOURCE_BITMAP.fullmatch(path.name)
            if match is not None:
                resource_id = match.group("id")
                width = int(match.group("width"))
                height = int(match.group("height"))
                contents = path.read_bytes()
                if width < 1 or height < 1 or width > 160 or height > 160:
                    raise SystemExit(f"invalid bitmap dimensions: {path}")
                if len(contents) != width * height * 2:
                    raise SystemExit(f"invalid RGB565 byte count: {path}")
                resource_type = 1
            else:
                resource_id = path.name
                if re.fullmatch(r"[a-z][a-z0-9_.-]*", resource_id) is None:
                    raise SystemExit(f"invalid resource id: {path.name}")
                width = 0
                height = 0
                contents = path.read_bytes()
                resource_type = 0
            if len(resource_id.encode("ascii")) >= 32:
                raise SystemExit(f"resource id is too long: {resource_id}")
            if len(contents) > RESOURCE_ITEM_MAX:
                raise SystemExit(f"resource exceeds 128 KiB: {path}")
            resources.append(
                (resource_id, resource_type, width, height, contents)
            )
    if len(resources) > RESOURCE_COUNT_MAX:
        raise SystemExit(f"too many resources for {app_id}")
    resources.sort(key=lambda item: item[0])
    ids = [item[0] for item in resources]
    if len(ids) != len(set(ids)):
        raise SystemExit(f"duplicate resource id in {resource_dir}")

    header_size = 16
    entry_size = 52
    data_offset = header_size + entry_size * len(resources)
    index = bytearray()
    payload = bytearray()
    for resource_id, resource_type, width, height, contents in resources:
        encoded_id = resource_id.encode("ascii")
        encoded_id += bytes(32 - len(encoded_id))
        index.extend(
            struct.pack(
                "<32sBBHHHIII",
                encoded_id,
                resource_type,
                0,
                width,
                height,
                0,
                data_offset,
                len(contents),
                zlib.crc32(contents) & 0xFFFFFFFF,
            )
        )
        payload.extend(contents)
        data_offset += len(contents)
    total_size = header_size + len(index) + len(payload)
    if total_size > RESOURCE_TOTAL_MAX:
        raise SystemExit(f"resources exceed 512 KiB for {app_id}")
    header = struct.pack(
        "<IHHII",
        RESOURCE_MAGIC,
        RESOURCE_VERSION,
        len(resources),
        total_size,
        zlib.crc32(index) & 0xFFFFFFFF,
    )
    return header + bytes(index) + bytes(payload)


def sign_manifest(
    openssl: str, key_path: Path, manifest: bytes
) -> bytes:
    with tempfile.TemporaryDirectory(prefix="crazypod-miniapp-sign-") as temp:
        manifest_path = Path(temp) / "manifest.ini"
        signature_path = Path(temp) / "signature.ed25519"
        manifest_path.write_bytes(manifest)
        subprocess.run(
            [
                openssl,
                "pkeyutl",
                "-sign",
                "-rawin",
                "-inkey",
                str(key_path),
                "-in",
                str(manifest_path),
                "-out",
                str(signature_path),
            ],
            check=True,
        )
        signature = signature_path.read_bytes()
    if len(signature) != 64:
        raise SystemExit(
            f"invalid Ed25519 signature size: expected 64, got {len(signature)}"
        )
    return signature


def manifest_value(manifest: bytes, key: str) -> str:
    prefix = f"{key}="
    for line in manifest.decode("utf-8").splitlines():
        if line.startswith(prefix):
            value = line[len(prefix) :]
            if value:
                return value
    raise SystemExit(f"manifest is missing {key}")


def package_version(manifest: bytes) -> str:
    version = manifest_value(manifest, "version")
    if re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version) is None:
        raise SystemExit(f"unsupported package version: {version}")
    return version


def zip_entry(name: str, contents: bytes) -> tuple[zipfile.ZipInfo, bytes]:
    info = zipfile.ZipInfo(name, ZIP_TIMESTAMP)
    info.compress_type = zipfile.ZIP_STORED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    return info, contents


def build_package(
    app_id: str,
    args: argparse.Namespace,
    openssl: str,
) -> Path:
    binary_path = args.build_dir / "miniapps" / app_id / args.binary
    if not binary_path.is_file():
        raise SystemExit(f"missing mini-app payload: {binary_path}")
    binary = binary_path.read_bytes()
    icon = make_icon(app_id)
    resources = build_resources(app_id)
    manifest = render_manifest(
        app_id, args.target, args.binary, binary, icon, resources
    )
    signature = sign_manifest(openssl, args.key, manifest)
    version = package_version(manifest)

    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output / f"{app_id}-{version}.cpk"
    with zipfile.ZipFile(output, "w", allowZip64=False) as package:
        entries = (
            zip_entry("manifest.ini", manifest),
            zip_entry(args.binary, binary),
            zip_entry("icon.bmp", icon),
            zip_entry("signature.ed25519", signature),
            zip_entry("resources.bin", resources),
        )
        for info, contents in entries:
            package.writestr(info, contents)
    return output


def main() -> None:
    args = parse_args()
    args.build_dir = args.build_dir.resolve()
    args.output = args.output.resolve()
    args.key = args.key.resolve()
    if not args.key.is_file():
        raise SystemExit(f"missing signing key: {args.key}")
    openssl = find_openssl()
    for app_id in APP_IDS:
        package = build_package(app_id, args, openssl)
        print(package)


if __name__ == "__main__":
    main()
