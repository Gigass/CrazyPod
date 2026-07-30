#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

cd "$repo_root"

python3 tests/test-crazypod-font-tool.py

python3 tools/crazypod_font_tool.py collect \
    --input apps/crazypod/crazypod_l10n.c \
    --input localization/crazypod/catalog.json \
    --input localization/crazypod/zh-Hans.json \
    --input localization/crazypod/zh-Hant.json \
    --input localization/crazypod/ja.json \
    --input localization/crazypod/ko.json \
    --input localization/crazypod/de.json \
    --input localization/crazypod/fr.json \
    --input localization/crazypod/es.json \
    --input localization/crazypod/pt-BR.json \
    --output "$test_root/crazypod-all.txt"

for font in \
    lib/lvgl/src/font/lv_font_crazypod_i18n_8.c \
    lib/lvgl/src/font/lv_font_crazypod_i18n_10.c \
    lib/lvgl/src/font/lv_font_crazypod_i18n_12.c \
    lib/lvgl/src/font/lv_font_source_han_sans_sc_14_cjk.c \
    lib/lvgl/src/font/lv_font_source_han_sans_sc_16_cjk.c
do
    python3 tools/crazypod_font_tool.py check \
        --chars "$test_root/crazypod-all.txt" \
        --lvgl-c "$font"
done
