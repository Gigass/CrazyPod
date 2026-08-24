#!/usr/bin/env python3

import argparse
import json
from pathlib import Path
import re
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SPEC_FILE = ROOT / "tools/crazypod-runtime-font-specs.txt"
LOCALES = ("jp", "kr", "sc", "tc")
SPEC_PATTERN = re.compile(r"(system|serif|mono):(\d{3}):(\d{1,2})")


def load_specs(path):
    specs = set()
    for line_number, raw_line in enumerate(
            path.read_text(encoding="ascii").splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if SPEC_PATTERN.fullmatch(line) is None:
            raise ValueError(f"invalid spec at {path}:{line_number}: {line}")
        if line in specs:
            raise ValueError(f"duplicate spec at {path}:{line_number}: {line}")
        specs.add(line)
    if not specs:
        raise ValueError(f"runtime font spec is empty: {path}")
    return specs


def read_manifest(package):
    try:
        with zipfile.ZipFile(package) as archive:
            return json.loads(archive.read("manifest.json"))
    except (KeyError, json.JSONDecodeError, zipfile.BadZipFile) as error:
        raise ValueError(f"invalid CPK manifest in {package}: {error}") from error


def package_font_set(package, manifest):
    if manifest.get("format") != 5 or manifest.get("runtime") != "native-aot":
        raise ValueError(f"unsupported package format in {package}")
    value = manifest.get("fontSet")
    if manifest.get("abiMinor", 0) >= 16 and not isinstance(value, str):
        raise ValueError(f"ABI 1.16+ package is missing fontSet: {package}")
    if value is None or value == "":
        return set()
    entries = value.split(",")
    if any(SPEC_PATTERN.fullmatch(entry) is None for entry in entries):
        raise ValueError(f"invalid fontSet in {package}: {value}")
    if len(entries) != len(set(entries)):
        raise ValueError(f"duplicate fontSet entry in {package}: {value}")
    return set(entries)


def audit_font_directory(font_dir, specs):
    expected = {
        f"{locale}-{spec.replace(':', '-')}.fnt"
        for locale in LOCALES
        for spec in specs
    }
    actual = {path.name for path in font_dir.glob("*.fnt")}
    missing = sorted(expected - actual)
    unexpected = sorted(actual - expected)
    if missing or unexpected:
        raise ValueError(
            f"runtime font directory mismatch; missing={missing}, "
            f"unexpected={unexpected}"
        )
    invalid = []
    for name in sorted(expected):
        path = font_dir / name
        with path.open("rb") as stream:
            if stream.read(4) != b"RB12":
                invalid.append(name)
    if invalid:
        raise ValueError(f"invalid RB12 runtime fonts: {invalid}")


def main():
    parser = argparse.ArgumentParser(
        description="Audit CPK fontSet declarations against release fonts"
    )
    parser.add_argument(
        "--spec-file", type=Path, default=DEFAULT_SPEC_FILE,
        help="canonical runtime font specification",
    )
    parser.add_argument(
        "--font-dir", type=Path, required=True,
        help="generated crazypod-aot font directory",
    )
    parser.add_argument("packages", nargs="+", type=Path)
    args = parser.parse_args()

    specs = load_specs(args.spec_file)
    audit_font_directory(args.font_dir, specs)
    requirements = set()
    for package in args.packages:
        manifest = read_manifest(package)
        package_specs = package_font_set(package, manifest)
        missing = sorted(package_specs - specs)
        if missing:
            raise ValueError(
                f"release fonts do not satisfy {package}: missing={missing}"
            )
        requirements.update(package_specs)
    print(
        f"CrazyPod runtime font audit: {len(args.packages)} CPKs, "
        f"{len(specs)} shipped tuples, {len(requirements)} requested tuples"
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)
