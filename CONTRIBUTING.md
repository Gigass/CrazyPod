# Contributing to CrazyPod

CrazyPod targets one device: the iPod Classic 6G (`ipod6g`). Keep changes
inside that product boundary unless a proposal explicitly expands it.

## Before changing code

1. Read [README.md](README.md) for the supported feature set.
2. Read [BUILD.md](BUILD.md) and build the simulator once.
3. For UI ownership changes, read
   [apps/crazypod/ARCHITECTURE.md](apps/crazypod/ARCHITECTURE.md).
4. For Mini App changes, read
   [miniapps/README.md](miniapps/README.md) and the
   [Chinese tutorial](miniapps/TUTORIAL.zh-CN.md).
5. For user-facing text or font changes, read
   [tools/CRAZYPOD_FONTS.md](tools/CRAZYPOD_FONTS.md).
6. Create a focused branch from the current default branch.
7. Check `git status` before editing. Do not overwrite unrelated local work.

## Source boundaries

- Put CrazyPod product code under `apps/crazypod/`.
- Keep LVGL integration under `lib/lvgl/`.
- Change inherited Rockbox code only when the iPod 6G platform requires it.
- Expose each UI feature through its `crazypod_<name>_feature.h` facade. Do not
  include feature-private headers from another feature, App, Shell,
  Presentation, Platform, or the composition root.
- Treat Native ABI 1, React Profile 1, CPK5 and each binary sub-format as
  versioned contracts.
  Keep fields append-only within a version, reject unknown versions, and add
  parser/renderer tests with every format change. Do not restore a JavaScript
  runtime or an ABI2/ABI3 compatibility loader.
- Wrap translatable CrazyPod UI text with `CP_TR()` and update all eight
  non-English locale files. Keep the nine native language names in the font
  character manifest.
- Do not reintroduce the Rockbox WPS, skin, theme, plugin, or recording UI into
  the CrazyPod product package.
- Do not commit build directories, device backups, caches, or `.DS_Store`
  files.

Follow the style of the file you edit. C source uses four-space indentation,
lowercase identifiers, C comments, and lines shorter than 80 columns.

## Verify changes

Run the checks that match the change:

```sh
./build-sim.sh --incremental
./build-hw.sh --incremental
sh tests/check-crazypod-ui-architecture.sh
sh tests/run-crazypod-ui-host-tests.sh
sh tests/run-miniapp-host-tests.sh
sh tests/run-epub-host-tests.sh
sh tests/run-crazypod-font-tests.sh
python3 tools/check-crazypod-l10n.py --strict-bare
git diff --check
```

Run only the relevant host suites while iterating, but run every applicable
suite before submission. Documentation-only changes still require link and
Markdown checks.

For localization changes, regenerate `crazypod_l10n_data.inc`, run the strict
localization audit, and verify every committed localized font size covers the
collected character manifest. Native AOT Mini App text is compiled into each
application binary and does not generate a firmware localization header. A
successful translation audit does not prove font coverage.

For interface changes, launch the simulator and verify click-wheel navigation,
Select, Menu, Left, Right, and Play. For hardware-specific changes, state
clearly whether the result was tested on an iPod or only compiled.

Bootloader, LCD, power, USB, Mini App Host, and filesystem changes require a
documented recovery path and explicit physical-test status. A successful ARM
build does not validate those behaviors.

## Submit a pull request

Keep each pull request focused. Include:

- the user-visible problem and its root cause;
- the behavior after the change;
- build and test commands with their results;
- simulator screenshots for visible interface changes;
- explicit hardware-test status.

Do not describe an untested hardware build as device-validated.
