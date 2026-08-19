#!/bin/sh
set -eu

python3 - <<'PY'
from pathlib import Path
import re
import sys

root = Path("apps/crazypod")
ui = root / "ui"
main = root / "crazypod_ui.c"
entry = root / "crazypod_main.c"
limits = root / "crazypod_runtime_limits.h"
errors = []

for name in (
    "controllers", "screens", "preview", "routes",
    "material", "menu", "media",
):
    path = ui / name
    if path.exists():
        errors.append(f"forbidden horizontal directory: {path}")

for path in (
    ui / "features" / "home",
    ui / "features" / "wallpaper",
):
    if path.exists():
        errors.append(f"forbidden feature directory: {path}")

line_count = len(main.read_text().splitlines())
if not 400 <= line_count <= 1500:
    errors.append(
        f"{main} has {line_count} lines; expected 400..1500")

main_text = main.read_text()
if re.search(r"^\s*case\b", main_text, re.MULTILINE):
    errors.append(f"{main} contains feature route/input switch cases")
if re.search(
    r"^\s*static\s+lv_obj_t\s*\*\s*[A-Za-z_]\w*\s*(?:=|;)",
    main_text, re.MULTILINE,
):
    errors.append(f"{main} owns static LVGL object state")

entry_text = entry.read_text()
limits_text = limits.read_text()
if "crazypod_ui_thread_stack[CRAZYPOD_UI_THREAD_STACK_SIZE]" \
        not in entry_text:
    errors.append(f"{entry} does not own a dedicated native UI stack")
if "create_thread(" not in entry_text or \
        '"crazypod_ui"' not in entry_text:
    errors.append(f"{entry} does not launch the native UI thread")
if "CRAZYPOD_UI_THREAD_STACK_SIZE (384u * 1024u)" \
        not in limits_text:
    errors.append("native UI stack is not the reviewed 384 KiB")

features = ui / "features"
feature_names = sorted(
    path.name for path in features.iterdir() if path.is_dir())
for path in features.rglob("*"):
    if path.suffix not in (".c", ".h"):
        continue
    owner = path.relative_to(features).parts[0]
    for number, line in enumerate(
        path.read_text(errors="ignore").splitlines(), 1
    ):
        if "#include" not in line:
            continue
        for other in feature_names:
            if other == owner or f"../{other}/" not in line:
                continue
            facade = f'crazypod_{other}_feature.h"'
            if not line.rstrip().endswith(facade):
                errors.append(
                    f"{path}:{number}: cross-feature private include")

external_sources = [main]
external_sources.extend(
    path for path in ui.rglob("*")
    if path.suffix in (".c", ".h")
    and features not in path.parents
)
for path in external_sources:
    for number, line in enumerate(
        path.read_text(errors="ignore").splitlines(), 1
    ):
        if "#include" not in line or "features/" not in line:
            continue
        header = line.split("/")[-1].rstrip('>"')
        if not header.endswith("_feature.h"):
            errors.append(
                f"{path}:{number}: Feature private include outside owner")

for path in ui.rglob("*"):
    if path.suffix not in (".c", ".h"):
        continue
    for number, line in enumerate(
        path.read_text(errors="ignore").splitlines(), 1
    ):
        if re.match(r"\s*extern\s+(?!const\b).+;", line):
            errors.append(
                f"{path}:{number}: mutable extern crosses a directory")

for path in root.glob("*.c"):
    if path == main:
        continue
    for number, line in enumerate(
        path.read_text(errors="ignore").splitlines(), 1
    ):
        if "#include" in line and '"ui/' in line:
            errors.append(
                f"{path}:{number}: domain module depends on UI")

if errors:
    print("\n".join(errors), file=sys.stderr)
    raise SystemExit(1)
print(f"CrazyPod UI architecture gate passed ({line_count} root lines)")
PY
