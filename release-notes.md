# CrazyPod V1.0

Released: 2026-08-22

V1.0 is CrazyPod's first public experimental release for the iPod Classic
`ipod6g` target family. It is a standalone LVGL firmware product built on the
Rockbox platform, not a Rockbox theme.

> V1.0 passes clean ARM compilation, package integrity, and repository host
> tests. The final release bytes have not completed physical iPod regression.
> Back up the device and keep Apple Disk Mode/DFU recovery available.

## Highlights

- 320×240 Click Wheel interface with a configurable 16-app carousel, lock
  screen, power menu, status surfaces, wallpapers, and 16 icon themes.
- Local music library, search, queue, favorites, shuffle/repeat, resume, LRC
  lyrics, Cover Flow, artwork cache, podcasts, and direct wheel volume.
- Media app with JPG/BMP photos, favorites, zoom/pan, and pre-transcoded
  MPEG-1/2 video playback with seek and resume.
- Notes, EPUB/TXT/Markdown books, calendar/ICS, contacts/VCF, workouts, clock,
  stopwatch, EQ Studio, USB charge/data selection, and idle power-off.
- Nine interface languages and 839 audited localization entries.
- CPK5 Native AOT Mini Apps: Native Reference, Capability Lab, and 2048.
- Native ABI 1.20 Now Playing themes: Neon and Signal One. The device runs no
  JavaScript engine; TSX is compiled to native ARM code before packaging.
- Package-private and semantic regional Noto font service with 38 bundled
  tuples and complete coverage for the current 1317-character UI manifest.

## Release fixes

- Added centered success/error notifications and simplified screenshot-save
  feedback ownership.
- Corrected book-reader input/session transitions and removed stale workflow
  state.
- Consolidated photo screen/controller ownership and settings catalog state.
- Added click-wheel button coverage used by the updated UI input paths.
- Regenerated all fixed localization fonts after new translated copy added 18
  previously missing glyphs.
- Removed 11 dead localization keys and registered the new choice pagination
  format; strict localization audit now reports zero errors and warnings.
- Made EPUB 2 coverage deterministic instead of depending on an intermittently
  failing Project Gutenberg endpoint.
- Prevented historical Now Playing CPK files in `dist/miniapps` from leaking
  into a clean firmware archive.

## Assets

| File | Purpose | SHA-256 |
| --- | --- | --- |
| `CrazyPod-V1.0-iPod6G.zip` | Complete install archive | `3574a52689254b3069336df68f50457f8a6a571e3fdb6e34b4a471c483315f25` |
| `CrazyPod-V1.0-iPod6G-rockbox.ipod` | Standalone firmware binary for controlled updates | `5c431005dcda6f7d30b3c5ca726bce6b2c0e420c416e652e47eaa1219acf81f7` |
| `SHA256SUMS.txt` | Download integrity checks | — |

Install the complete ZIP for a first CrazyPod copy or when resources changed.
Do not install only `rockbox.ipod` on a device that lacks the matching V1.0
fonts, icons, codecs, and CPK packages.

## Installation and upgrade

The [README installation tutorial](README.md#install-crazypod-v10) covers:

- compatible models and FAT32 requirements;
- first-time bootloader installation on Windows, macOS, and Linux;
- DFU and Apple Disk Mode entry;
- non-destructive firmware copy with `rockbox.ipod` written last;
- SHA-256 verification, safe eject, dual boot, and recovery.

Existing `.crazypod` settings and user media are preserved. State versions
older than 14 are migrated by the firmware. The release archive does not
install or replace a bootloader.

## Validation

- UI architecture, UI host, Mini App host, EPUB, font, localization, and
  whitespace gates pass.
- A clean `VERSION=V1.0 ./build-hw.sh` completed with
  `arm-none-eabi-gcc 16.1.0`.
- Firmware and archive both report target `ipod6g` and version `V1.0`.
- The 499-entry install ZIP passes `unzip -tq` and contains only the five CPK5
  packages generated or selected by the current build.

See [PROJECT_STATUS.md](PROJECT_STATUS.md) for the exact physical-validation
boundary and [BUILD.md](BUILD.md) for the reproducible build procedure.
