# CrazyPod project status

Status date: 2026-07-30 09:38 CST

This document separates implemented code from build results, device
installation records, and design proposals. A task discussion is not evidence
that a feature exists; the current source and successful builds take
precedence.

## Evidence used

The status below is based on the current source tree, successful simulator and
iPod 6G builds, Mini App host tests, instruction-level ARM execution,
package inspection, and recorded physical-device copy checks. Design
discussions without corresponding source or test evidence are listed
separately.

### Conversation coverage

This audit re-read the CrazyPod tasks updated after the 01:25 documentation
sync, including localization, lock-screen interaction, Now Playing controls,
icon alignment, and the active display-flicker investigation. Claims were
checked against the current source, generated artifacts, working-tree diff,
and recorded device-copy results. The Codex index is a recent-history window,
not a complete archive of every CrazyPod task.

## Recent progress: 2026-07-29 to 2026-07-30

### Implemented in the current source

- Split `crazypod_ui.c` from 18,456 lines into feature, shell, presentation,
  navigation, platform, EPUB, photo, video, wallpaper, and Mini App modules.
  The current composition root is 800 lines. The architecture gate rejects
  feature-private dependencies from `ui/app/`.
- Reworked Home rendering and input: fixed the 50fps motion clock, added Q16
  wheel motion and snap calculations, reduced renderer hot-path work, filtered
  small click-wheel movement, and reduced the cost of all six waveform styles.
- Reworked the CV8 artwork cache around unique albums, one 128x128 RGB565
  cache image per album, path-ordered generation, 64-album checkpoints,
  resumable preparation, and source-signature-based invalidation. Firmware-only
  USB updates no longer require a full artwork rebuild after the new firmware
  has run once.
- Added artwork-derived three-color palettes for Home and Now Playing
  waveforms. Removed the Radial Spectrum center line and simplified the
  Now Playing Vinyl Groove.
- Fixed Settings and More menu corruption caused by a negative window index
  when fewer than seven rows were present.
- Fixed the lock/wake race between the UI and backlight queues, the Home
  0.5-second MENU hold to Now Playing, the three-second PLAY hold power menu,
  and the USB Data full-screen input block.
- Added a shared centered empty-state overlay and applied it to empty Music,
  Media, Books, Podcasts, Contacts, Workouts, Mini Apps, and related routes.
- Added a persistent `My Favorites` playlist, the Now Playing favorite action
  and heart indicator, queue auto-close after selecting a track, direct
  click-wheel volume control on the main Now Playing screen, and the separate
  `PROGRESS` seek view.
- Replaced the manual track-skip square wave with a trimmed 90ms embedded
  `Sharp_Pop` sample mixed through the beep channel without resetting music
  playback.
- Added Latin/CJK collation, pinyin A-Z grouping, fast-wheel section jumps,
  a 760ms letter HUD, and shared marquee behavior for Home, Now Playing, and
  selected list rows.
- Added nine-language runtime localization: English, Simplified Chinese,
  Traditional Chinese, Japanese, Korean, German, French, Spanish, and Brazilian
  Portuguese. Settings applies the language immediately and state version 10
  persists it; older state versions migrate to English.
- Added 856 firmware translation keys, 27 translated Calculator/Pomodoro
  strings, strict missing-key and bare-string audits, UTF-8 LCD decoding, and
  complete 8/10/12/14/16px font subsets.
- Replaced the lock-screen wheel-distance gesture with a 0.5-second Select
  hold. Early release cancels progress; the opening shackle animation and
  post-animation input isolation were rebuilt.
- Added the temporary 18px-wide left-edge volume HUD on Now Playing. Playback,
  favorite, repeat, shuffle, lyrics, progress, and queue status icons now use
  consistent optical centering, with repeat-one rendered as a combined symbol.
- Unified menu-row icon and text vertical centering, and removed the fixed text
  clipping heights from search and choice overlays.
- Detached `Gigass/CrazyPod` from the upstream GitHub fork network. `origin`
  points to the independent repository, and `upstream` remains fetch-only
  because its push URL is `file:///dev/null`.

### In progress or not yet complete

- Localization and the recent interaction/presentation changes remain
  uncommitted in a broad working tree. They need review and coherent commits;
  the old description of only two uncommitted marquee files is no longer true.
- 3.5mm headset remote support remains unimplemented. The current iPod 6G
  target lacks the Mikey remote driver, and CrazyPod's raw-button
  normalization would also need multimedia/remote event handling.
- Local `main` and `stable`, plus `origin/main` and
  `origin/stablerelease1`, point to `6c5f960bb7`. `origin/dev` is two commits
  behind. `release` still has three commits outside `main`, so branch cleanup
  remains unresolved.
- Intermittent white block flicker has been reported in both CrazyPod and the
  stock iPod firmware. Hardware diagnosis is active; no LCD, connector, power,
  or mainboard root cause has been established.

## Validation snapshot

