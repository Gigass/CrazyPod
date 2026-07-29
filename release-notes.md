# CrazyPod unreleased changes

Last updated: 2026-07-30

These notes describe the current unreleased CrazyPod source. See
[README.md](README.md) for the full feature baseline and
[PROJECT_STATUS.md](PROJECT_STATUS.md) for build, device, and release-blocker
status.

## Product boundary

CrazyPod remains experimental standalone firmware for the iPod Classic 6G
(`ipod6g`). It uses Rockbox for codecs, playback, storage, power, USB, kernel,
and device drivers while replacing the Rockbox application interface with a
320×240 LVGL product.

The package excludes the Rockbox root menu, browser, WPS, skin and theme
engines, plugin UI, recording pipeline, USB Audio, HID, iPod accessory
protocol, and non-`ipod6g` targets.

## Added

### Music and Now Playing

- Persistent `My Favorites` playlist stored at
  `/.crazypod/favorites.m3u8`.
- Favorite action and state indicator on Now Playing.
- Latin/CJK collation with pinyin A-Z grouping.
- Fast-wheel section jumps after a short input burst, with a 760ms letter HUD.
- Shared overflow marquee for Home, Now Playing, and selected list rows.
- Progress seek surface with five-second wheel steps.
- Direct wheel volume control on the main Now Playing screen without a volume
  modal.
- Queue dismissal after selecting and starting a queued track.
- Artwork-derived primary, secondary, and highlight waveform colors.
- Embedded 90ms `Sharp_Pop` sample for manual track changes. It mixes through
  the beep channel without pausing or resetting music playback.

### Artwork cache

- CV8 stores one 128×128 RGB565 image per unique album.
- First-time generation sorts work by source path to reduce storage seeks.
- Progress persists every 64 albums and resumes after a normal exit or reboot.
- Cache records track artwork source signatures, so unchanged albums survive
  firmware-only USB updates.
- Artwork preparation blocks lock input while active and stops outside Music.

### Shell and presentation

- Shared centered empty-state overlay for empty Music, Media, Books, Podcasts,
  Contacts, Workouts, Mini Apps, and related routes.
- Full-screen black-and-white input block after choosing USB Data mode.
- Home MENU hold opens Now Playing after 0.5 seconds without waiting for
  artwork.
- PLAY hold opens the Shut Down / Restart menu after three seconds without
  also toggling playback.
- Default icon scale increased from 100% to 120% for new state and built-in
  presets.

### Architecture

- Replaced the 18,456-line `crazypod_ui.c` implementation with vertical
  features, Shell, Navigation, Presentation, Platform, and domain modules.
- Reduced `crazypod_ui.c` to the 800-line composition-root limit.
- Added feature facades, active-feature dispatch, navigation commands, and a
  platform display boundary.
- Added an architecture gate that rejects horizontal transition directories,
  cross-feature private includes, mutable UI `extern` state, domain-to-UI
  dependencies, route switches in the composition root, and root-size drift.
- Added host coverage for collation and fast-wheel section-jump state.

### Mini Apps

- ABI 1 revision 3 retains the original host-table prefix and adds
  capability-gated asynchronous text, choice, and confirmation surfaces.
- CPK format 2 adds a deterministic `resources.bin` container and bounded
  RGB565 bitmap rendering.
- Read-only Now Playing snapshots are available through an optional host API.
- Same-version damaged installs repair from a valid package.
- Package verification is cached by app ID and version for the current
  firmware session.

## Changed

- Home uses a fixed 50fps motion clock, Q16 wheel position, reduced renderer
  hot-path work, and a small-movement filter.
- All six Home waveform styles use lower-cost drawing paths. LVGL now keeps a
  32×32 shadow cache, and the Home capsule updates text, artwork state, and
  progress only when values change.
- Radial Spectrum no longer draws the horizontal center line on Now Playing.
- Now Playing Vinyl Groove uses three restrained, phase-aligned lines instead
  of five high-amplitude lines.
- The music-library cache format is version 3 so collation order and section
  jumps use the same key.
- Settings, More, and other short menus share the corrected row-window logic.
- `Gigass/CrazyPod` is an independent GitHub repository. `origin` points to it;
  the local `upstream` remote remains fetch-only.

## Fixed

- Settings and More menu corruption after scrolling a list with fewer than
  seven rows. The previous window calculation could produce a negative source
  index.
- Lock-screen white/black wake sequence caused by stale LCD sleep work racing a
  newer backlight-on request.
- Short Home wheel movement triggering icon motion while the user intended to
  click.
- Long PLAY triggering play/pause before the power menu.
- Queue selection leaving the queue overlay open.
- Empty-page content being rendered under or over inconsistent inline notices.
- Repeated USB entry invalidating completed artwork work after firmware-only
  updates.

## Validation

- Simulator build and pixel-level short/long marquee checks pass for the
  current working tree.
- UI, Mini App, EPUB, and architecture host gates pass.
- The current working tree builds and packages for `ipod6g`; its ZIP contains
  324 entries and passes `unzip -tq`.
- The latest recorded installed firmware has SHA-256
  `1b939f9266c89aac4d13635a3da5755189d59e7a9d6b956328fd655fb3c0440b`.
- All 301 package files matched after device copy, and the iPod was safely
  ejected. The two marquee source files remain uncommitted.
- Full release-grade physical regression remains incomplete.

## Not implemented

- Nine-language localization. The firmware UI remains English-only and has no
  language selector, translated string table, font subsets, or state migration.
- 3.5mm headset remote control. The target does not yet initialize the Mikey
  remote controller or normalize its multimedia events for CrazyPod.
- Native Mini App sandboxing, general permissions, direct filesystem access,
  audio callbacks, networking, sensors, or a multilingual IME.
- A supported end-user installer.

## Upgrade notes

- CrazyPod persistent state remains version 9.
- The music-library cache upgrades to version 3 and may rescan once.
- Artwork uses CV8. The first run of the new cache format prepares each unique
  album once; later runs resume or preserve unchanged entries.
- The optional bootloader remains separate from `CrazyPod-6G.zip` and has not
  completed physical boot regression.
