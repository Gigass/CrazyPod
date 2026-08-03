#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
destination=${1:-"$repo_root/.cache/crazypod-noto"}
cjk_commit=f8d157532fbfaeda587e826d4cd5b21a49186f7c
core_commit=ffebf8c1ee449e544955a7e813c54f9b73848eac

sans_cjk_weights="Thin Light DemiLight Regular Medium Bold Black"
serif_cjk_weights="ExtraLight Light Regular Medium SemiBold Bold Black"
core_weights="Thin ExtraLight Light Regular Medium SemiBold Bold ExtraBold Black"

complete=true
for weight in $sans_cjk_weights; do
    test -s "$destination/NotoSansCJK-$weight.ttc" || complete=false
done
for weight in $serif_cjk_weights; do
    test -s "$destination/NotoSerifCJK-$weight.ttc" || complete=false
done
for family in NotoSans NotoSerif NotoSansMono; do
    for weight in $core_weights; do
        test -s "$destination/$family-$weight.ttf" || complete=false
    done
done
if [ "$complete" = true ] &&
   grep -q "$cjk_commit" "$destination/SOURCE" 2>/dev/null &&
   grep -q "$core_commit" "$destination/SOURCE" 2>/dev/null; then
    exit 0
fi

temporary=$(mktemp -d "${TMPDIR:-/tmp}/crazypod-noto.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
mkdir -p "$destination"

extract_file()
{
    repository=$1
    repository_name=$2
    commit=$3
    source_path=$4
    output=$5
    part="$output.part"

    if [ -s "$output" ]; then
        return
    fi
    rm -f "$part"
    if git -C "$repository" show "$commit:$source_path" > "$part" &&
       [ -s "$part" ]; then
        mv "$part" "$output"
        return
    fi
    rm -f "$part"
    curl --fail --location --retry 8 --retry-all-errors --retry-delay 2 \
        "https://raw.githubusercontent.com/notofonts/$repository_name/$commit/$source_path" \
        --output "$part"
    test -s "$part"
    mv "$part" "$output"
}

cjk_repo=${CRAZYPOD_NOTO_CJK_REPO:-"$temporary/cjk"}
core_repo=${CRAZYPOD_NOTO_CORE_REPO:-"$temporary/core"}
if [ ! -d "$cjk_repo/.git" ]; then
    git clone --quiet --depth 1 --filter=blob:none --no-checkout \
        https://github.com/notofonts/noto-cjk.git "$cjk_repo"
    git -C "$cjk_repo" fetch --quiet --depth 1 origin "$cjk_commit"
fi
for weight in $sans_cjk_weights; do
    extract_file "$cjk_repo" noto-cjk "$cjk_commit" \
        "Sans/OTC/NotoSansCJK-$weight.ttc" \
        "$destination/NotoSansCJK-$weight.ttc"
done
for weight in $serif_cjk_weights; do
    extract_file "$cjk_repo" noto-cjk "$cjk_commit" \
        "Serif/OTC/NotoSerifCJK-$weight.ttc" \
        "$destination/NotoSerifCJK-$weight.ttc"
done
extract_file "$cjk_repo" noto-cjk "$cjk_commit" "Sans/LICENSE" \
    "$destination/OFL-Noto-CJK.txt"

if [ ! -d "$core_repo/.git" ]; then
    git clone --quiet --depth 1 --filter=blob:none --no-checkout \
        https://github.com/notofonts/noto-fonts.git "$core_repo"
    git -C "$core_repo" fetch --quiet --depth 1 origin "$core_commit"
fi
for family in NotoSans NotoSerif NotoSansMono; do
    for weight in $core_weights; do
        extract_file "$core_repo" noto-fonts "$core_commit" \
            "hinted/ttf/$family/$family-$weight.ttf" \
            "$destination/$family-$weight.ttf"
    done
done
extract_file "$core_repo" noto-fonts "$core_commit" "LICENSE" \
    "$destination/OFL-Noto-Core.txt"

{
    echo "noto-cjk $cjk_commit"
    echo "noto-fonts $core_commit"
} > "$destination/SOURCE"

(cd "$destination" && shasum -a 256 \
    Noto*.ttf Noto*.ttc OFL-*.txt SOURCE > SHA256SUMS)
