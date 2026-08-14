# CrazyPod

CrazyPod is experimental standalone firmware for the iPod Classic 6th
generation. It replaces the Rockbox interface with a 320×240 LVGL application
carousel while retaining Rockbox's codecs, playback engine, storage, power,
USB, and device drivers.

![CrazyPod home screen](screenshots/crazypod-home.png)

> [!WARNING]
> CrazyPod is experimental firmware. Development builds have been installed
> and file-integrity checked on an iPod Classic 6G, but the project has no
> release-grade installer or complete hardware regression suite. Keep a full
> backup and a tested recovery path.

## Scope

| | Current support |
| --- | --- |
| Device | iPod Classic 6G (`ipod6g`) only |
| Display | 320×240 RGB565 |
| Interface | LVGL 9.5.0; click-wheel navigation |
| Languages | English, Simplified Chinese, Traditional Chinese, Japanese, Korean, German, French, Spanish, and Brazilian Portuguese |
| Media | Local files only |
| Connectivity | Selectable USB charge-only or mass-storage mode |
| Network services | None |

CrazyPod is a firmware product, not a Rockbox theme. Its build excludes the
Rockbox menu, file browser, WPS, skin engine, theme system, plugin UI, recording
pipeline, USB Audio, HID, and iPod accessory protocol.

## Features

### Music and media

- Recursively scans `/Music` and `/iPod_Control/Music` for formats supported by
  the bundled Rockbox codecs.
- Builds artist, album, song, M3U/M3U8 playlist, and persistent `My Favorites`
  views from local metadata.
- Sorts Latin and CJK metadata through one collation key. Fast wheel movement
  can jump between pinyin or Latin A-Z sections without changing playlist,
  album-track, or queue order.
- Provides search, a dynamic playback queue, shuffle, repeat, resume,
  synchronized local LRC lyrics, and native Cover Flow.
- Stores one CV8 128×128 RGB565 image per album. First-time artwork preparation
  is path-ordered and resumable; later scans preserve unchanged cache entries.
- Keeps the previous artwork visible until the next cover and blurred
  background are ready, then publishes them together.
- Scrolls overflowing Home, Now Playing, and selected-list text. Hidden and
  unselected labels stop animating.
- Plays podcasts stored under `/Podcasts`.
- Provides a unified **Media** app with Photos, Videos, and Favorites.
- Browses JPG, JPEG, and BMP images under `/Pictures`, with favorites,
  full-screen viewing, zoom, and pan.
- Plays pre-transcoded MPEG-1/2 `.mpg` and `.mpeg` files from `/Videos`, with
  poster art, play/pause, 10-second seek, volume control, and resume.

### Device applications

- **Notes:** drafts, pinning, search, duplication, trash, and restore.
- **Books:** EPUB, TXT, and Markdown reading with progress, bookmarks,
  favorites, chapters, font sizes, and paper themes. TXT and Markdown content
  automatically distinguishes strict UTF-8 from GBK/GB2312 (CP936).
- **Organizer:** local calendar events, read-only ICS import, and VCF contacts.
- **Mini Apps:** React-style TypeScript/TSX is AOT-compiled to C and then to
  native `app.arm`; Native ABI 1 drives host-owned LVGL. The device runs no
  JavaScript engine. Reference apps are 2048 and Capability Lab.
- **Now Playing themes:** installable pure-TSX source packages can replace the
  playback page after explicit user selection. Firmware keeps the default
  page, audio engine, cover decoder, and sound-wave renderer. ABI 1.7 exposes
  bounded queue, favorite, playback-mode, lyrics, seek, and marquee services;
  ABI 1.8 adds modal-scoped Menu back and coalesced wheel steps. Outer-page
  Menu and held Menu remain firmware escape paths. ABI 1.9 adds automatic
  committed-cover binding, atomic theme replacement, and next-two prefetch.
- **Workouts:** 20 timed activities with pause, resume, history, and summaries.
  CrazyPod records elapsed time only; it does not invent distance, steps, or
  calorie data.
- **System:** live battery and clock status, selectable USB Charge/Data modes,
  a full-screen input block while Data mode is active, configurable idle
  power-off, and a configurable 16-application main menu.
- **Lock screen:** immediate manual lock, wake-key isolation, a large clock,
  configurable wallpaper and corner radii, and a 0.5-second Center-button hold
  to unlock with progress feedback.
- **Power menu:** holding Play for about three seconds opens a Shut Down /
  Restart confirmation surface without entering Rockbox's committed shutdown
  path first.
