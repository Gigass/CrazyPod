#!/usr/bin/env python3

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
PHOTOS = ROOT / "apps/crazypod/crazypod_photos.c"
CACHE = ROOT / "apps/crazypod/photos/crazypod_photo_cache.c"
SYSTEM_PROMPTS = (
    ROOT / "apps/crazypod/ui/shell/crazypod_system_prompts.c"
)


def function_body(source: str, name: str) -> str:
    match = re.search(
        rf"\b{name}\s*\([^)]*\)\s*\{{(?P<body>.*?)^\}}",
        source,
        re.MULTILINE | re.DOTALL,
    )
    if match is None:
        raise SystemExit(f"missing function: {name}")
    return match.group("body")


photos_source = PHOTOS.read_text(encoding="utf-8")
cache_source = CACHE.read_text(encoding="utf-8")
prompts_source = SYSTEM_PROMPTS.read_text(encoding="utf-8")

invalidate_body = function_body(
    photos_source, "crazypod_photos_invalidate_catalog"
)
if "crazypod_photo_catalog_invalidate();" not in invalidate_body:
    raise SystemExit("photo catalog invalidation no longer refreshes the catalog")
if "crazypod_photo_cache_invalidate" in invalidate_body:
    raise SystemExit("USB catalog invalidation must preserve decoded photo caches")
if "crazypod_photos_invalidate_catalog();" not in prompts_source:
    raise SystemExit("USB connection no longer invalidates the photo catalog")

load_body = function_body(cache_source, "crazypod_photo_cache_load")
for identity_check in (
    "entry->key != key",
    "entry->source_size != source_size",
    "entry->source_mtime != source_mtime",
):
    if identity_check not in load_body:
        raise SystemExit(
            "retained photo caches require source identity check: "
            f"{identity_check}"
        )

print("Photo USB refresh preserves source-validated decoded caches")
