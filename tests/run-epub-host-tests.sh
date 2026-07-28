#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

zip_sources="
    firmware/common/zip.c
    firmware/common/inflate.c
    firmware/common/crc32.c
    firmware/common/adler32.c
"

cc -std=c11 -Wall -Wextra -Werror \
    -include tests/epub-host-stubs/config.h \
    -DEPUB_CACHE_PARENT="\"$test_root/cache\"" \
    -DEPUB_CACHE_DIRECTORY="\"$test_root/cache/books\"" \
    -Itests/epub-host-stubs \
    -Iapps/crazypod \
    -Ifirmware/include \
    tests/epub_parser_host_test.c \
    apps/crazypod/epub/crazypod_epub_html.c \
    apps/crazypod/epub/crazypod_epub_navigation.c \
    apps/crazypod/epub/crazypod_epub_parser.c \
    apps/crazypod/epub/cache/crazypod_epub_cache.c \
    apps/crazypod/epub/cache/crazypod_epub_cover_store.c \
    apps/crazypod/epub/extraction/crazypod_epub_extraction.c \
    $zip_sources \
    -o "$test_root/epub_parser_test"
cc -std=c11 -Wall -Wextra -Werror \
    -include tests/epub-host-stubs/config.h \
    -Itests/epub-host-stubs \
    -Ifirmware/include \
    tests/zip_extract_host_test.c \
    $zip_sources \
    -o "$test_root/zip_extract_test"

python3 - "$test_root/traversal.epub" <<'PY'
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1], "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("../escape.txt", "must not escape extraction root")
PY
mkdir "$test_root/traversal"
if "$test_root/zip_extract_test" \
       "$test_root/traversal.epub" "$test_root/traversal" \
       >/dev/null 2>&1; then
    echo "path traversal archive was accepted" >&2
    exit 1
fi
test ! -e "$test_root/escape.txt"

python3 - "$test_root/corrupt.epub" <<'PY'
import struct
import sys
import zipfile

path = sys.argv[1]
with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
    archive.writestr("broken.txt", b"EPUB integrity check " * 2048)
with open(path, "r+b") as archive:
    header = archive.read(30)
    compressed_size = struct.unpack_from("<I", header, 18)[0]
    name_size, extra_size = struct.unpack_from("<HH", header, 26)
    data_offset = 30 + name_size + extra_size
    archive.seek(data_offset + compressed_size // 2)
    value = archive.read(1)
    archive.seek(-1, 1)
    archive.write(bytes([value[0] ^ 0x01]))
PY
mkdir "$test_root/corrupt"
if "$test_root/zip_extract_test" \
       "$test_root/corrupt.epub" "$test_root/corrupt" \
       >/dev/null 2>&1; then
    echo "corrupt archive was accepted" >&2
    exit 1
fi

run_sample() {
    name=$1
    url=$2
    archive="$test_root/$name.epub"
    extracted="$test_root/$name"

    curl -fsSL "$url" -o "$archive"
    mkdir "$extracted"
    "$test_root/zip_extract_test" "$archive" "$extracted"
    "$test_root/epub_parser_test" "$extracted" "$archive"
}

run_sample \
    moby-dick \
    https://github.com/IDPF/epub3-samples/releases/download/20230704/moby-dick.epub
run_sample \
    wasteland \
    https://github.com/IDPF/epub3-samples/releases/download/20230704/wasteland.epub
run_sample \
    alice-epub2 \
    https://www.gutenberg.org/cache/epub/11/pg11-images.epub