- **Settings:** sound, EQ Studio, display and backlight, Reduce Motion,
  playback, idle power-off, sleep timer, USB charging, click feedback,
  language, and main-menu order.

Lock turns off the backlight, suspends product background media work, and stops
periodic LVGL/UI servicing until an input, system event, alarm, or playback
checkpoint is due; it is still not whole-device suspend. Settings → Power →
Idle Power Off defaults to ten minutes, is disabled while audio is actively
playing or external power is connected, and shuts the device down after
paused/stopped inactivity. See the [power-management contract](docs/CRAZYPOD_POWER_MANAGEMENT.zh-CN.md).

### Localization

- Settings → Language applies one of nine languages immediately and persists
  it across restarts.
- The firmware catalog contains 831 translated UI keys.
- Generated 8, 10, 12, 14, and 16px font subsets cover the current CJK,
  Hangul, and accented Latin catalog. The non-LVGL LCD text path also decodes
  UTF-8.

### Mini Apps

The firmware bundles three CPK5 Native AOT references and one theme demo:

- **2048:** React-style TSX UI, generated native C game logic, Click Wheel
  input and CRC-checked persistent board state.
- **Capability Lab:** controls, Flex layout, resources, conditional subtrees,
  events and repeated mount/unmount behavior.
- **Native Reference:** minimum native lifecycle and retained UI path.
- **Neon Playback:** pure TSX Now Playing theme using firmware playback,
  dynamic artwork, a theme-drawn progress bar, and bounded PCM-derived peak
  telemetry for its VU segments.

Fast wheel input never skips visible actionable controls. The host keeps a
bounded input queue and consumes at most one discrete focus movement per
display frame; reversing direction discards stale queued movement. Acceleration
is available to applications as a 1–4 step magnitude. Additional CPK5 packages
can be dropped directly into `/MiniApps`. Cold start and USB disconnect scan
and publish valid packages before Mini Apps or Themes becomes interactive.
A persistent index skips unchanged CPKs by source, filename, size, timestamp,
package format, and ABI; only new or changed packages are opened and verified. See
[miniapps/README.md](miniapps/README.md) for the package and runtime contract,
or follow the [Chinese Mini App tutorial](miniapps/TUTORIAL.zh-CN.md).

### Appearance

- 16 packaged icon themes with scale, glow, and highlight controls.
- Independent top and bottom screen corner radii.
- Home and menu wallpapers loaded from `/Pictures`.
- Versioned `.upodtheme` appearance presets with validated import and export.
- Music, Media, Notes, and Books use object-specific skeuomorphic preview
  transitions on their first-level menus. Rapid wheel input cancels the
  current transition and presents only the latest selection.
- Preview objects render a complete procedural surface immediately. Album art
  and photo thumbnails are optional inner skins: cached media appears with a
  short fade after the physical entrance, without a spinner or replaying the
  scene. The All Music wall limits media reads to its first row.
- Settings → Display → Reduce Motion replaces transforms and stagger with a
  short crossfade. The preference is stored in CrazyPod's versioned state.

## Build and run

See [BUILD.md](BUILD.md) for toolchain prerequisites and detailed build
options. The scripts have been verified on macOS.

Clone the canonical repository and build the simulator:

```sh
git clone https://github.com/Gigass/CrazyPod.git
cd CrazyPod
./build-sim.sh
cd build-sim
./rockboxui
```

Build the iPod 6G firmware:

```sh
./build-hw.sh
```

Both scripts perform clean builds by default. Pass `--incremental` to reuse an
existing build directory.

| Artifact | Path |
| --- | --- |
| Simulator | `build-sim/rockboxui` |
| Firmware | `build-hw-ipod6g/rockbox.ipod` |
| Install archive | `build-hw-ipod6g/CrazyPod-6G.zip` |
| Optional bootloader | `build-bootloader-ipod6g/bootloader-ipod6g.ipod` |
| 2048 package | `dist/miniapps/game2048-5.0.1.cpk` |

The project does not yet claim a supported hardware installation procedure.
The generated archive is for controlled device testing by users who already
have a compatible bootloader and recovery method.

See [PROJECT_STATUS.md](PROJECT_STATUS.md) for the current validation level,
conversation audit, and known release blockers.

## Device content

Create these directories at the root of the iPod's storage:

| Content | Location |
| --- | --- |
| Music and playlists | `/Music` |
| iTunes-managed music | `/iPod_Control/Music` |
| Podcasts | `/Podcasts` |
| Books | `/Books` |
| Photos and wallpapers | `/Pictures` |
| Videos | `/Videos/*.mpg` or `/Videos/*.mpeg` |
| Contacts | `/Contacts/*.vcf` |
| Calendars | `/Calendars/*.ics` or `/Calendar/*.ics` |
| Mini App and theme packages | `/MiniApps/*.cpk` |
| Theme import | `/.crazypod/import.upodtheme` |

