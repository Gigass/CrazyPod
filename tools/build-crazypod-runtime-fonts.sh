#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 OUTPUT_FONT_DIR" >&2
    exit 2
fi

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir=${CRAZYPOD_NOTO_DIR:-"$repo_root/.cache/crazypod-noto"}
cache_dir=${CRAZYPOD_AOT_FONT_CACHE:-"$repo_root/.cache/crazypod-noto-aot"}
destination="$1/crazypod-aot"
converter=${CRAZYPOD_CONVTTF:-"$repo_root/tools/convttf"}
# Bump this whenever convttf changes the meaning of stored glyph metrics.
# Prefixing the cache entry keeps older artifacts available but unusable.
cache_revision=advance-bearing-v1

if [ ! -f "$source_dir/SHA256SUMS" ]; then
    "$repo_root/tools/fetch-crazypod-noto.sh" "$source_dir"
fi
if [ ! -x "$converter" ]; then
    echo "Error: missing convttf '$converter'." >&2
    exit 1
fi
(cd "$source_dir" && shasum -a 256 -c SHA256SUMS >/dev/null)

mkdir -p "$cache_dir" "$destination"

physical_weight()
{
    family=$1
    weight=$2
    case "$family:$weight" in
        serif:100|serif:200) echo ExtraLight ;;
        serif:300) echo Light ;;
        serif:400) echo Regular ;;
        serif:500) echo Medium ;;
        serif:600) echo SemiBold ;;
        serif:700) echo Bold ;;
        serif:800|serif:900) echo Black ;;
        mono:100) echo Thin ;;
        mono:200) echo Light ;;
        mono:300) echo DemiLight ;;
        mono:400) echo Regular ;;
        mono:500) echo Medium ;;
        mono:600|mono:700) echo Bold ;;
        mono:800|mono:900) echo Black ;;
        system:100) echo Thin ;;
        system:200) echo Light ;;
        system:300) echo DemiLight ;;
        system:400) echo Regular ;;
        system:500) echo Medium ;;
        system:600|system:700) echo Bold ;;
        system:800|system:900) echo Black ;;
        *) return 1 ;;
    esac
}

collection_name()
{
    case "$1" in
        serif) echo NotoSerifCJK ;;
        system|mono) echo NotoSansCJK ;;
        *) return 1 ;;
    esac
}

locale_face()
{
    locale=$1
    family=$2
    case "$locale" in
        jp) face=0 ;;
        kr) face=1 ;;
        sc) face=2 ;;
        tc) face=3 ;;
        *) return 1 ;;
    esac
    if [ "$family" = mono ]; then
        face=$((face + 5))
    fi
    echo "$face"
}

build_one()
{
    locale=$1
    family=$2
    weight=$3
    size=$4
    physical=$(physical_weight "$family" "$weight")
    collection=$(collection_name "$family")
    face=$(locale_face "$locale" "$family")
    source="$source_dir/$collection-$physical.ttc"
    name="$locale-$family-$weight-$size.fnt"
    cached="$cache_dir/$cache_revision-$name"

    if [ ! -f "$source" ]; then
        echo "Error: missing pinned Noto source '$source'." >&2
        exit 1
    fi
    if [ ! -f "$cached" ] ||
       [ "$(dd if="$cached" bs=4 count=1 2>/dev/null)" != RB12 ]; then
        echo "CrazyPod AOT font: $locale $family ${weight} ${size}px" >&2
        "$converter" -p "$size" -s 32 -l 65535 \
            -o "$cached" -t "$face" "$source" >/dev/null
    fi
    cp "$cached" "$destination/$name"
}

# Exact tuples used by the system UI and the eight bundled themes. Devtool
# installs additional tuples beside these when a third-party package requests
# another RN size or weight.
font_specs='system:400:6 system:400:7 system:400:8 system:400:9
system:400:10 system:400:11 system:400:12 system:400:14
system:400:15 system:400:16 system:400:18 system:400:22 system:400:24
system:400:28 system:400:32 system:400:40
system:500:32 system:700:16 system:700:32 system:900:32
serif:400:12 serif:400:14 serif:400:16 serif:400:28
serif:400:11 serif:700:14 serif:700:16 serif:700:28 serif:900:22
mono:400:7 mono:400:8 mono:400:11 mono:400:12 mono:400:16
mono:700:8 mono:700:11 mono:700:14 mono:700:22'

for spec in $font_specs; do
    family=${spec%%:*}
    remainder=${spec#*:}
    weight=${remainder%%:*}
    size=${remainder##*:}
    for locale in jp kr sc tc; do
        build_one "$locale" "$family" "$weight" "$size"
    done
done

cp "$source_dir/OFL-Noto-CJK.txt" "$destination/OFL-Noto-CJK.txt"
cp "$source_dir/SOURCE" "$destination/SOURCE"
