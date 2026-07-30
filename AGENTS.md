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

## Commit & Pull Request Guidelines

History mixes terse English, Chinese summaries, and version tags; no enforced
commit format exists. Use a specific imperative subject, such as
`Fix album-flow cache invalidation`, and keep each commit focused. Pull requests
must explain the root cause and resulting behavior, list commands and results,
include simulator screenshots for visible changes, link relevant issues, and
state whether real hardware was tested.
