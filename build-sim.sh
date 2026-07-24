#!/bin/sh
set -eu

cd "$(dirname "$0")"

detect_jobs() {
    if [ -n "${JOBS:-}" ]; then
        printf '%s\n' "$JOBS"
    elif command -v getconf >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN 2>/dev/null || printf '%s\n' 4
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu 2>/dev/null || printf '%s\n' 4
    else
        printf '%s\n' 4
    fi
}

require_tools() {
    missing=0
    for tool in make gcc perl sdl2-config pkg-config; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "Error: missing required simulator tool '$tool' on PATH." >&2
            missing=1
        fi
    done
    [ "$missing" -eq 0 ] || exit 2
}

usage() {
    cat <<'EOF'
Usage: build-sim.sh [-i|--incremental]

Builds the CrazyPod iPod 6G simulator. It does not install Rockbox themes,
skins, fonts, or plugins.

  -i, --incremental   Reuse build-sim/ when already configured.

Environment:
  ROCKPOD_INCREMENTAL=1   same as --incremental
  ROCKPOD_SKIP_DEP=1      skip make dep when make.dep exists
  JOBS=N                  parallel job count
EOF
}

incremental=0
case "${ROCKPOD_INCREMENTAL:-}" in
    1|yes|true|YES|TRUE) incremental=1 ;;
esac

while [ "$#" -gt 0 ]; do
    case "$1" in
        -i|--incremental)
            incremental=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

require_tools

builddir="build-sim"
configure_stamp="crazypod simulator ipod6g lvgl sdl-threads"

configure_build() {
    ../tools/configure --target=ipod6g --type=s --sdl-threads
    printf '%s\n' "$configure_stamp" > .crazypod-configure
}

if [ "$incremental" -eq 0 ]; then
    echo "CrazyPod: clean iPod 6G simulator build"
    rm -rf "$builddir"
    mkdir "$builddir"
    cd "$builddir"
    configure_build
else
    echo "CrazyPod: incremental iPod 6G simulator build"
    mkdir -p "$builddir"
    cd "$builddir"
    if [ ! -f Makefile ] || [ ! -f .crazypod-configure ] ||
       [ "$(cat .crazypod-configure 2>/dev/null || true)" != "$configure_stamp" ]; then
        configure_build
    fi
fi

if [ -z "${ROCKPOD_SKIP_DEP:-}" ] || [ ! -f make.dep ]; then
    make dep
fi

make -j"$(detect_jobs)"

codec_dir="lib/rbcodec/codecs"
sim_codec_dir="simdisk/.rockbox/codecs"
mkdir -p "$sim_codec_dir"
find "$sim_codec_dir" -type f -name '*.codec' -delete
for codec in "$codec_dir"/*.codec; do
    [ -f "$codec" ] || continue
    case "$codec" in
        *_enc.codec) continue ;;
    esac
    cp "$codec" "$sim_codec_dir/"
done
icon_dir="simdisk/.rockbox/crazypod/icons"
if [ ! -d ../assets/crazypod-icons ]; then
    echo "Error: missing generated CrazyPod icon assets." >&2
    exit 1
fi
if [ ! -f ../assets/crazypod/default-home.bmp ]; then
    echo "Error: missing generated CrazyPod default wallpaper." >&2
    exit 1
fi
rm -rf "$icon_dir"
mkdir -p "$icon_dir"
cp -R ../assets/crazypod-icons/. "$icon_dir/"
cp ../assets/crazypod/default-home.bmp \
   simdisk/.rockbox/crazypod/default-home.bmp

app_bundle="CrazyPod Simulator.app"
mkdir -p "$app_bundle/Contents/MacOS"
cp ../tools/crazypod-simulator/Info.plist "$app_bundle/Contents/Info.plist"
cp ../tools/crazypod-simulator/launch.sh \
   "$app_bundle/Contents/MacOS/CrazyPod Simulator"
chmod +x "$app_bundle/Contents/MacOS/CrazyPod Simulator"

echo "CrazyPod: simulator $(pwd)/rockboxui"
echo "CrazyPod: app bundle $(pwd)/$app_bundle"