| Level | Result |
| --- | --- |
| Source review | Recent Codex tasks, commits since 2026-07-29, current source, branch state, and working-tree diff inspected |
| Simulator build | Current working tree passes the incremental simulator build; `build-sim/rockboxui` was produced at 2026-07-30 09:08 CST |
| ARM build | Current working tree passes the `ipod6g` build, stack gate, package build, and ZIP integrity check |
| UI and architecture tests | Current working tree passes UI host tests, Mini App host tests, EPUB host tests, the architecture gate, and `git diff --check` |
| Localization | Strict audit reports 0 errors and 0 warnings; all five generated font sizes cover all 1,344 required non-whitespace characters |
| Video simulator test | MPEG-2 video and MP2 audio open through the integrated engine; poster rendering, playback controls, and persisted resume at 0:06 pass |
| Mini App host tests | SDK ABI-prefix, Calculator, Pomodoro, crypto, and host input-pacing tests pass, including revision 3 tail gating, fixed-layout bitmap commands, one visible focus step per frame, bounded backlog, direction reversal, and accelerated numeric editing |
| ARM Mini App runtime | Calculator payload loaded at its real ARM address with dirty-then-cleared BSS renders `0` initially and after AC in instruction-level emulation |
| Mini App runtime integration | Same-version repair and App-before-UI alarm delivery pass in an isolated simulator root |
| Mini App launch path | Package verification is cached by app ID and version after startup/install/USB rescan; normal list entry and launch no longer repeat Ed25519, icon, binary, and resource hashing; FAT AppleDouble `._*.cpk` files are ignored before parsing |
| Package test | `CrazyPod-6G.zip` passes `unzip -tq` |
| Package contents | 324 ZIP entries, including 39 playback codecs, 256 app icons, and 2 signed Mini App packages |
| Physical copy | Latest recorded device firmware is SHA-256 `6e087dc7202a8fce085c7a78be45ad01ece66b33bc4710bc20f06b54f6f62b9d`; 324 package entries were verified, the FAT32 check passed, user media and state were retained, and the iPod was safely ejected |
| Full hardware regression | Incomplete |

Artifact snapshot from this audit:

```text
rockbox.ipod
  size:    2,303,296 bytes
  sha256:  6e087dc7202a8fce085c7a78be45ad01ece66b33bc4710bc20f06b54f6f62b9d

CrazyPod-6G.zip
  size:    10,149,192 bytes
  sha256:  b11c07416b37913b04da4e0b4e35d7388bd3a724e2befe10080327dc2108a9b9

build-sim/rockboxui
  size:    4,885,928 bytes
  sha256:  a945130c29b157717595b1d08401c24baab3dfe99bea74efc55f9c55eb8eec94
```

These artifacts include the current localization, unlock, Now Playing, icon,
and menu-alignment work. The latest recorded device copy matches the hardware
artifact and was preceded by backup
`device-backups/20260730-091352-before-menu-alignment`. No bootloader change
was made.

## Implementation audit

### Implemented and present in source

| Area | Consolidated result |
| --- | --- |
| Architecture | Feature-oriented UI modules, feature registry, active-feature dispatch, navigation commands, platform display facade, and an 800-line composition root guarded by structural tests |
| Home | Native application carousel, continuous Q16 click-wheel motion, small-movement filtering, opaque themed icons, optimized waveform styles, and a wallpaper-aware frosted now-playing capsule |
| Music library | Local metadata scan, artists, albums, songs, M3U/M3U8 playlists, persistent favorites, pinyin/Latin collation, A-Z fast jump, search, dynamic playback queue, and resumable CV8 album artwork preparation |
| Now Playing | Atomic cover/background handoff, local synchronized LRC lyrics, queue, favorite control, shuffle/repeat, direct wheel volume with a temporary left-edge HUD, progress seeking, marquee text, optically unified 16×16 action icons, artwork palette waveforms, and contrast-aware text |
| Cover Flow | Native RGB565 renderer, 50fps frame clock, artwork caching and prefetch, subpixel edge coverage, C2-continuous Q16 pose curves, preserved release velocity, and projected snap physics |
| Media | Photos, Videos, and Favorites routes; recursive `/Pictures` browsing; pre-transcoded MPEG-1/2 video playback from `/Videos`; cached photo thumbnails and video posters; play/pause, seek, volume, and resume |
| Appearance | 16 icon themes, colors, glow, scale, wallpapers, independent screen radii, presets, and `.upodtheme` import/export |
| Localization | Nine runtime languages, immediate Settings selector, 856 firmware keys, 27 built-in Mini App keys, persisted language, strict audits, UTF-8 LCD text, and five fully covered shared-SC font sizes; region-specific Han glyph shapes are not implemented |
| Settings | Language, sound, EQ Studio, display, backlight, Reduce Motion, playback, power, sleep timer, USB charging, click feedback, and main-menu order |
| Applications | Notes, Books, Podcasts, signed native Calculator and Pomodoro Mini Apps with ABI 1 revision 3 host dialogs, CPK2 resources, bitmap drawing, and read-only Now Playing metadata, Workouts, More Features, Settings, Shuffle, Lock, and Media |
| Organizer | Local calendar events, ICS import, VCF contacts, clock, stopwatch, and sleep timer |
| Lock screen | Immediate lock, automatic lock path, first-key wake isolation, custom wallpaper, DIY radii, large clock, 0.5-second Center-button hold with early-release cancellation, and animated shackle opening |
| USB and power | Charge/Data prompt, mass-storage mode, charge-only mode, corrected iPod 6G SUSP/HPWR charging control, and a three-second Play-hold Shut Down / Restart confirmation menu |
| Boot | Optional silent bootloader surface, shared firmware handoff mark, and a 220ms desktop fade; the normal firmware archive does not contain the bootloader |
| Menu previews | Interruptible two-layer first-level previews for Music, Media, Notes, and Books, with Reduce Motion and bounded media loading |

