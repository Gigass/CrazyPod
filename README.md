<h1 align="center">
  <a href="https://ultrapod.gigassbox.com/crazypod/">▶ WATCH THE CRAZYPOD LIVE DEMO</a>
</h1>

<p align="center"><strong>Open the full interactive product showcase.</strong></p>

<p align="center"><a href="README.md">English</a> · <a href="README.zh-CN.md">简体中文</a></p>

> **Lineage:** CrazyPod derives from
> [Poorfocus/Rockbox-UI-UX-Overhaul](https://github.com/Poorfocus/Rockbox-UI-UX-Overhaul),
> which derives from [nuxcodes/rockpod](https://github.com/nuxcodes/rockpod),
> itself based on [Rockbox](https://www.rockbox.org/). All inherited copyright
> notices remain intact. See [NOTICE](NOTICE) and
> [LICENSE](LICENSE).

# CrazyPod

CrazyPod V1.0 is experimental standalone firmware for the iPod Classic 6G
hardware family. It replaces the Rockbox interface with a 320×240 LVGL
application carousel while retaining Rockbox's codecs, playback engine,
storage, power, USB, and device drivers.

![CrazyPod home screen](screenshots/crazypod-home.png)

> [!WARNING]
> CrazyPod is experimental firmware. V1.0 compiles and packages cleanly, and
> earlier development builds were installed and file-integrity checked on an
> iPod Classic 6G. V1.0 has not completed a full physical-device regression
> suite. Keep a backup and a tested Apple Disk Mode/DFU recovery path.
> The existing TE command is retained on 8-bit type 0/1 displays, and phase
> synchronization is used only after runtime GPIO validation. Type 2/3 displays
> retain the original Rockbox LCD initialization and register values.

## Project lineage

CrazyPod was created from
[Poorfocus/Rockbox-UI-UX-Overhaul](https://github.com/Poorfocus/Rockbox-UI-UX-Overhaul),
which was developed from
[nuxcodes/rockpod](https://github.com/nuxcodes/rockpod), itself based on
[Rockbox](https://www.rockbox.org/). CrazyPod replaces the product UI and
excludes the former Apple2026 theme, but retains inherited source code and Git
history. Upstream copyright and license notices remain in place.

## Scope

| | Current support |
| --- | --- |
| Device | iPod Classic 6G target family (`ipod6g`: 6th, 6.5th, and 7th generation) |
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

- Recursively scans `/Music` and, when Settings → Playback → Original iPod
  Music is enabled, `/iPod_Control/Music` for formats supported by the bundled
  Rockbox codecs. Fresh installs enable this setting; upgrades from V1.0 keep
  the previous disabled behavior until the user enables it.
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
- **Game Boy:** A dedicated **Game Boy** desktop app opens the Rockboy game library
  for `.gb` and `.gbc` ROMs in `/MiniApps/Games`. It includes audio, wheel controls,
  battery saves and RTC.
  No games are bundled. Compatibility and speed need real-device testing;
  see the [Game Boy guide](docs/CRAZYPOD_GAMEBOY.zh-CN.md).
- **Now Playing themes:** installable pure-TSX source packages can replace the
  playback page after explicit user selection. Firmware keeps the default
  page, audio engine, cover decoder, and sound-wave renderer. ABI 1.7 exposes
  bounded queue, favorite, playback-mode, lyrics, seek, and marquee services;
  ABI 1.8 adds modal-scoped Menu back and coalesced wheel steps. Outer-page
  Menu and held Menu remain firmware escape paths. ABI 1.9 adds automatic
  committed-cover binding, atomic theme replacement, and next-two prefetch.
  The current Native ABI is 1.20, including theme status bars, role-based and
  package-private fonts, configurable cover decode size, host sound-wave
  styles, paged lyric context, and four-line adaptive lyrics.
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
- The firmware catalog contains 839 translated UI keys.
- Generated 8, 10, 12, 14, and 16px font subsets use PingFang SC for the
  Simplified Chinese system face, with Noto CJK coverage fallback for Hangul
  and other missing characters. The non-LVGL LCD text path also decodes UTF-8.

### Mini Apps

The firmware bundles three CPK5 Native AOT references and two theme demos:

- **2048:** React-style TSX UI, generated native C game logic, Click Wheel
  input and CRC-checked persistent board state.
- **Capability Lab:** controls, Flex layout, resources, conditional subtrees,
  events and repeated mount/unmount behavior.
- **Native Reference:** minimum native lifecycle and retained UI path.
- **Neon Playback:** pure TSX Now Playing theme using firmware playback,
  dynamic artwork, a theme-drawn progress bar, and bounded PCM-derived peak
  telemetry for its VU segments.
- **Signal One:** pure TSX Now Playing theme with the same firmware-owned
  playback and escape-path guarantees in a warm industrial visual system.

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
| V1.0 release archive | `build-hw-ipod6g/CrazyPod-V1.0-iPod6G.zip` |
| Optional bootloader | `build-bootloader-ipod6g/bootloader-ipod6g.ipod` |
| 2048 package | `dist/miniapps/game2048-5.0.1.cpk` |

The V1.0 release uses a manual install. Follow the complete procedure below;
do not copy only `rockbox.ipod`, because the archive also contains required
codecs, fonts, icons, and Native Mini App packages.

See [PROJECT_STATUS.md](PROJECT_STATUS.md) for the current validation level,
release evidence, and remaining physical-device validation gaps.

## Install CrazyPod V1.0

This manual procedure covers two distinct starting points on Windows, macOS,
and Linux. Choose one route before touching the bootloader:

| Current iPod state | Required route |
| --- | --- |
| Apple firmware only; Rockbox has never been installed | Follow **2A** to install the official dual bootloader, then copy the complete package in **3**. |
| Rockbox already boots | Skip **2A**. Read **2B**, then copy the complete package in **3**. |
| An older CrazyPod build already boots | Use the same package-only route as an existing Rockbox user. |

The complete ZIP is required in every route. Copying only `rockbox.ipod`
creates a mismatched installation without the matching codecs, fonts, icons,
and Native Mini App packages.

### 1. Confirm the device and prepare recovery

CrazyPod supports only the Rockbox `ipod6g` target: the iPod Classic 6th,
6.5th, and 7th generation family. Check the model in Apple firmware under
Settings → About. Known family model prefixes include `MB029`, `MB145`,
`MB147`, `MB150`, `MB562`, `MB565`, `MC293`, and `MC297` (including regional
`PB`/`PC` variants). Do not install this package on an iPod Video/5G, Nano,
Mini, Touch, or any other target.

Before writing anything:

1. Back up the iPod's media and the existing `.rockbox` and `.crazypod`
   directories.
2. Confirm that the data volume is FAT32. HFS/HFS+ “MacPods” cannot boot
   Rockbox. Converting one requires a Windows restore and erases the device;
   complete that restore and restore the media backup before continuing.
3. Download `CrazyPod-V1.0-iPod6G.zip` and `SHA256SUMS.txt` from the
   [V1.0 release](https://github.com/Gigass/CrazyPod/releases/tag/V1.0).
4. Keep a USB cable and the Apple Disk Mode/DFU button sequences available.

The firmware archive does not alter the Apple firmware partition, repartition
the disk, or install a bootloader. The official bootloader provides dual boot
when installed without `--single`. Never use `mks5lboot --single`: that option
destroys the normal Apple NOR boot path.

Verify the downloaded archive before installation. On Windows, compare
`Get-FileHash "$HOME\Downloads\CrazyPod-V1.0-iPod6G.zip" -Algorithm SHA256`
with the archive line in `SHA256SUMS.txt`. On macOS run:

```sh
cd "$HOME/Downloads"
grep 'CrazyPod-V1.0-iPod6G.zip' SHA256SUMS.txt | shasum -a 256 -c -
```

On Linux run:

```sh
cd "$HOME/Downloads"
grep 'CrazyPod-V1.0-iPod6G.zip' SHA256SUMS.txt | sha256sum -c -
```

The macOS/Linux command must report `OK`.

### 2A. Apple firmware-only users: install the dual bootloader

Follow this section only if Rockbox has never booted on this iPod. If a Rockbox
bootloader already appears at startup, skip directly to 2B. CrazyPod V1.0 uses
the official Rockbox iPod 6G dual bootloader; the separate experimental
CrazyPod bootloader is not required or included.

Download the official
[`bootloader-ipod6g.ipod`](https://download.rockbox.org/bootloader/ipod/bootloader-ipod6g.ipod).

#### Windows bootloader install

1. Install the current [Rockbox Utility](https://www.rockbox.org/download/)
   and run it as an administrator.
2. Select iPod Classic 6G and the correct iPod drive letter. If necessary,
   enable **Show disabled targets** in the utility's configuration.
3. Choose a bootloader-only installation and follow the utility's DFU prompts.
   Do not install the standard Rockbox firmware over CrazyPod after completing
   the V1.0 copy in section 3.
4. If Windows has no usable Apple DFU driver, install/update Apple Devices or
   iTunes from Apple, reconnect the iPod, and retry Rockbox Utility.

Manual `mks5lboot` installation on Windows is not supported by the inherited
Rockbox manual; use Rockbox Utility.

#### macOS bootloader install

Install the command-line tools, build `mks5lboot` from this repository, and
scan for the iPod:

```sh
xcode-select --install
brew install libusb
git clone https://github.com/Gigass/CrazyPod.git
cd CrazyPod
make -C utils/mks5lboot
./utils/mks5lboot/mks5lboot --dfuscan -l
```

Quit Music/iTunes and close any Finder iPod window before entering DFU. Once
the scan reports the device, press Control-C and install the bootloader:

```sh
./utils/mks5lboot/mks5lboot --bl-inst \
  "$HOME/Downloads/bootloader-ipod6g.ipod"
```

#### Linux bootloader install

Install a compiler and the libusb development package, then build and run
`mks5lboot`. Debian/Ubuntu commands are:

```sh
sudo apt update
sudo apt install build-essential libusb-1.0-0-dev
git clone https://github.com/Gigass/CrazyPod.git
cd CrazyPod
make -C utils/mks5lboot
sudo ./utils/mks5lboot/mks5lboot --dfuscan -l
```

Once the scan reports the device, press Control-C and install the bootloader:

```sh
sudo ./utils/mks5lboot/mks5lboot --bl-inst \
  "$HOME/Downloads/bootloader-ipod6g.ipod"
```

On Fedora use `gcc make libusb1-devel`; on Arch use `base-devel libusb`.

#### Enter iPod Classic DFU mode

1. Connect the iPod by USB.
2. Lock the HOLD switch, wait one second, then unlock it.
3. Hold **Menu + Center** together for 12 seconds.
4. Release both buttons. The screen remains completely black in DFU mode.
5. Confirm that Rockbox Utility or `mks5lboot --dfuscan -l` detects USB product
   ID `1223` before installing the bootloader.

If the Apple logo appears, the timing missed DFU; repeat the sequence. Do not
run `--bl-inst` until the scan identifies the DFU device.

### 2B. Existing Rockbox or CrazyPod users: keep the bootloader

If the iPod already shows a Rockbox bootloader at startup, do not enter DFU and
do not reinstall, update, or uninstall the bootloader. The existing bootloader
already loads `/.rockbox/rockbox.ipod` and retains the Apple dual-boot path.

Before replacing the firmware, keep a copy of the existing `.rockbox`
directory if you want a simple rollback. Continue with section 3 and merge the
complete CrazyPod package. Do not delete `.rockbox`, do not use a mirror mode
that deletes destination files, and do not remove `.crazypod` or user media.
To return to standard Rockbox later, restore the previous complete `.rockbox`
backup; the bootloader does not need to change.

### 3. Copy the V1.0 firmware package for either route

Boot Apple Disk Mode for the most conservative file-transfer path: reboot with
**Menu + Center**, then immediately hold **Center + Play** until Disk Mode
appears. Mount the iPod's FAT32 data volume and use the instructions for your
host OS. The commands merge the package and do not delete `iPod_Control`,
music, `.crazypod`, settings, application data, or unrelated files. Replace
the example mount path with the actual iPod.

#### Windows firmware copy (PowerShell)

```powershell
$archive = "$HOME\Downloads\CrazyPod-V1.0-iPod6G.zip"
$mount = "E:"
$temp = New-Item -ItemType Directory -Path `
  (Join-Path $env:TEMP ("crazypod-" + [guid]::NewGuid()))
Expand-Archive -Path $archive -DestinationPath $temp.FullName
New-Item -ItemType Directory -Force -Path "$mount\.rockbox" | Out-Null
Copy-Item "$($temp.FullName)\.rockbox\*" `
  "$mount\.rockbox\" -Recurse -Force
"Music","Podcasts","Books","Pictures","Videos","Contacts",`
  "Calendars","MiniApps" | ForEach-Object {
    New-Item -ItemType Directory -Force -Path "$mount\$_" | Out-Null
  }
# Write the firmware again last, after every resource has reached the iPod.
Copy-Item "$($temp.FullName)\.rockbox\rockbox.ipod" `
  "$mount\.rockbox\rockbox.ipod" -Force
Get-FileHash "$mount\.rockbox\rockbox.ipod" -Algorithm SHA256
```

Compare the printed firmware hash with `SHA256SUMS.txt`, then use **Safely
Remove Hardware** before unplugging the cable.

#### macOS firmware copy (Terminal)

```sh
archive="$HOME/Downloads/CrazyPod-V1.0-iPod6G.zip"
mount="/Volumes/IPOD"
tmp="$(mktemp -d)"
ditto -x -k "$archive" "$tmp"
mkdir -p "$mount/.rockbox"
cp -R "$tmp/.rockbox/." "$mount/.rockbox/"
for dir in Music Podcasts Books Pictures Videos Contacts Calendars MiniApps; do
    mkdir -p "$mount/$dir"
done
# Write the firmware again last.
cp "$tmp/.rockbox/rockbox.ipod" "$mount/.rockbox/rockbox.ipod"
sync
shasum -a 256 "$mount/.rockbox/rockbox.ipod"
diskutil eject "$mount"
```

#### Linux firmware copy (Terminal)

```sh
archive="$HOME/Downloads/CrazyPod-V1.0-iPod6G.zip"
mount="/media/$USER/IPOD"
tmp="$(mktemp -d)"
unzip -q "$archive" -d "$tmp"
mkdir -p "$mount/.rockbox"
cp -a "$tmp/.rockbox/." "$mount/.rockbox/"
for dir in Music Podcasts Books Pictures Videos Contacts Calendars MiniApps; do
    mkdir -p "$mount/$dir"
done
# Write the firmware again last.
cp "$tmp/.rockbox/rockbox.ipod" "$mount/.rockbox/rockbox.ipod"
sync
sha256sum "$mount/.rockbox/rockbox.ipod"
udisksctl unmount -b "$(findmnt -no SOURCE --target "$mount")"
```

Compare the installed firmware hash with `SHA256SUMS.txt` before unmounting.

### 4. First boot, dual-boot controls, and recovery

Disconnect USB and reboot with **Menu + Center**. With no startup key held, the
bootloader loads `/.rockbox/rockbox.ipod`; V1.0 then creates or migrates state
under `/.crazypod` without deleting media.

| Startup action | Result |
| --- | --- |
| No key held | Boot CrazyPod. |
| Hold **Menu** during the first second of power-up | Boot the original Apple firmware. |
| Engage HOLD immediately after power-up | Boot the original Apple firmware; release HOLD after it starts. |
| Hold **Center + Play** during reboot | Enter Apple Disk Mode. |
| Hold **Menu + Center** for about 12 seconds | Enter DFU recovery mode. |

- If the bootloader reports a missing firmware, enter Apple Disk Mode with
  **Center + Play** during reboot and repeat the package copy.
- With flash/SD storage adapters, use Apple Disk Mode for transfers if Rockbox
  USB is slow or unstable. Stop immediately if files disappear, sizes change,
  or the filesystem becomes read-only.
- To remove the bootloader, use Rockbox Utility or
  `mks5lboot --bl-uninst ipod6g`; removing `.rockbox` alone does not uninstall
  the bootloader.

## Device content

The install archive includes these directories at the root of the iPod's
storage. Create them manually when installing without the archive:

| Content | Location |
| --- | --- |
| Music and playlists | `/Music` |
| iTunes-managed music | `/iPod_Control/Music` when enabled in Playback settings |
| Podcasts | `/Podcasts` |
| Books | `/Books` |
| Photos and wallpapers | `/Pictures` |
| Videos | `/Videos/*.mpg` or `/Videos/*.mpeg` |
| Contacts | `/Contacts/*.vcf` |
| Calendars | `/Calendars/*.ics` or `/Calendar/*.ics` |
| Mini App and theme packages | `/MiniApps/*.cpk` |
| Game Boy ROMs | `/MiniApps/Games`, `/MiniApps/Games/GB`, `/MiniApps/Games/GBC` |
| Game Boy battery saves | `/.crazypod/gameboy` |
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
- The fixed localized font artifacts use PingFang SC as the primary Simplified
  Chinese face and Noto CJK for uncovered characters. Runtime Japanese,
  Traditional Chinese, and Korean system text keeps its regional Noto face.
- Compatible 3.5 mm inline remotes support volume plus single-, double-, and
  triple-press playback controls. Remote hardware compatibility may vary.
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
