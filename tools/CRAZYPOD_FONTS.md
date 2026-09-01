# CrazyPod localization font gate

`crazypod_font_tool.py` makes font coverage a build-time invariant. It does
not silently substitute another font or ignore missing glyphs.

The manifest retains Unicode spacing separators, including ordinary
`U+0020 SPACE`. A blank glyph still needs a non-zero advance width. Tabs,
newlines, and other layout controls are excluded because LVGL handles them
outside the font.

## Source fonts

The product's Simplified Chinese system face is PingFang SC, fetched at the
pinned commit recorded by `tools/fetch-crazypod-pingfang.sh` from
`refinec/PingFangSC`. The six requested weights are kept in the build cache;
the source font files are not committed to the repository. The fetcher also
stores the upstream repository license, source revision, and SHA-256 manifest
alongside the cache.

For the fixed-font workflow, set `CRAZYPOD_PINGFANG_SC` to the cached
`PingFangSC-Regular.ttf` and `CRAZYPOD_NOTO_SC` to the cached
`NotoSansCJKsc-Regular.otf`. The latter is fetched by
`tools/fetch-crazypod-noto.sh` from the same pinned Noto CJK revision.

PingFang SC does not contain the complete Japanese/Korean catalog used by
CrazyPod. Fixed LVGL fonts therefore merge PingFang SC first and the pinned
Noto Sans CJK SC face only for missing code points. The resulting artifact is
fully covered while the common Simplified Chinese glyphs use PingFang SC.

Runtime semantic fonts use PingFang SC for the `system`/`sc` combination.
`system` in `jp`, `kr`, and `tc`, plus the `serif` and `mono` families, retain
their regional Noto faces so locale-specific glyph shapes and coverage remain
correct. The runtime resolver supplies a canonical system line box because
PingFang SC and Noto CJK have different native vertical metrics.

The Latin Extended characters needed by German, French, Spanish and Brazilian
Portuguese remain covered by the Noto regional faces. The fixed product fonts
are not valid fonts for arbitrary media metadata; that text continues to use
the runtime font service.

The 14 and 16px files retain their historical
`lv_font_source_han_sans_sc_*` names for LVGL/Kconfig compatibility. Their
generated contents now use PingFang SC first and Noto SC as the explicit
coverage fallback.

Arbitrary song, artist, album, playlist, book, theme, and Mini App text uses
the CrazyPod Noto font service. Devtool converts the exact semantic
`family:weight:size` tuples used by a CPK into regional RB12 bitmap fonts before
installation. The firmware package includes the 47 tuples used by the system,
bundled Mini Apps, and bundled themes. Firmware loads glyphs through Rockbox's
bounded bitmap cache; it does not parse or rasterize Noto outlines. `fontWeight`
selects 100–900; CJK requests use the nearest available physical Noto weight.
Locale selects SC, TC, JP, or KR faces with identical line metrics.

The public API exposes only `system`, `serif`, and `mono`. The old 23 Rockbox
font names remain numeric ABI values for old packages but are no longer a
development API. Latin, Greek, Cyrillic, CJK, Japanese, and Korean use matched
Noto faces. RTL and complex-script shaping remain outside the text contract.

ABI 1.16 CPK manifests declare a compiler-generated `fontSet`. Standard
Devtool staging installs every missing regional font before the CPK. The
firmware installer rejects a package whose declared files are missing instead
of substituting another size or weight. `fontSize` is static under AOT; text
size animation uses transforms.

The canonical base-firmware tuple list is
`tools/crazypod-runtime-font-specs.txt`. Font generation, structural tests,
and release-package audits all consume that file. Every CPK in `dist/miniapps`
must declare a `fontSet` satisfied by the base firmware before a hardware
release can be packaged.

Native AOT MiniApps and now-playing themes may also package private fonts.
Their `assets.json` source must be TTF, OTF, TTC, or BDF and the TSX reference
is `fontFamily: "asset:<resource-id>"`. The Devtool converts package-private
source fonts to RB12 at build time. Converted
fonts are structurally validated during package installation, materialized in
the package's private install directory, and named with the resource CRC so a
same-ID replacement cannot reuse stale bytes. Each active package may load at
most four private fonts. Missing glyphs fall through to the Noto system face
at the asset font's pixel height.

The PingFang source cache is a build input, not a runtime outline-font store;
the firmware only ships converted LVGL/RB12 bitmap data. Preserve
`PingFangSC-LICENSE.txt` and `PingFangSC-SOURCE` in release font packages.

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
  --font "$CRAZYPOD_PINGFANG_SC" \
  --font "$CRAZYPOD_NOTO_SC"
```

Generate a size-specific LVGL font. `npx` uses the explicitly pinned
`lv_font_conv@1.5.3`; pass `--converter /path/to/lv_font_conv` for an offline
installation of the same version.

```sh
python3 tools/crazypod_font_tool.py generate \
  --chars build/fonts/crazypod-all.txt \
  --font "$CRAZYPOD_PINGFANG_SC" \
  --fallback-font "$CRAZYPOD_NOTO_SC" \
  --size 14 \
  --symbol lv_font_source_han_sans_sc_14_cjk \
  --output lib/lvgl/src/font/lv_font_source_han_sans_sc_14_cjk.c
```

The generator accepts one or more `--fallback-font` inputs. Each fallback is
used only for characters absent from the preceding sources, so the first
source wins for shared glyphs. This also keeps the command line short enough
for Windows hosts by passing the manifest as `--symbols` internally.

Finally, audit the committed C artifact, not only the source fonts:

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
