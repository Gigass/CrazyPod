#!/usr/bin/env python3
"""Build-time character coverage and LVGL font generation for CrazyPod.

This tool deliberately has no Python package dependencies.  It reads Unicode
cmaps directly from OpenType/TrueType/TTC files and can also audit the U+XXXX
glyph comments emitted by lv_font_conv.

Typical use:
  python3 tools/crazypod_font_tool.py collect \
      --input apps/crazypod/l10n/*.json --output build/crazypod-chars.txt
  python3 tools/crazypod_font_tool.py check \
      --chars build/crazypod-chars.txt --font SourceHanSansSC-VF.otf
  python3 tools/crazypod_font_tool.py generate \
      --chars build/crazypod-chars.txt --font SourceHanSansSC-VF.otf \
      --size 14 --symbol lv_font_crazypod_14 \
      --output lib/lvgl/src/font/lv_font_crazypod_14.c

Any missing non-whitespace character is a fatal error.  There is no fallback.
"""

from __future__ import annotations

import argparse
import ast
import json
import re
import struct
import subprocess
import sys
import unicodedata
from pathlib import Path
from typing import Iterable


LVGL_CODEPOINT = re.compile(r'/\*\s*U\+([0-9A-Fa-f]{4,6})\s+"')
C_COMMENT = re.compile(r"/\*.*?\*/|//[^\r\n]*", re.DOTALL)
C_STRING = re.compile(r'"(?:\\.|[^"\\])*"', re.DOTALL)


def _u16(data: bytes, offset: int) -> int:
    return struct.unpack_from(">H", data, offset)[0]


def _u32(data: bytes, offset: int) -> int:
    return struct.unpack_from(">I", data, offset)[0]


def _sfnt_offset(data: bytes, face: int) -> int:
    if data[:4] != b"ttcf":
        if face:
            raise ValueError("face index is only valid for a TTC/OTC font")
        return 0
    count = _u32(data, 8)
    if face < 0 or face >= count:
        raise ValueError(f"face index {face} outside TTC face count {count}")
    return _u32(data, 12 + face * 4)


def _cmap_ranges(data: bytes, offset: int) -> set[int]:
    fmt = _u16(data, offset)
    result: set[int] = set()
    if fmt == 4:
        seg_count = _u16(data, offset + 6) // 2
        end_codes = offset + 14
        start_codes = end_codes + seg_count * 2 + 2
        id_deltas = start_codes + seg_count * 2
        id_range_offsets = id_deltas + seg_count * 2
        for index in range(seg_count):
            start = _u16(data, start_codes + index * 2)
            end = _u16(data, end_codes + index * 2)
            delta = _u16(data, id_deltas + index * 2)
            range_offset = _u16(data, id_range_offsets + index * 2)
            if start == 0xFFFF:
                continue
            for codepoint in range(start, end + 1):
                if range_offset == 0:
                    glyph = (codepoint + delta) & 0xFFFF
                else:
                    glyph_pos = (
                        id_range_offsets + index * 2 + range_offset
                        + (codepoint - start) * 2
                    )
                    glyph = _u16(data, glyph_pos)
                    if glyph:
                        glyph = (glyph + delta) & 0xFFFF
                if glyph:
                    result.add(codepoint)
    elif fmt in (12, 13):
        groups = _u32(data, offset + 12)
        for index in range(groups):
            group = offset + 16 + index * 12
            start, end, glyph = struct.unpack_from(">III", data, group)
            if fmt == 13 and glyph == 0:
                continue
            result.update(range(start, end + 1))
    return result


def font_codepoints(path: Path, face: int = 0) -> set[int]:
    data = path.read_bytes()
    sfnt = _sfnt_offset(data, face)
    table_count = _u16(data, sfnt + 4)
    cmap_offset = None
    for index in range(table_count):
        record = sfnt + 12 + index * 16
        if data[record:record + 4] == b"cmap":
            cmap_offset = _u32(data, record + 8)
            break
    if cmap_offset is None:
        raise ValueError(f"{path}: no cmap table")

    result: set[int] = set()
    subtable_count = _u16(data, cmap_offset + 2)
    for index in range(subtable_count):
        record = cmap_offset + 4 + index * 8
        platform = _u16(data, record)
        encoding = _u16(data, record + 2)
        if platform not in (0, 3) or (platform == 3 and encoding not in (1, 10)):
            continue
        subtable = cmap_offset + _u32(data, record + 4)
        result.update(_cmap_ranges(data, subtable))
    return result


def lvgl_codepoints(path: Path) -> set[int]:
    return {int(value, 16) for value in LVGL_CODEPOINT.findall(
        path.read_text(encoding="utf-8", errors="strict")
    )}


def _json_strings(value: object) -> Iterable[str]:
    if isinstance(value, str):
        yield value
    elif isinstance(value, list):
        for item in value:
            yield from _json_strings(item)
    elif isinstance(value, dict):
        for key, item in value.items():
            # Translation keys are identifiers, not rendered UI.
            if key in ("comment", "note", "description", "_meta"):
                continue
            yield from _json_strings(item)


def input_strings(path: Path) -> Iterable[str]:
    if path.suffix.lower() == ".json":
        yield from _json_strings(json.loads(path.read_text(encoding="utf-8")))
    elif path.suffix.lower() in (".c", ".h", ".inc"):
        source = C_COMMENT.sub("", path.read_text(encoding="utf-8", errors="strict"))
        for literal in C_STRING.findall(source):
            try:
                yield ast.literal_eval(literal)
            except (SyntaxError, ValueError):
                raise ValueError(f"{path}: unsupported C string literal {literal!r}")
    else:
        yield path.read_text(encoding="utf-8", errors="strict")


