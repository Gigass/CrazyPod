# Contributing to CrazyPod

CrazyPod targets one device: the iPod Classic 6G (`ipod6g`). Keep changes
inside that product boundary unless a proposal explicitly expands it.

## Before changing code

1. Read [README.md](README.md) for the supported feature set.
2. Read [BUILD.md](BUILD.md) and build the simulator once.
3. For Mini App changes, read
   [miniapps/README.md](miniapps/README.md) and the
   [Chinese tutorial](miniapps/TUTORIAL.zh-CN.md).
4. Create a focused branch from the current default branch.
5. Check `git status` before editing. Do not overwrite unrelated local work.

## Source boundaries

- Put CrazyPod product code under `apps/crazypod/`.
- Keep LVGL integration under `lib/lvgl/`.
- Change inherited Rockbox code only when the iPod 6G platform requires it.
- Keep Mini App ABI fields append-only. Preserve the ABI 1 host-table prefix
  and gate optional tail functions with size and capability checks.
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
```

For interface changes, launch the simulator and verify click-wheel navigation,
Select, Menu, Left, Right, and Play. For hardware-specific changes, state
clearly whether the result was tested on an iPod or only compiled.

Bootloader, LCD, power, USB, native Mini App, and filesystem changes require a
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
