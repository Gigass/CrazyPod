#!/bin/sh
set -eu

cd "$(dirname "$0")"

detect_jobs() {
    if [ -n "${JOBS:-}" ]; then
        echo "$JOBS"
    elif command -v getconf >/dev/null 2>&1; then
        getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu 2>/dev/null || echo 4
    else
        echo 4
    fi
}

require_tools() {
    missing=0
    for tool in make perl zip "${CROSS_COMPILE}gcc" "${CROSS_COMPILE}objcopy"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "Error: missing required tool '$tool' on PATH." >&2
            missing=1
        fi
    done
    [ "$missing" -eq 0 ] || exit 2
}

prepare_generated_headers() {
    builddir_unix=$(pwd)
    # rbversion.h is phony when the UTC date changes. Build it separately so
    # GNU Make 3.81 cannot schedule generated files twice through other goals.
    make -j1 "$builddir_unix/rbversion.h"
    make -j1 "$builddir_unix/apps/core_asmdefs.h"
    make -j1 "$builddir_unix/ram.link"
}

INCREMENTAL=0
case "${CRAZYPOD_INCREMENTAL:-}" in
    1|yes|true|YES|TRUE) INCREMENTAL=1 ;;
esac

while [ $# -gt 0 ]; do
    case "$1" in
        -i|--incremental)
            INCREMENTAL=1
            ;;
        -h|--help)
            cat <<'EOF'
Usage: build-hw.sh [-i|--incremental]

Builds CrazyPod exclusively for the Rockbox iPod 6G target.

Environment:
  CRAZYPOD_INCREMENTAL=1  reuse build-hw-ipod6g/
  CRAZYPOD_SKIP_DEP=1     skip make dep when make.dep exists
  CROSS_COMPILE=prefix-   default arm-none-eabi-
  JOBS=N                  parallel job count
EOF
            exit 0
            ;;
        *)
            echo "Error: CrazyPod supports only iPod 6G; unsupported argument '$1'." >&2
            exit 2
            ;;
    esac
    shift
done

CROSS_COMPILE="${CROSS_COMPILE:-arm-none-eabi-}"
export CROSS_COMPILE
require_tools

BUILDDIR="build-hw-ipod6g"
STAMP="crazypod hardware ipod6g lvgl"

configure_build() {
    ../tools/configure --target=ipod6g --type=n
    printf '%s\n' "$STAMP" > .crazypod_configure_stamp
}

if [ "$INCREMENTAL" -eq 0 ]; then
    echo "CrazyPod: clean iPod 6G hardware build"
    rm -rf "$BUILDDIR"
    mkdir "$BUILDDIR"
    cd "$BUILDDIR"
    configure_build
else
    echo "CrazyPod: incremental iPod 6G hardware build"
    mkdir -p "$BUILDDIR"
    cd "$BUILDDIR"
    if [ ! -f Makefile ] ||
       [ ! -f .crazypod_configure_stamp ] ||
       [ "$(cat .crazypod_configure_stamp)" != "$STAMP" ]; then
        configure_build
    fi
fi

if [ -n "${CRAZYPOD_SKIP_DEP:-}" ] && [ -f make.dep ]; then
    echo "CrazyPod: reusing make.dep"
else
    make dep
fi

prepare_generated_headers
make -j"$(detect_jobs)"

if [ ! -f rockbox.ipod ]; then
    echo "Error: hardware build did not produce rockbox.ipod." >&2
    exit 1
fi

PACKAGE_DIR="$(mktemp -d)"
trap 'rm -rf "$PACKAGE_DIR"' EXIT HUP INT TERM
if [ ! -d ../assets/crazypod-icons ]; then
    echo "Error: missing generated CrazyPod icon assets." >&2
    exit 1
fi
if [ ! -f ../assets/crazypod/default-home.bmp ]; then
    echo "Error: missing generated CrazyPod default wallpaper." >&2
    exit 1
fi
mkdir -p "$PACKAGE_DIR/.rockbox/codecs"
mkdir -p "$PACKAGE_DIR/.rockbox/crazypod/icons"
cp rockbox.ipod "$PACKAGE_DIR/.rockbox/rockbox.ipod"
[ ! -f rockbox-info.txt ] || cp rockbox-info.txt "$PACKAGE_DIR/.rockbox/rockbox-info.txt"
cp -R ../assets/crazypod-icons/. \
    "$PACKAGE_DIR/.rockbox/crazypod/icons/"
cp ../assets/crazypod/default-home.bmp \
    "$PACKAGE_DIR/.rockbox/crazypod/default-home.bmp"
for codec in lib/rbcodec/codecs/*.codec; do
    [ -f "$codec" ] || continue
    case "$codec" in
        *_enc.codec) continue ;;
    esac
    cp "$codec" "$PACKAGE_DIR/.rockbox/codecs/"
done
rm -f CrazyPod-6G.zip
(
    cd "$PACKAGE_DIR"
    zip -q -r "$PACKAGE_DIR/../CrazyPod-6G.zip" .rockbox
)
mv "$PACKAGE_DIR/../CrazyPod-6G.zip" CrazyPod-6G.zip

echo "CrazyPod: built $(pwd)/rockbox.ipod"
echo "CrazyPod: packaged $(pwd)/CrazyPod-6G.zip"
