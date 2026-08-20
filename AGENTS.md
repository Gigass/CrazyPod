# Repository Guidelines

## Project Structure & Module Organization

CrazyPod is an iPod Classic 6G firmware product built on Rockbox. Product code
lives in `apps/crazypod/`; UI features are grouped under
`apps/crazypod/ui/features/`, with shared navigation, presentation, and shell
code in adjacent directories. Keep LVGL integration in `lib/lvgl/`, firmware
and target support in `firmware/`, Mini App sources and SDK files in
`miniapps/`, and host tests in `tests/`. Generated themes, icons, and wallpapers
belong in `assets/`. Do not commit `build-*`, device backups, caches, or
`.DS_Store`.

Read `apps/crazypod/ARCHITECTURE.md` before changing UI ownership. Expose each
feature through its `crazypod_<name>_feature.h` facade; do not reach into another
feature's private headers.

## Build, Test, and Development Commands

- `./build-sim.sh` creates a clean macOS simulator build.
- `./build-sim.sh --incremental` reuses `build-sim/`; run it with
  `cd build-sim && ./rockboxui`.
- `./build-hw.sh --incremental` compiles and packages iPod 6G firmware. This
  proves compilation, not physical-device safety.
- `sh tests/check-crazypod-ui-architecture.sh` enforces module boundaries.
- `sh tests/run-crazypod-ui-host-tests.sh`, `sh tests/run-miniapp-host-tests.sh`,
  and `sh tests/run-epub-host-tests.sh` run the C host suites. The EPUB suite
  downloads public samples and therefore needs network access.

## Coding Style & Naming Conventions

Follow the surrounding C style: four spaces, lowercase `snake_case`
identifiers, C comments, and lines under 80 columns. Use the `crazypod_` prefix
for product symbols and matching `.c`/`.h` filenames. Compile host code with
warnings treated as errors; finish with `git diff --check`.

## Testing Guidelines

Add focused `assert`-based tests named `*_host_test.c` and register them in the
matching `run-*-host-tests.sh` script. No numeric coverage target exists.
Exercise affected simulator routes and click-wheel controls for UI changes.
Report hardware testing separately for LCD, storage, USB, power, audio, or
native Mini App work.

Match verification effort to risk. For typo, wording, formatting, and narrow
constant or default-value changes that preserve types, valid ranges, control
flow, storage layouts, interfaces, and build configuration, inspect the diff
and run only cheap, directly relevant checks such as `git diff --check`. Do not
run full test suites, simulator or hardware builds, or device tests for these
confirmatory changes unless the user requests them or concrete evidence shows
broader risk. Never report an unrun check as passed.

## Real-Device Flashing

When the user asks to flash a connected iPod, use the shortest safe path. First
confirm that the only selected external device is the intended iPod 6G and that
the artifact targets `ipod6g`. Do not rerun the full test suite, perform routine
pre- or post-flash filesystem scans, compare every packaged file, or add other
extended checks unless the user requests them or the device reports a concrete
storage error.

Do not create a device backup during routine flashing. Merge the generated
`.rockbox` package into the existing device tree without delete mirroring.
Preserve `iPod_Control`, `Music`, `.crazypod`, user settings, application data,
media, and every unrelated device file. Never clean, format, repartition,
repair, or remove stale files as part of flashing unless the user explicitly
requests that separate operation.

For firmware updates, copy the generated `.rockbox` contents directly and
write `rockbox.ipod` last. Treat successful copy commands and the installed
firmware file's presence as the routine completion check. For bootloader
updates, require DFU mode and use `mks5lboot --bl-inst`; never pass `--single`
unless the user explicitly requests single-boot and accepts destruction of the
Apple NOR boot path.

This section controls agent-driven routine device updates. The longer backup,
filesystem-audit, and per-file comparison checklist in `BUILD.md` remains a
manual release-validation procedure, not the default flashing workflow.

## Commit & Pull Request Guidelines

History mixes terse English, Chinese summaries, and version tags; no enforced
commit format exists. Use a specific imperative subject, such as
`Fix album-flow cache invalidation`, and keep each commit focused. Pull requests
must explain the root cause and resulting behavior, list commands and results,
include simulator screenshots for visible changes, link relevant issues, and
state whether real hardware was tested.