def required_codepoints(paths: list[Path]) -> set[int]:
    result: set[int] = set()
    for path in paths:
        for text in input_strings(path):
            normalized = unicodedata.normalize("NFC", text)
            result.update(ord(char) for char in normalized if not char.isspace())
    return result


def read_chars(path: Path) -> set[int]:
    text = unicodedata.normalize("NFC", path.read_text(encoding="utf-8"))
    return {ord(char) for char in text if not char.isspace()}


def printable(codepoint: int) -> str:
    char = chr(codepoint)
    name = unicodedata.name(char, "UNNAMED")
    return f"U+{codepoint:04X} {char!r} {name}"


def write_chars(path: Path, codepoints: set[int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(map(chr, sorted(codepoints))) + "\n", encoding="utf-8")


def command_collect(args: argparse.Namespace) -> int:
    codepoints = required_codepoints(args.input)
    write_chars(args.output, codepoints)
    print(f"{args.output}: {len(codepoints)} required non-whitespace characters")
    return 0


def command_check(args: argparse.Namespace) -> int:
    required = read_chars(args.chars)
    available: set[int] = set()
    for path in args.font:
        available.update(font_codepoints(path, args.face))
    for path in args.lvgl_c:
        available.update(lvgl_codepoints(path))
    missing = sorted(required - available)
    if missing:
        print(
            f"ERROR: {len(missing)} required characters are absent from all "
            "provided fonts:",
            file=sys.stderr,
        )
        for codepoint in missing:
            print(f"  {printable(codepoint)}", file=sys.stderr)
        return 1
    print(
        f"OK: all {len(required)} required characters are covered by "
        f"{len(args.font) + len(args.lvgl_c)} font artifact(s)"
    )
    return 0


def _range_argument(codepoints: set[int]) -> str:
    ordered = sorted(codepoints)
    if not ordered:
        raise ValueError("character manifest is empty")
    ranges: list[str] = []
    start = previous = ordered[0]
    for current in ordered[1:]:
        if current == previous + 1:
            previous = current
            continue
        ranges.append(str(start) if start == previous else f"{start}-{previous}")
        start = previous = current
    ranges.append(str(start) if start == previous else f"{start}-{previous}")
    return ",".join(ranges)


def command_generate(args: argparse.Namespace) -> int:
    required = read_chars(args.chars)
    generated_symbol = args.output.stem
    if generated_symbol != args.symbol:
        print(
            f"ERROR: output stem {generated_symbol!r} must equal requested "
            f"symbol {args.symbol!r}",
            file=sys.stderr,
        )
        return 1
    available = font_codepoints(args.font, args.face)
    missing = sorted(required - available)
    if missing:
        print(f"ERROR: source font lacks {len(missing)} required characters:", file=sys.stderr)
        for codepoint in missing:
            print(f"  {printable(codepoint)}", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        args.converter,
        "--no-compress",
        "--no-prefilter",
        "--bpp", str(args.bpp),
        "--size", str(args.size),
        "--font", str(args.font),
        "-r", _range_argument(required),
        "--format", "lvgl",
        "-o", str(args.output),
        "--force-fast-kern-format",
    ]
    if args.converter == "npx":
        command[1:1] = ["--yes", "lv_font_conv@1.5.3"]
    subprocess.run(command, check=True)
    generated_text = args.output.read_text(encoding="utf-8")
    generated_text = generated_text.replace(
        '#include "lvgl/lvgl.h"', '#include "../../lvgl.h"'
    )
    args.output.write_text(generated_text, encoding="utf-8")

    generated = lvgl_codepoints(args.output)
    missing_after = sorted(required - generated)
    if missing_after:
        print(
            f"ERROR: generated font lacks {len(missing_after)} required "
            "characters; output is not usable",
            file=sys.stderr,
        )
        return 1
    print(f"{args.output}: generated {len(generated)} glyphs")
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    subparsers = result.add_subparsers(dest="command", required=True)

    collect = subparsers.add_parser("collect", help="extract required characters")
    collect.add_argument("--input", type=Path, action="append", required=True)
    collect.add_argument("--output", type=Path, required=True)
    collect.set_defaults(func=command_collect)

    check = subparsers.add_parser("check", help="fail if any character is missing")
    check.add_argument("--chars", type=Path, required=True)
    check.add_argument("--font", type=Path, action="append", default=[])
    check.add_argument("--lvgl-c", type=Path, action="append", default=[])
    check.add_argument("--face", type=int, default=0)
    check.set_defaults(func=command_check)

    generate = subparsers.add_parser("generate", help="generate and verify LVGL C")
    generate.add_argument("--chars", type=Path, required=True)
    generate.add_argument("--font", type=Path, required=True)
    generate.add_argument("--face", type=int, default=0)
    generate.add_argument("--size", type=int, required=True)
    generate.add_argument("--bpp", type=int, default=4)
    generate.add_argument("--symbol", required=True)
    generate.add_argument("--output", type=Path, required=True)
    generate.add_argument(
        "--converter", default="npx",
        help="lv_font_conv executable, or npx for pinned lv_font_conv@1.5.3",
    )
    generate.set_defaults(func=command_generate)
    return result


def main() -> int:
    args = parser().parse_args()
    if args.command == "check" and not (args.font or args.lvgl_c):
        print("ERROR: check requires --font and/or --lvgl-c", file=sys.stderr)
        return 2
    try:
        return args.func(args)
    except (OSError, ValueError, json.JSONDecodeError, struct.error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as error:
        print(f"ERROR: font converter exited {error.returncode}", file=sys.stderr)
        return error.returncode or 2


if __name__ == "__main__":
    raise SystemExit(main())
