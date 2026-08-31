#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
destination=${1:-"$repo_root/.cache/crazypod-pingfang"}
repository=refinec/PingFangSC
commit=ec0d24064c7573bea6416b00d719d4ba5342a452

weights="Ultralight Thin Light Regular Medium Semibold"

complete=true
for weight in $weights; do
    test -s "$destination/PingFangSC-$weight.ttf" || complete=false
done
if [ "$complete" = true ] &&
   grep -q "$commit" "$destination/SOURCE" 2>/dev/null &&
   test -s "$destination/LICENSE" &&
   test -s "$destination/SHA256SUMS"; then
    exit 0
fi

temporary=$(mktemp -d "${TMPDIR:-/tmp}/crazypod-pingfang.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM
mkdir -p "$destination"

for weight in $weights; do
    output="$destination/PingFangSC-$weight.ttf"
    part="$temporary/PingFangSC-$weight.ttf.part"
    curl --fail --location --retry 8 --retry-all-errors --retry-delay 2 \
        "https://raw.githubusercontent.com/$repository/$commit/ttf/PingFangSC-$weight.ttf" \
        --output "$part"
    test -s "$part"
    mv "$part" "$output"
done

curl --fail --location --retry 8 --retry-all-errors --retry-delay 2 \
    "https://raw.githubusercontent.com/$repository/$commit/LICENSE" \
    --output "$destination/LICENSE"
test -s "$destination/LICENSE"

{
    echo "$repository $commit"
    echo "source https://github.com/$repository/tree/$commit"
} > "$destination/SOURCE"

(cd "$destination" && shasum -a 256 \
    LICENSE PingFangSC-*.ttf SOURCE > SHA256SUMS)
