# CrazyPod build guide

CrazyPod supports one product target: iPod Classic 6G (`ipod6g`).

## Prerequisites

Simulator:

- GNU make
- GCC
- Perl
- SDL2 development files (`sdl2-config` and `pkg-config`)

Hardware:

- GNU make
- Perl
- `zip`
- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`

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

## Hardware

```sh
./build-hw.sh
./build-hw.sh --incremental
```

The script rejects target arguments because no target other than `ipod6g` is
supported.

Artifacts:

- `build-hw-ipod6g/rockbox.ipod`
- `build-hw-ipod6g/CrazyPod-6G.zip`

The zip deliberately contains only the firmware and the runtime resources
required by the independent product:

```text
.rockbox/rockbox.ipod
.rockbox/rockbox-info.txt
.rockbox/codecs/*.codec
.rockbox/crazypod/default-home.bmp
.rockbox/crazypod/icons/<theme>/*.bmp
```

There are no Rockbox WPS files, themes, skin fonts, plugins, or recording
encoder codecs in the product package.

Portable DIY appearances use fixed USB-visible paths:

- Import: copy one file to `/.crazypod/import.upodtheme`, then choose
  Customize → Presets → Import.
- Export: choose Export on a saved appearance; CrazyPod writes it under
  `/.crazypod/export/`.

## Installation warning

No physical iPod validation has been completed for this revision. Back up the
existing `.rockbox` directory and keep a known-good restore path before testing
the generated firmware.

## Environment variables

| Variable | Purpose |
| --- | --- |
| `JOBS=N` | Parallel compile jobs |
| `CRAZYPOD_INCREMENTAL=1` | Reuse the hardware build directory |
| `CRAZYPOD_SKIP_DEP=1` | Reuse an existing hardware `make.dep` |
| `ROCKPOD_INCREMENTAL=1` | Reuse the simulator build directory |
| `ROCKPOD_SKIP_DEP=1` | Reuse an existing simulator `make.dep` |
| `CROSS_COMPILE=prefix-` | Override `arm-none-eabi-` |
