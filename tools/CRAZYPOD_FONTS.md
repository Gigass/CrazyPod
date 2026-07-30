# CrazyPod localization font gate

`crazypod_font_tool.py` makes font coverage a build-time invariant. It does
not silently substitute another font or ignore missing glyphs.

The manifest retains Unicode spacing separators, including ordinary
`U+0020 SPACE`. A blank glyph still needs a non-zero advance width. Tabs,
newlines, and other layout controls are excluded because LVGL handles them
outside the font.

## Source fonts

Use the official open-source Source Han Sans / Noto CJK regional fonts:

- SC for Simplified Chinese
- TC for Traditional Chinese
- JP for Japanese
- KR for Korean

The regional variants matter because shared Han code points can have different
preferred glyph shapes. Do not generate all four localized fonts from the SC
file. The Latin Extended characters needed by German, French, Spanish and
Brazilian Portuguese may be included in each regional font, or generated from
an OFL-licensed Latin font such as Montserrat.

The repository currently uses one shared Noto Sans CJK SC subset at 8, 10, 12,
14, and 16px. It covers the complete catalog, including Japanese, Traditional
Chinese, and Korean characters, but it does not provide region-specific Han
glyph shapes. Regional font splitting remains a typography improvement, not a
coverage fix.

The 14 and 16px files retain their historical
`lv_font_source_han_sans_sc_*` names. Their current glyph sets were regenerated
from the complete nine-language manifest. The source font is not committed, so
regeneration requires a separately supplied, redistributable source.

macOS system fonts (Arial Unicode, PingFang, Hiragino and Apple SD Gothic Neo)
are useful for local coverage diagnosis, but must not be copied into the
repository or used as distributable firmware font sources.

## Required workflow

Create the complete character manifest after translations are final:

```sh
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
  --output build/fonts/crazypod-all.txt
```

JSON values, plain UTF-8 text, and C string literals are supported. The
`crazypod_l10n.c` input is mandatory because it contains native language names
such as `简体中文`; those names are not translation JSON values. Omitting that
file previously dropped `简` from the firmware font. Separate locale
manifests can reduce firmware size later, but every current generated font
must cover the complete shared manifest.

Audit the selected source before conversion:

```sh
python3 tools/crazypod_font_tool.py check \
  --chars build/fonts/crazypod-all.txt \
  --font "$CRAZYPOD_SOURCE_HAN_SC"
```

Generate a size-specific LVGL font. `npx` uses the explicitly pinned
`lv_font_conv@1.5.3`; pass `--converter /path/to/lv_font_conv` for an offline
installation of the same version.

```sh
python3 tools/crazypod_font_tool.py generate \
  --chars build/fonts/crazypod-all.txt \
  --font "$CRAZYPOD_SOURCE_HAN_SC" \
  --size 14 \
  --symbol lv_font_source_han_sans_sc_14_cjk \
  --output lib/lvgl/src/font/lv_font_source_han_sans_sc_14_cjk.c
```

Finally, audit the committed C artifact, not only the source font:

```sh
python3 tools/crazypod_font_tool.py check \
  --chars build/fonts/crazypod-all.txt \
  --lvgl-c lib/lvgl/src/font/lv_font_source_han_sans_sc_14_cjk.c
```

Run this for every font size used for localized UI text. A single successful
font-size check does not prove that the other generated sizes are complete.
The audit must report `U+0020` as covered; otherwise localized labels will
visually concatenate words even when their source strings contain spaces.
Artifact checks also reject spacing glyphs whose advance width is zero.
