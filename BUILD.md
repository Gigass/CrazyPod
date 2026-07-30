# CrazyPod build guide

CrazyPod supports one product target: iPod Classic 6G (`ipod6g`).

## Prerequisites

Simulator:

- GNU make
- GCC
- Perl
- Python 3
- OpenSSL 3
- SDL2 development files (`sdl2-config` and `pkg-config`)

Hardware:

- GNU make
- Perl
- Python 3
- OpenSSL 3
- `zip`
- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`
- `arm-none-eabi-nm`

Optional video conversion:

- FFmpeg, including `ffmpeg` and `ffprobe`

The current scripts have been verified on macOS. The inherited PowerShell
scripts are not the release path for this LVGL product revision.

## Simulator

```sh
./build-sim.sh
./build-sim.sh --incremental
```

Run from the build directory so the simulated disk resolves correctly:

```sh
cd build-sim
./rockboxui
```

| Input | Key |
| --- | --- |
| Wheel counter-clockwise | Up |
| Wheel clockwise | Down |
| Left / Right | Left / Right |
| Select | Return |
| Menu / Back | Backspace or Escape |
| Play | Space |

Simulator-only framebuffer snapshots can open a deterministic route and write
the real 320×240 RGB565 output to `build-sim/simdisk/dump *.bmp`:

```sh
CRAZYPOD_SIM_DUMP=1 CRAZYPOD_SIM_SCREEN=clock \
  "build-sim/CrazyPod Simulator.app/Contents/MacOS/CrazyPod Simulator"