CrazyPod stores settings, notes, reading progress, workout history, caches, and
exported appearances under `/.crazypod`.

Video files must already be MPEG-1/2 program streams. Convert a desktop video
and generate its same-basename 128×96 BMP poster with:

```sh
./tools/convert-crazypod-video.sh input.mp4
```

The default output is `Videos/input.mpg` plus `Videos/input.bmp`; copy both
files into `/Videos` on the iPod. Pass a directory as the second argument to
choose another output location.

## Controls

| Action | iPod | Simulator |
| --- | --- | --- |
| Move focus | Click wheel | Up / Down |
| Open | Select | Return |
| Back | Menu | Backspace / Escape |
| Previous / Next | Left / Right | Left / Right |
| Play | Play | Space |

On the iPod, hold Play for about three seconds to open the power menu. Use the
wheel or Left/Right to choose Shut Down or Restart, Select to confirm, and Menu
to cancel.

During video playback, Play toggles pause, Left/Right seek by 10 seconds, the
wheel changes volume, and Menu exits while saving resume progress.

On the main Now Playing screen, the wheel changes volume and shows a temporary
vertical level bar at the left edge. In the action surface, choose Progress to
seek in five-second steps; choosing a queue item starts that track and closes
the queue. The Favorite action adds or removes the current track from
`My Favorites`.

## Known limits

- Only the iPod Classic 6G target is supported.
- The current localized font artifacts use one shared Noto Sans CJK SC subset.
  Character coverage is complete for the catalog, but Japanese and Traditional
  Chinese do not yet use region-specific Han glyph shapes.
- 3.5mm headset remote buttons are not supported. The current target does not
  initialize the Mikey remote controller or route its events into CrazyPod.
- Music, lyrics, books, photos, contacts, and calendars are local-only.
- Video playback does not accept MP4/H.264/AAC directly. Convert those files
  to MPEG-1/2 first; subtitles, playlists, deletion, and on-device conversion
  are not included in the first video release.
- Books detects strict UTF-8 and CP936-compatible GBK/GB2312 text. It does not
  decode GB18030 four-byte extensions, and uncommon characters outside the
  bundled font subset may appear blank.
- Camera and voice recording are absent because the target has neither
  required input.
- Mini Apps accepts only target-matched CPK5 native packages. Packages are
  CRC/structure checked but are not signed. React Profile v1 is a constrained
  AOT source subset, not arbitrary TypeScript-to-C. See the
  [Native AOT architecture](docs/CRAZYPOD_MINIAPP_NATIVE_AOT_ARCHITECTURE.zh-CN.md).
- The optional custom bootloader is built and installed separately from the
  firmware archive. It has not completed physical boot regression. Its current
  Apple-shaped mark is also unsuitable for an unaffiliated public release
  without a trademark review or replacement artwork.
- Rockbox plugins, WPS, skins, themes, USB Audio, HID, and iPod accessory
  protocol are excluded from the product package.

## Project structure

- `apps/crazypod/` contains the product UI, applications, playback bridge, and
  persistent state.
- `localization/crazypod/` contains the source catalog and eight non-English
  locale files.
- [`apps/crazypod/ARCHITECTURE.md`](apps/crazypod/ARCHITECTURE.md) defines the
  feature, Shell, Navigation, Presentation, Platform, and composition-root
  boundaries enforced by the architecture test.
- `miniapps/` contains the Native ABI header, React Profile examples, 2048,
  and Capability Lab.
- `lib/lvgl/` contains the vendored LVGL 9.5.0 source.
- `assets/crazypod/` and `assets/crazypod-icons/` contain packaged runtime
  graphics.
- `tools/convert-crazypod-video.sh` creates device-ready MPEG video and poster
  pairs with FFmpeg.
- `build-bootloader.sh` builds the optional silent boot surface but never
  writes it to a device.
- `firmware/`, `lib/rbcodec/`, and the remaining inherited Rockbox tree provide
  the iPod 6G platform.

See [release-notes.md](release-notes.md) for the current feature delta and
[PROJECT_STATUS.md](PROJECT_STATUS.md) for validation details. Read
[CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes.

## License

CrazyPod preserves the upstream Rockbox and Rockpod copyright notices and is
distributed under GPLv2. See [LICENSE](LICENSE) and [NOTICE](NOTICE). CrazyPod
is not affiliated with or endorsed by Apple Inc.
