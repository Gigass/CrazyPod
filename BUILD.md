# CrazyPod build guide

CrazyPod's shipping product target is iPod Classic 6G (`ipod6g`).

A second target, iPod Classic 5G/5.5G "Video" (`ipodvideo`), is under
bring-up. It compiles, links and packages, but it has **not** been run on
hardware. See "iPod Video bring-up target" below before using it.

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
CRAZYPOD_SIM_DUMP=1 CRAZYPOD_SIM_EXIT_AFTER_DUMP=1 \
CRAZYPOD_SIM_SCREEN=clock \
  "build-sim/CrazyPod Simulator.app/Contents/MacOS/CrazyPod Simulator"
```

Supported routes are `home`, `hold-feedback`, `notification-success`,
`notification-error`,
`power`, `coverflow`, `coverflow-power`, `more`,
`more-second`,
`settings-main-menu`, `settings-language`, `settings-reduce-motion`, `notes`,
`note-compose`,
`notes-new`, `notes-draft`, `notes-item`, `notes-search`,
`notes-deleted`, `books`, `books-reading`, `book-reader`,
`book-reader-actions`, `book-reader-next`, `clock`, `stopwatch`, `workouts`,
`workout-ready`,
`workout-active`, `workout-detail`, `calendar`, `calendar-day`,
`contacts`, `contact-detail`, `game2048`, `capability-lab-controls`,
`capability-lab-data`, `capability-lab-game`, and the
`capability-lab-host-*` actions. First-level preview
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

`--target` selects the model; it defaults to `ipod6g`. The build directory,
the packaged zip and the Mini App CPK payloads all follow the target:

```sh
./build-hw.sh                        # build-hw-ipod6g/CrazyPod-6G.zip
./build-hw.sh --target ipodvideo     # build-hw-ipodvideo/CrazyPod-5G.zip
```

Set Rockbox's build version explicitly for a tagged release. V1.0 was built
with:

```sh
VERSION=V1.0 ./build-hw.sh
cp build-hw-ipod6g/CrazyPod-6G.zip \
  build-hw-ipod6g/CrazyPod-V1.0-iPod6G.zip
cp build-hw-ipod6g/rockbox.ipod \
  build-hw-ipod6g/CrazyPod-V1.0-iPod6G-rockbox.ipod
```

## iPod Video bring-up target

`ipodvideo` builds the same product UI against Rockbox's existing iPod
Classic 5G/5.5G platform layer. Both models are 320x240 RGB565 at 160 DPI
with the same click-wheel keypad, so no UI asset or layout differs.

Targets that ship the product UI are listed in two places, which must stay
in sync:

- `HAVE_CRAZYPOD_UI` in `firmware/export/config/<model>.h`, which selects the
  product code over the Rockbox app layer.
- `CRAZYPOD_MODELS` in `tools/root.make`, which selects the LVGL, Mini App
  and product build rules.

What differs from the 6G, and how:

| Area | 6G | iPod Video |
| --- | --- | --- |
| SoC | S5L8702, ARMv5, 216 MHz, single core | PP5022, ARMv4T, dual core |
| RAM | 64 MiB | 64 MiB, or 32 MiB on 30 GB units (detected in `crt0-pp.S`) |
| Coprocessor | none | `cop_main()` in `crazypod_main.c` releases the COP |
| LCD present | TE/phase-synchronised `lcd_update_rect_*_sync()` | generic `lcd_update_rect()` |
| Accessory | serial iAP plus USB iAP (`HAVE_CRAZYPOD_IAP`) | not ported; the feature is off |
| Install | DFU plus `mks5lboot` | `bootloader-ipodvideo.ipod` via `ipodpatcher` |

Build the 5G bootloader with stock Rockbox tooling; `build-bootloader.sh`
covers the 6G NOR path only and does not apply here:

```sh
mkdir build-bootloader-ipodvideo && cd build-bootloader-ipodvideo
../tools/configure --target=ipodvideo --type=b
make
```

### Memory budget on 32 MiB units

30 GB 5G/5.5G models carry 32 MiB; 60 and 80 GB models carry 64 MiB.
`crt0-pp.S` probes which, and `core_allocator_init()` in `firmware/core_alloc.c`
lowers `audiobufend` by 32 MiB when it finds the smaller one. The shared
buflib arena that results holds both the audio buffer and every CrazyPod
runtime allocation:

| Build | Static | Shared arena |
| --- | --- | --- |
| iPod 6G | 19.6 MiB | ~40 MiB |
| iPod Video, 64 MiB | 18.5 MiB | ~41.5 MiB |
| iPod Video, 32 MiB | 18.5 MiB | ~9.5 MiB |

The address math leaves no slack on a 32 MiB unit: the codec (1 MiB) and
plugin (3 MiB) buffers alias into 28-32 MiB and land exactly on the top of
RAM, so `PLUGIN_BUFFER_SIZE` cannot grow past 3 MiB without overflowing.
Against a 4 MiB audio floor, a 32 MiB unit has roughly 5.5 MiB of working
room where the 6G has about 36 MiB. This is unmeasured on hardware and is
the most likely thing to fail there.

### Installing on Windows

The 5G writes its bootloader into the firmware partition with `ipodpatcher`,
not through the 6G's DFU/`mks5lboot` path. CI builds `ipodpatcher.exe` and
attaches it to the `crazypod-ipodvideo-firmware` artifact; to cross-build it
locally:

```sh
make -C utils/ipodpatcher \
  CROSS=i686-w64-mingw32- WINDRES=windres ipodpatcher.exe
