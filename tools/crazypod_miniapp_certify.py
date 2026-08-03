#!/usr/bin/env python3
"""Prepare and collect evidence for CrazyPod Mini App device certification."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


REPO_ROOT = Path(__file__).resolve().parents[1]
RELEASE_ZIP = REPO_ROOT / "build-hw-ipod6g" / "CrazyPod-6G.zip"
PACKAGES = tuple(sorted((REPO_ROOT / "dist" / "miniapps").glob("*.cpk")))
MINIMUM_FREE_BYTES = 16 * 1024 * 1024
REPRO_FILENAMES = (
    "summary.json",
    "environment.txt",
    "trace.csv",
    "frame-crc.csv",
)


class CertificationError(RuntimeError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def parse_info(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        key, separator, value = line.partition(":")
        if separator:
            values[key.strip()] = value.strip()
    return values


def validate_volume(value: str | Path) -> tuple[Path, dict[str, str]]:
    requested = Path(value).expanduser()
    try:
        volume = requested.resolve(strict=True)
    except FileNotFoundError as error:
        raise CertificationError(f"volume does not exist: {requested}") from error
    forbidden = {
        Path("/").resolve(),
        Path.home().resolve(),
        REPO_ROOT.resolve(),
        Path("/Volumes").resolve(),
    }
    if volume in forbidden or not volume.is_dir():
        raise CertificationError(f"unsafe device volume: {volume}")
    info_path = volume / ".rockbox" / "rockbox-info.txt"
    if not info_path.is_file():
        raise CertificationError(
            f"missing device Rockbox identity: {info_path}"
        )
    info = parse_info(info_path)
    if (
        info.get("Target") != "ipod6g"
        or info.get("Target id") != "71"
        or info.get("Memory") != "64"
    ):
        raise CertificationError(
            "device is not an iPod Classic 6G 64 MiB Rockbox target"
        )
    return volume, info


def artifact_report(
    release_zip: Path = RELEASE_ZIP,
    packages: tuple[Path, ...] = PACKAGES,
) -> dict[str, dict[str, int | str]]:
    paths = (release_zip, *packages)
    for path in paths:
        if not path.is_file():
            raise CertificationError(f"missing release artifact: {path}")
    try:
        with zipfile.ZipFile(release_zip) as archive:
            corrupt = archive.testzip()
            entries = archive.infolist()
            if corrupt is not None:
                raise CertificationError(
                    f"release ZIP has a corrupt entry: {corrupt}"
                )
            names = [entry.filename for entry in entries]
            if len(names) != len(set(names)):
                raise CertificationError(
                    "release ZIP contains duplicate entry names"
                )
            if any(
                not name.startswith(".rockbox/")
                or Path(name).is_absolute()
                or ".." in Path(name).parts
                for name in names
            ):
                raise CertificationError(
                    "release ZIP contains an unsafe or non-firmware entry"
                )
            names = set(names)
            required = {
                ".rockbox/rockbox.ipod",
                *{
                    ".rockbox/crazypod/miniapps/packages/"
                    f"{package.name}"
                    for package in packages
                },
            }
            missing = sorted(required - names)
            if missing:
                raise CertificationError(
                    "release ZIP is missing: " + ", ".join(missing)
                )
    except zipfile.BadZipFile as error:
        raise CertificationError("release ZIP is invalid") from error
    report: dict[str, dict[str, int | str]] = {}
    for path in paths:
        try:
            name = str(path.relative_to(REPO_ROOT))
        except ValueError:
            name = str(path)
        report[name] = {
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        }
    return report


def preflight(
    volume_value: str | Path,
    release_zip: Path = RELEASE_ZIP,
    packages: tuple[Path, ...] = PACKAGES,
) -> dict[str, object]:
    volume, info = validate_volume(volume_value)
    free_bytes = shutil.disk_usage(volume).free
    if free_bytes < MINIMUM_FREE_BYTES:
        raise CertificationError(
            f"device has only {free_bytes} free bytes"
        )
    return {
        "volume": str(volume),
        "device": info,
        "freeBytes": free_bytes,
        "artifacts": artifact_report(release_zip, packages),
    }


def copy_atomic(source: Path, destination: Path) -> None:
    temporary = destination.with_name(
        f".{destination.name}.tmp-{os.getpid()}"
    )
    if temporary.exists():
        temporary.unlink()
    try:
        with source.open("rb") as input_file, temporary.open("xb") as output:
            shutil.copyfileobj(input_file, output, 1024 * 1024)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, destination)
        directory_fd = os.open(destination.parent, os.O_RDONLY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    finally:
        if temporary.exists():
            temporary.unlink()


def copy_stream_atomic(source, destination: Path) -> None:
    temporary = destination.with_name(
        f".{destination.name}.tmp-{os.getpid()}"
    )
    if temporary.exists():
        temporary.unlink()
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        with temporary.open("xb") as output:
            shutil.copyfileobj(source, output, 1024 * 1024)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, destination)
        directory_fd = os.open(destination.parent, os.O_RDONLY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    finally:
        if temporary.exists():
            temporary.unlink()


def remove_appledouble_packages(directory: Path) -> None:
    """Remove macOS FAT sidecars that otherwise look like installable CPKs."""
    if not directory.is_dir():
        return
    for sidecar in directory.glob("._*.cpk"):
        if sidecar.is_file():
            sidecar.unlink()


def stage_packages(
    volume_value: str | Path,
    packages: tuple[Path, ...] = PACKAGES,
) -> dict[str, str]:
    volume, _ = validate_volume(volume_value)
    for package in packages:
        if not package.is_file():
            raise CertificationError(f"missing package: {package}")
    install_directory = volume / "MiniApps" / "Install"
    install_directory.mkdir(parents=True, exist_ok=True)
    staged: dict[str, str] = {}
    for package in packages:
        destination = install_directory / package.name
        copy_atomic(package, destination)
        source_hash = sha256(package)
        if sha256(destination) != source_hash:
            raise CertificationError(
                f"staged package verification failed: {destination}"
            )
        staged[str(destination)] = source_hash
    remove_appledouble_packages(install_directory)
    return staged


def install_release(
    volume_value: str | Path,
    backup_value: str | Path,
    release_zip: Path = RELEASE_ZIP,
    packages: tuple[Path, ...] = PACKAGES,
) -> dict[str, object]:
    report = preflight(volume_value, release_zip, packages)
    volume = Path(str(report["volume"]))
    backup = Path(backup_value).expanduser().resolve()
    try:
        backup.relative_to(volume)
    except ValueError:
        pass
    else:
        raise CertificationError("backup must be outside the device volume")
    if backup.exists():
        raise CertificationError(f"backup path already exists: {backup}")
    backup.mkdir(parents=True)

    package_prefix = ".rockbox/crazypod/miniapps/packages/"
    current_package_names = {package.name for package in packages}
    installed_hashes: dict[str, str] = {}
    backed_up: set[Path] = set()

    def back_up(relative: Path, source: Path) -> None:
        if relative in backed_up or not source.is_file():
            return
        destination = backup / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        backed_up.add(relative)

    package_directory = volume / package_prefix
    if package_directory.is_dir():
        for source in package_directory.iterdir():
            if source.is_file():
                back_up(
                    Path(package_prefix) / source.name,
                    source,
                )

    with zipfile.ZipFile(release_zip) as archive:
        files = [
            entry for entry in archive.infolist()
            if not entry.is_dir()
        ]
        for entry in files:
            relative = Path(entry.filename)
            if relative.is_absolute() or ".." in relative.parts:
                raise CertificationError(
                    f"unsafe release ZIP entry: {entry.filename}"
                )
            back_up(relative, volume / relative)

        package_directory.mkdir(parents=True, exist_ok=True)
        for source in package_directory.iterdir():
            if (
                source.is_file()
                and source.name.endswith(".cpk")
                and source.name not in current_package_names
            ):
                source.unlink()

        deferred = ".rockbox/rockbox.ipod"
        ordered = [
            entry for entry in files if entry.filename != deferred
        ] + [
            entry for entry in files if entry.filename == deferred
        ]
        for entry in ordered:
            destination = volume / entry.filename
            with archive.open(entry, "r") as source:
                copy_stream_atomic(source, destination)
            installed_hashes[entry.filename] = sha256(destination)

        for entry in files:
            with archive.open(entry, "r") as source:
                expected = hashlib.sha256(source.read()).hexdigest()
            if installed_hashes[entry.filename] != expected:
                raise CertificationError(
                    "installed file verification failed: "
                    f"{entry.filename}"
                )
    remove_appledouble_packages(package_directory)
    if hasattr(os, "sync"):
        os.sync()
    report["backup"] = str(backup)
    report["backedUpFiles"] = len(backed_up)
    report["installedFiles"] = len(installed_hashes)
    report["firmwareSha256"] = installed_hashes[
        ".rockbox/rockbox.ipod"
    ]
    report["packageSha256"] = {
        name: installed_hashes[f"{package_prefix}{name}"]
        for name in sorted(current_package_names)
    }
    return report


def current_commit() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=REPO_ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return result.stdout.strip()


def collect_evidence(
    volume_value: str | Path,
    output: Path,
    release_zip: Path = RELEASE_ZIP,
    packages: tuple[Path, ...] = PACKAGES,
) -> dict[str, object]:
    report = preflight(volume_value, release_zip, packages)
    volume = Path(str(report["volume"]))
    if output.exists():
        raise CertificationError(f"evidence path already exists: {output}")
    output.mkdir(parents=True)
    logs_output = output / "device-logs"
    logs_output.mkdir()
    source_logs = sorted(
        (volume / ".crazypod" / "miniapps").glob("*/miniapp.log")
    )
    copied_logs: list[Path] = []
    for source in source_logs:
        destination = logs_output / f"{source.parent.name}.log"
        shutil.copyfile(source, destination)
        copied_logs.append(destination)
    shutil.copyfile(
        volume / ".rockbox" / "rockbox-info.txt",
        output / "rockbox-info.txt",
    )
    report["collectedAtUtc"] = (
        dt.datetime.now(dt.timezone.utc).isoformat()
    )
    report["commit"] = current_commit()
    report["logs"] = [str(path.relative_to(output)) for path in copied_logs]
    repro_output = output / "miniapp-repro"
    repro_files: list[str] = []
    repro_source = volume / ".crazypod" / "repro"
    for filename in REPRO_FILENAMES:
        source = repro_source / filename
        if source.is_file():
            repro_output.mkdir(exist_ok=True)
            destination = repro_output / filename
            shutil.copyfile(source, destination)
            repro_files.append(str(destination.relative_to(output)))
    report["reproFiles"] = repro_files
    (output / "environment.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    for filename, header in (
        (
            "device-cycles.csv",
            "cycle,game_moves,lab_interactive,latency_ms,result,notes\n",
        ),
        (
            "playback-usb.csv",
            "case,player_state,usb_result,reentry_result,notes\n",
        ),
        (
            "fault-injection.csv",
            "case,result,device_booted,firmware_unchanged,notes\n",
        ),
    ):
        (output / filename).write_text(header, encoding="utf-8")
    (output / "verdict.md").write_text(
        "# CrazyPod Mini App device certification\n\n"
        "Collection does not imply certification. Replace `NOT RUN` only "
        "with linked raw evidence.\n\n"
        "| Requirement | Status | Evidence |\n"
        "| --- | --- | --- |\n"
        "| Firmware and CPK5 hash verification | NOT RUN | |\n"
        "| Cold start for 2048 and Lab | NOT RUN | |\n"
        "| Five 2048 → exit → Lab cycles | NOT RUN | |\n"
        "| Rapid 2048 input latency | NOT RUN | |\n"
        "| Playback and USB coexistence | NOT RUN | |\n"
        "| Restart persistence | NOT RUN | |\n"
        "| Invalid CPK5 rejection | NOT RUN | |\n",
        encoding="utf-8",
    )
    return report


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description=__doc__,
    )
    commands = result.add_subparsers(dest="command", required=True)
    preflight_parser = commands.add_parser(
        "preflight", help="read-only device and artifact validation"
    )
    preflight_parser.add_argument("volume")
    preflight_parser.add_argument(
        "--release", type=Path, default=RELEASE_ZIP,
        help="release ZIP to validate (defaults to production build)",
    )
    stage_parser = commands.add_parser(
        "stage", help="atomically copy all CPK5 files to MiniApps/Install"
    )
    stage_parser.add_argument("volume")
    stage_parser.add_argument(
        "--yes",
        action="store_true",
        help="confirm the package write to the validated device",
    )
    install_parser = commands.add_parser(
        "install",
        help="back up overwritten files and atomically install the release",
    )
    install_parser.add_argument("volume")
    install_parser.add_argument("--backup", required=True)
    install_parser.add_argument(
        "--release", type=Path, default=RELEASE_ZIP,
        help="release ZIP to install (defaults to production build)",
    )
    install_parser.add_argument(
        "--yes",
        action="store_true",
        help="confirm the firmware write to the validated device",
    )
    collect_parser = commands.add_parser(
        "collect", help="copy device logs into a new evidence directory"
    )
    collect_parser.add_argument("volume")
    collect_parser.add_argument("--output", type=Path)
    return result


def main() -> int:
    arguments = parser().parse_args()
    try:
        if arguments.command == "preflight":
            result = preflight(
                arguments.volume, arguments.release.resolve()
            )
        elif arguments.command == "stage":
            if not arguments.yes:
                raise CertificationError(
                    "stage requires --yes after reviewing preflight"
                )
            result = stage_packages(arguments.volume)
        elif arguments.command == "install":
            if not arguments.yes:
                raise CertificationError(
                    "install requires --yes after reviewing preflight"
                )
            result = install_release(
                arguments.volume,
                arguments.backup,
                arguments.release.resolve(),
            )
        else:
            output = arguments.output
            if output is None:
                stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
                output = (
                    REPO_ROOT
                    / "verification"
                    / f"{current_commit()[:10]}-{stamp}"
                )
            result = collect_evidence(
                arguments.volume, output.resolve()
            )
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    except (CertificationError, OSError, subprocess.CalledProcessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