### Implemented but still needing focused hardware regression

- Current menu-preview entrance/exit smoothness and dropped-frame behavior.
- The new Q16 Hermite Cover Flow motion and projected snap behavior on a
  physical click wheel.
- The LCD scanline-synchronized full-frame path on each supported iPod 6G
  panel type. The current implementation targets the 8-bit panel command path.
- Lock-screen wake, 0.5-second Center hold, early-release cancellation, and
  residual-input isolation across repeated sleep cycles.
- Language changes, all nine language surfaces, all five font sizes, localized
  Calculator/Pomodoro text, and long-string clipping on a physical iPod.
  Simulator and ARM checks pass, but a complete device matrix is not recorded.
- Now Playing volume-HUD timing and the optically normalized action icons on a
  physical display.
- The Play-hold power menu, restart path, and absence of the previously
  observed LCD white flash across repeated attempts.
- Charge/Data selection on different hosts and cables.
- Charging behavior through low, middle, and high battery states.
- Large photo, EPUB, library, and playlist limits on a filled device.
- CP936 TXT reading with common and uncommon Chinese characters. The code and
  packaged codepage are verified, and a build was copied, but a complete
  content corpus was not recorded.
- Physical `/Podcasts` scanning and playback. The scanner is present in the
  latest copied aggregate build, but no post-boot podcast result was recorded.
- The post-fix Calculator zero/AC display and one-control-per-wheel-event
  behavior on a physical iPod. The zero/AC build was installed, but the
  subsequent visible-frame pacing fix is not yet installed.
- Foreground/background Pomodoro transitions, screen-off alarms, and music
  coexistence on a physical iPod.
- Revision 3 host text/choice/confirmation interaction, RGB565 resource
  rendering, and Now Playing snapshots on a physical iPod.
- MPEG-1/2 playback performance, audio/video synchronization, battery impact,
  seeking, and resume behavior on a physical iPod. The device listed converted
  videos; an AppleDouble false-entry bug was fixed and reinstalled, but the
  post-reboot playback result was not recorded.
- The optional bootloader's power-on display, recovery paths, and firmware
  handoff. It has been built but not flashed.

### Design proposals only

- Mikey-based 3.5mm headset remote support and the required CrazyPod remote
  input normalization.
- Lua or another sandboxed Mini Apps runtime.
- General Mini App permissions, capability isolation, and native-code
  sandboxing.
- A mediated Mini App file picker and export area. This is deferred behind the
  implemented revision 3 UI and resource work.
- Production signing-key custody, rotation, and revocation.
- Network music, Subsonic, online lyrics, or network import.

Current Mini Apps use a restricted native `.cpk` loader with exact ZIP,
manifest, target, ABI, payload, icon, hash, CRC, and Ed25519 verification.
There is no native-code sandbox or capability isolation. The committed signing
key is a public development key and must not be used as a production trust
root.

## Product boundary

CrazyPod supports only the Rockbox `ipod6g` target. Rockbox remains the
low-level platform for codecs, buffering, playlists, PCM, storage, power, USB,
kernel, and device drivers. CrazyPod excludes the Rockbox menu, browser, WPS,
skin engine, theme system, plugin UI, recording pipeline, USB Audio, HID, and
iPod accessory protocol.

Camera and voice recording are absent because the target lacks the required
camera and microphone. Workouts record elapsed time only.

## Release blockers

1. Run a clean, repeatable simulator and ARM build in CI.
2. Complete a documented iPod 6G regression matrix.
3. Review and commit the current localization and interaction working tree in
   coherent changes, then record post-boot physical interaction checks.
4. Test Home wheel dead-zone, motion, and snap behavior on a physical click
   wheel.
5. Test USB Charge/Data, artwork-cache preservation, and charging behavior on
   physical hardware.
6. Test Mini App installation, wheel input, background Pomodoro alarms, and
   concurrent music playback on physical hardware.
7. Test the custom bootloader on hardware and replace or clear the
   Apple-shaped mark before an unaffiliated public release.
8. Test content limits and corrupted-file handling.
9. Keep device backups and generated build products out of Git; the repository
   no longer tracks `device-backups`, but thousands of local backup files still
   need an external retention policy.
10. Normalize the branch set after preserving or explicitly discarding the
    three `release`-only commits.
11. Replace the Mini App development trust key and define key rotation before
   accepting production packages.
12. Publish a versioned release, checksums, recovery requirements, and an
   installation procedure.

Until these are complete, CrazyPod should be described as experimental
firmware, not a stable end-user release.