```

Run it from a Command Prompt started with "Run as administrator". The
executable carries a `requireAdministrator` manifest, and launching it from an
ordinary prompt makes Windows relaunch it elevated in a **new console that
closes as it exits**, so the command appears to return instantly with no
output at all - not even the version banner `main()` prints before it parses
arguments. An already-elevated console keeps the output.

On Windows the device argument is the `PhysicalDrive` **number** alone, not a
path (`main.c` expands it):

```bat
ipodpatcher.exe --scan
ipodpatcher.exe 2 -r ipod-firmware-backup.bin
ipodpatcher.exe 2 -a bootloader-ipodvideo.ipod
```

Take the backup before writing anything; `-w ipod-firmware-backup.bin`
restores it and `-d` removes the bootloader. Then extract `CrazyPod-5G.zip`
to the root of the iPod's data volume, which produces `\.rockbox\`
including `\.rockbox\rockbox.ipod`, the path `BOOTDIR`/`BOOTFILE` in
`ipodvideo.h` point the bootloader at. Eject, then reset with MENU+SELECT.

Recovery, should it not boot: hold MENU+SELECT for about six seconds to
reset; to reach disk mode, reset and immediately hold SELECT+PLAY. Disk mode
works with broken firmware installed, which is what makes the bootloader
reversible.

Not yet done: no hardware validation of any kind, no frame-time measurement
on the slower CPU, no 32 MiB memory-budget check, and no accessory or
inline-remote support. Treat a 5G build as a compile artifact, not firmware
to install casually.

## Verification

Run the structural and host tests from the repository root:

```sh
sh tests/check-crazypod-ui-architecture.sh
sh tests/run-crazypod-ui-host-tests.sh
sh tests/run-miniapp-host-tests.sh
sh tests/run-gameboy-host-tests.sh
sh tests/run-epub-host-tests.sh
sh tests/run-crazypod-font-tests.sh
python3 tools/check-crazypod-l10n.py --strict-bare
git diff --check
```

The UI host test covers collation, A-Z wheel-jump state, route dispatch,
navigation commands, menu layout, book-reader input, and text helpers. The
architecture gate requires `crazypod_ui.c` to remain between 400 and 1500
lines and rejects feature-private includes outside their owner.

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
- `dist/miniapps/game2048-5.0.1.cpk`
- `dist/miniapps/capability-lab-5.0.1.cpk`
- `dist/miniapps/native-reference-1.0.0.cpk`
- `dist/miniapps/now-playing-neon-1.4.6.cpk`
- `dist/miniapps/now-playing-signal-1.0.7.cpk`

The zip deliberately contains only the firmware and the runtime resources
required by the independent product:

```text
.rockbox/rockbox.ipod
.rockbox/rockbox-info.txt
.rockbox/codecs/*.codec
.rockbox/codepages/936.cp
.rockbox/fonts/crazypod-aot/*.fnt
.rockbox/crazypod/default-home.bmp
.rockbox/crazypod/icons/<theme>/*.bmp
.rockbox/crazypod/miniapps/packages/game2048-5.0.1.cpk
.rockbox/crazypod/miniapps/packages/capability-lab-5.0.1.cpk
.rockbox/crazypod/miniapps/packages/native-reference-1.0.0.cpk
.rockbox/crazypod/miniapps/packages/now-playing-neon-1.4.6.cpk
.rockbox/crazypod/miniapps/packages/now-playing-signal-1.0.7.cpk
```

There are no Rockbox WPS files, themes, skin fonts, plugins, or recording
encoder codecs in the product package.

### Build-time checks

The hardware script stops before packaging if any of the main, IRQ, or FIQ
stack boundary symbols is missing or not 8-byte aligned. This is required by
the ARM EABI for values such as `double` and prevents variadic number
formatting from reading the wrong stack data on the iPod 6G.

The Mini App package builder requires Node.js 22 and npm. It AOT-compiles the
supported React Profile TypeScript/TSX subset to C. The Rockbox build compiles
that same C to `app.arm`; the simulator compiles it to `app.dylib`. CPK5 is a
deterministic ZIP STORE package containing the target-native binary, ABI
profile, converted resources and icon. It uses ZIP CRC and strict structure
validation but has no signature.

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

CrazyPod V1.0 has a manual installation and recovery procedure for Windows,
macOS, and Linux in [README.md](README.md#install-crazypod-v10). It does not
provide a one-click CrazyPod installer. See [PROJECT_STATUS.md](PROJECT_STATUS.md)
for the current validation record.

## Environment variables

| Variable | Purpose |
| --- | --- |
| `JOBS=N` | Parallel compile jobs |
| `CRAZYPOD_INCREMENTAL=1` | Reuse the hardware build directory |
| `CRAZYPOD_SKIP_DEP=1` | Reuse an existing hardware `make.dep` |
| `ROCKPOD_INCREMENTAL=1` | Reuse the simulator build directory |
| `ROCKPOD_SKIP_DEP=1` | Reuse an existing simulator `make.dep` |
| `CROSS_COMPILE=prefix-` | Override `arm-none-eabi-` |
| `CRAZYPOD_SIM_EXIT_AFTER_DUMP=1` | Exit after a simulator framebuffer dump |
