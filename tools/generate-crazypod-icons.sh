#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

SOURCE_ROOT="${MAXPOD_ASSETS_ROOT:-../../MaxPodApp/ios/MaxPodApp/Assets.xcassets}"
OUTPUT_ROOT="assets/crazypod-icons"
WALLPAPER_OUTPUT="assets/crazypod/default-home.bmp"
THEMES="basic cel_frame anime_pop mecha_spec toy y2k flat skeuo lucid_pop noize_bloom soft_skeuo acrylic ink sticker sticker2 voxel"
APPS="music podcasts mini_apps shuffle screen_lock photos diy fitness books notes clock contacts calendar stopwatch extras settings"

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "Error: ffmpeg is required to regenerate CrazyPod icon assets." >&2
    exit 2
fi
if [ ! -d "$SOURCE_ROOT" ]; then
    echo "Error: MaxPodApp asset catalog not found at $SOURCE_ROOT." >&2
    exit 2
fi

mkdir -p "$OUTPUT_ROOT"
for theme in $THEMES; do
    mkdir -p "$OUTPUT_ROOT/$theme"
    for app in $APPS; do
        source_dir="$SOURCE_ROOT/${theme}_${app}.imageset"
        source_file=$(find "$source_dir" -maxdepth 1 -type f -name '*.png' | head -n 1)
        if [ -z "$source_file" ]; then
            echo "Error: missing ${theme}_${app} source icon." >&2
            exit 1
        fi
        ffmpeg -hide_banner -loglevel error -i "$source_file" \
            -vf "scale=160:160:flags=lanczos" -pix_fmt bgra \
            -y "$OUTPUT_ROOT/$theme/$app.bmp"
    done
done

mkdir -p "$(dirname "$WALLPAPER_OUTPUT")"
ffmpeg -hide_banner -loglevel error \
    -i "$SOURCE_ROOT/default_home_wallpaper.imageset/default_home_wallpaper.jpg" \
    -vf "scale=320:240:flags=lanczos" -pix_fmt bgra \
    -y "$WALLPAPER_OUTPUT"

echo "CrazyPod: generated 16 icon themes in $OUTPUT_ROOT"