```

Supported routes are `home`, `power`, `more`, `more-second`,
`settings-main-menu`, `settings-language`, `settings-reduce-motion`, `notes`,
`note-compose`,
`notes-new`, `notes-draft`, `notes-item`, `notes-search`,
`notes-deleted`, `books`, `books-reading`, `book-reader`,
`book-reader-next`, `clock`, `stopwatch`, `workouts`, `workout-ready`,
`workout-active`, `workout-detail`, `calendar`, `calendar-day`,
`contacts`, `contact-detail`, `calculator`, and `pomodoro`. First-level preview
variants also support `music-0` through `music-7`, `media-0` through
`media-2` (`photos-N` remains an alias), and `books-N`. Video routes use
`videos-N` for a list frame and `play-video-N` for a playback smoke test.
Numeric indices are clamped to the current route. Routes that depend on
content use files from the simulated disk.

Set `CRAZYPOD_SIM_LANGUAGE` to `en`, `zh-Hans`, `zh-Hant`, `ja`, `ko`, `de`,
`fr`, `es`, or `pt-BR` to capture the same route in a specific language.

## Hardware

```sh
./build-hw.sh
./build-hw.sh --incremental
```

The script rejects target arguments because no target other than `ipod6g` is
supported.

## Verification

Run the structural and host tests from the repository root:

```sh
sh tests/check-crazypod-ui-architecture.sh
sh tests/run-crazypod-ui-host-tests.sh
sh tests/run-miniapp-host-tests.sh
sh tests/run-epub-host-tests.sh
sh tests/run-crazypod-font-tests.sh
python3 tools/check-crazypod-l10n.py --strict-bare
git diff --check
```

The UI host test covers collation, A-Z wheel-jump state, route dispatch,
navigation commands, menu layout, and text helpers. The architecture gate
requires `crazypod_ui.c` to remain between 400 and 800 lines and rejects
feature-private includes outside their owner.

For a user-visible change, also run the simulator and exercise the affected
route. For LCD, storage, USB, power, audio, or native Mini App changes, an ARM
build proves compilation only; record physical iPod results separately.

## Localization

English source keys and locale resources live under `localization/crazypod`.
After changing them, regenerate both firmware and Mini App lookup tables, then
run the strict audit:

```sh
python3 tools/generate-crazypod-l10n.py
python3 tools/check-crazypod-l10n.py --strict-bare
```

The generator rejects missing keys and mismatched format placeholders. The
audit also rejects untagged user-facing strings in common UI sinks.

Localized fonts are committed build inputs at 8, 10, 12, 14, and 16px. Font
regeneration requires `lv_font_conv` 1.5.3 plus a distributable CJK source
font. Follow [tools/CRAZYPOD_FONTS.md](tools/CRAZYPOD_FONTS.md) and include
`apps/crazypod/crazypod_l10n.c` when collecting characters; it contains native
language names that are not present as translation values.

## Bootloader

The normal hardware archive does not replace the installed bootloader. Build
the CrazyPod bootloader separately when changing the power-on screen:

```sh
./build-bootloader.sh
```

Incremental build:

```sh
./build-bootloader.sh --incremental
```

Output:

```text
build-bootloader-ipod6g/bootloader-ipod6g.ipod
```

Installing a bootloader is a separate, device-writing operation. The build
script deliberately does not flash it and `CrazyPod-6G.zip` deliberately does
not include it. The current bootloader artifact is build-verified but has not
completed physical boot regression.

Artifacts:

- `build-hw-ipod6g/rockbox.ipod`
- `build-hw-ipod6g/CrazyPod-6G.zip`
- `dist/miniapps/calculator-1.2.0.cpk`
- `dist/miniapps/pomodoro-1.2.0.cpk`

The zip deliberately contains only the firmware and the runtime resources
required by the independent product:

```text
.rockbox/rockbox.ipod
.rockbox/rockbox-info.txt
.rockbox/codecs/*.codec
.rockbox/codepages/936.cp
.rockbox/crazypod/default-home.bmp
.rockbox/crazypod/icons/<theme>/*.bmp
.rockbox/crazypod/miniapps/packages/calculator-1.2.0.cpk
.rockbox/crazypod/miniapps/packages/pomodoro-1.2.0.cpk
```

There are no Rockbox WPS files, themes, skin fonts, plugins, or recording
encoder codecs in the product package.

### Build-time checks

The hardware script stops before packaging if any of the main, IRQ, or FIQ
stack boundary symbols is missing or not 8-byte aligned. This is required by
the ARM EABI for values such as `double` and prevents variadic number
formatting from reading the wrong stack data on the iPod 6G.

The Mini App package builder requires OpenSSL 3, produces deterministic stored
ZIP entries, signs each manifest with Ed25519, and records SHA-256 hashes for
the payload, icon, and resource container.

Check the generated archive and record its hashes before copying it:

```sh
unzip -tq build-hw-ipod6g/CrazyPod-6G.zip
shasum -a 256 \
  build-hw-ipod6g/rockbox.ipod \
  build-hw-ipod6g/CrazyPod-6G.zip \
  dist/miniapps/*.cpk
```

Portable DIY appearances use fixed USB-visible paths:

- Import: copy one file to `/.crazypod/import.upodtheme`, then choose
  Customize → Presets → Import.
- Export: choose Export on a saved appearance; CrazyPod writes it under
  `/.crazypod/export/`.

## Installation warning

Development builds have been installed on an iPod Classic 6G and checked
against their local artifacts with SHA-256 and per-file comparisons. That
proves the copy completed; it does not prove every current behavior is safe or
regression-free.

Before device testing:

1. Confirm the mounted volume belongs to the intended iPod.
2. Back up its existing `.rockbox` and `.crazypod` directories outside the
   repository.
3. Keep a known-good firmware and bootloader recovery procedure.
4. Check the data volume before writing. If directory entries repeat, files
   disappear, or file sizes change between reads, stop and repair the
   filesystem only after backing up every readable file.
5. Copy only the generated `.rockbox` package. Do not use a delete-mirroring
   operation, erase user content, or rewrite the boot partition.
6. Write `rockbox.ipod` last, sync, and compare the installed firmware and
   `.cpk` files against their local hashes.
7. Check the data volume again and safely eject the whole device.

CrazyPod does not yet provide a supported end-user installer. See
[PROJECT_STATUS.md](PROJECT_STATUS.md) for the current validation record.

## Environment variables

| Variable | Purpose |
| --- | --- |
| `JOBS=N` | Parallel compile jobs |
| `CRAZYPOD_INCREMENTAL=1` | Reuse the hardware build directory |
| `CRAZYPOD_SKIP_DEP=1` | Reuse an existing hardware `make.dep` |
| `ROCKPOD_INCREMENTAL=1` | Reuse the simulator build directory |
| `ROCKPOD_SKIP_DEP=1` | Reuse an existing simulator `make.dep` |
| `CROSS_COMPILE=prefix-` | Override `arm-none-eabi-` |
| `CRAZYPOD_OPENSSL=/path/to/openssl` | Select an OpenSSL 3 executable |
