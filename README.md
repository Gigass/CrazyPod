# CrazyPod

CrazyPod is an independent firmware product for iPod Classic 6G. It uses
Rockbox hardware support as its low-level platform, but it does not use the
Rockbox menu, browser, WPS, skin engine, theme system, or plugin UI. Rockbox's
codec, buffering, playlist, PCM, storage, power, and USB layers remain as
low-level services.

## Current state

- LVGL 9.5.0 is integrated for a 320×240 RGB565 display.
- The firmware boots directly into a 14-application animated icon carousel
  derived from `MaxPodApp`, with the original default wallpaper, icon
  reflections, position indicators, and now-playing capsule.
- The click wheel moves focus; Select opens an application; Menu goes back.
- Music is functional for local files: recursive metadata scanning, artists,
  albums, songs, M3U/M3U8 playlists, local search, queue creation, shuffle,
  repeat, resume persistence, album artwork, codec playback, and PCM output.
- Network music, online lyrics, and network import are intentionally excluded.
- Customize implements the firmware-applicable part of MaxPod DIY: 16 icon
  themes, icon scale, glow, highlight colors, independent top/bottom screen
  corner radii, Home/Menu wallpaper selection from `/Pictures`, and persistent
  appearance presets with validated `.upodtheme` import/export. Each setting
  opens a chooser before applying a value.
  Chassis and click-wheel customization are intentionally excluded because
  firmware cannot change physical hardware.
- Shuffle is a functional local-music shortcut. Every application other than
  Music and Customize remains a product placeholder.
- Battery and clock status are live.
- USB exposes mass storage only. USB Audio, HID, and iPod accessory protocols
  are disabled.
- Only the Rockbox `ipod6g` target is supported.

Physical-device behavior has not yet been validated in this revision. The
native ARM firmware and simulator both build successfully, but installing on
hardware remains a separate test.

## Build

```sh
./build-sim.sh
./build-hw.sh
```

Fast incremental builds:

```sh
./build-sim.sh --incremental
./build-hw.sh --incremental
```

Outputs:

- Simulator: `build-sim/rockboxui`
- Firmware: `build-hw-ipod6g/rockbox.ipod`
- Install archive: `build-hw-ipod6g/CrazyPod-6G.zip`

See [BUILD.md](BUILD.md) for prerequisites and installation notes.

## Product boundary

The active product sources are:

- `apps/crazypod/crazypod_main.c` — platform startup
- `apps/crazypod/crazypod_lcd.c` — product framebuffer and panic display
- `apps/crazypod/crazypod_ui.c` — LVGL desktop and placeholders
- `apps/crazypod/crazypod_music.c` — local library and playback queue bridge
- `apps/crazypod/crazypod_playlist.c` — Rockbox playlist integration
- `apps/crazypod/crazypod_state.c` — queue, playback, and settings persistence
- `apps/crazypod/crazypod_appearance.c` — active DIY appearance
- `apps/crazypod/crazypod_presets.c` — DIY preset persistence
- `apps/crazypod/crazypod_icons.c` — runtime icon-theme loading
- `apps/crazypod/crazypod_wallpaper.c` — MaxPod desktop wallpaper loading
- `lib/lvgl/` — vendored LVGL 9.5.0

Inherited Rockbox sources remain where they provide 6G drivers, storage,
filesystem, power, kernel, USB mass storage, or simulator infrastructure.
Legacy Rockbox UI sources are not part of the CrazyPod 6G build.

## License

CrazyPod preserves the upstream Rockbox/Rockpod copyright notices and is
distributed under GPLv2. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
