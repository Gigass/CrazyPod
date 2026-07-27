# CrazyPod project status

Status date: 2026-07-27

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

The current Codex 50-task index exposes 27 CrazyPod tasks. This audit read the
new and changed tasks and reused the prior documentation pass for unchanged
items that were already inspected. The index has no cursor for tasks that have
fallen outside its current window, so this is a complete audit of the
currently exposed CrazyPod history, not a claim about every task ever created.

## Validation snapshot

| Level | Result |
| --- | --- |
| Source review | CrazyPod product paths and current feature routes inspected |
| Simulator build | `./build-sim.sh --incremental` passes |
| ARM build | `./build-hw.sh --incremental` passes for `ipod6g`; all eight main/IRQ/FIQ stack symbols pass the 8-byte alignment gate |
| Video simulator test | MPEG-2 video and MP2 audio open through the integrated engine; poster rendering, playback controls, and persisted resume at 0:06 pass |
| Mini App host tests | SDK ABI-prefix, Calculator, Pomodoro, crypto, and host input-pacing tests pass, including revision 3 tail gating, fixed-layout bitmap commands, one visible focus step per frame, bounded backlog, direction reversal, and accelerated numeric editing |
| ARM Mini App runtime | Calculator payload loaded at its real ARM address with dirty-then-cleared BSS renders `0` initially and after AC in instruction-level emulation |
| Mini App runtime integration | Same-version repair and App-before-UI alarm delivery pass in an isolated simulator root |
| Mini App launch path | Package verification is cached by app ID and version after startup/install/USB rescan; normal list entry and launch no longer repeat Ed25519, icon, binary, and resource hashing; FAT AppleDouble `._*.cpk` files are ignored before parsing |
| Package test | `CrazyPod-6G.zip` passes `unzip -tq` |
| Package contents | 324 ZIP entries, including 39 playback codecs, 256 app icons, and 2 signed Mini App packages |
| Physical copy | Current revision 3 firmware and both Calculator/Pomodoro 1.2.0 packages were copied to `/Volumes/CRAZYPOD`; source/device SHA-256 values match and a post-write FAT verification passed |
| Full hardware regression | Incomplete |

Artifact snapshot from this audit:

```text
rockbox.ipod
  size:    1,432,268 bytes
  sha256:  064042544db9944b9f382eaa5a4b7951e6f33946390affa199b83a1dd1b9142f

CrazyPod-6G.zip
  size:    9,682,414 bytes
  sha256:  31826636f52d4912cee03cdd625a1f71e77f21765f4bd184e9d92fc2d6ad099d

calculator-1.2.0.cpk
  size:    111,346 bytes
  sha256:  f3759e04ebe3c780f0dae9e3b97b70fd12dfcea01cb9c3270cab907497ff0000

pomodoro-1.2.0.cpk
  size:    111,112 bytes
  sha256:  8fed98e6d8eaaf43121ff3b51c96a7d2b02f57b89e21dc268df43d1198e9f432
```

A pre-existing FAT32 cross-link was repaired only after readable `.rockbox`
and `.crazypod` data was backed up. Later firmware copies passed per-file or
firmware-hash comparison and post-write FAT32 checks. The most recent recorded
copy contains Mini App SDK revision 3 and the launch verification-cache fix.
Its device SHA-256 is
`064042544db9944b9f382eaa5a4b7951e6f33946390affa199b83a1dd1b9142f`,
matching the artifact above.

## Implementation audit

### Implemented and present in source

| Area | Consolidated result |
| --- | --- |
| Home | Native application carousel, opaque themed icons, playback controls, waveform styles, and a wallpaper-aware frosted now-playing capsule |
| Music library | Local metadata scan, artists, albums, songs, M3U/M3U8 playlists, search, and dynamic playback queue |
| Now Playing | Atomic cover/background handoff, local synchronized LRC lyrics, queue, shuffle/repeat, volume, and contrast-aware text |
| Cover Flow | Native RGB565 renderer, 50fps frame clock, artwork caching and prefetch, subpixel edge coverage, C2-continuous Q16 pose curves, preserved release velocity, and projected snap physics |
| Media | Photos, Videos, and Favorites routes; recursive `/Pictures` browsing; pre-transcoded MPEG-1/2 video playback from `/Videos`; cached photo thumbnails and video posters; play/pause, seek, volume, and resume |
| Appearance | 16 icon themes, colors, glow, scale, wallpapers, independent screen radii, presets, and `.upodtheme` import/export |
| Settings | Sound, EQ Studio, display, backlight, Reduce Motion, playback, power, sleep timer, USB charging, click feedback, and main-menu order |
| Applications | Notes, Books, Podcasts, signed native Calculator and Pomodoro Mini Apps with ABI 1 revision 3 host dialogs, CPK2 resources, bitmap drawing, and read-only Now Playing metadata, Workouts, More Features, Settings, Shuffle, Lock, and Media |
| Organizer | Local calendar events, ICS import, VCF contacts, clock, stopwatch, and sleep timer |
| Lock screen | Immediate lock, automatic lock path, first-key wake isolation, custom wallpaper, DIY radii, large clock, and wheel-distance unlock |
| USB and power | Charge/Data prompt, mass-storage mode, charge-only mode, corrected iPod 6G SUSP/HPWR charging control, and a three-second Play-hold Shut Down / Restart confirmation menu |
| Boot | Optional silent bootloader surface, shared firmware handoff mark, and a 220ms desktop fade; the normal firmware archive does not contain the bootloader |
| Menu previews | Interruptible two-layer first-level previews for Music, Media, Notes, and Books, with Reduce Motion and bounded media loading |

### Implemented but still needing focused hardware regression

- Current menu-preview entrance/exit smoothness and dropped-frame behavior.
- The new Q16 Hermite Cover Flow motion and projected snap behavior on a
  physical click wheel.
- The LCD scanline-synchronized full-frame path on each supported iPod 6G
  panel type. The current implementation targets the 8-bit panel command path.
- Lock-screen wake, wheel unlock, decay, and residual-input isolation across
  repeated sleep cycles, including the current 19-step threshold.
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
3. Test the completed Cover Flow motion-curve change on a physical click wheel.
4. Test USB Charge/Data and charging behavior on physical hardware.
5. Test Mini App installation, wheel input, background Pomodoro alarms, and
   concurrent music playback on physical hardware.
6. Test the custom bootloader on hardware and replace or clear the
   Apple-shaped mark before an unaffiliated public release.
7. Test content limits and corrupted-file handling.
8. Remove device backups and generated build products from Git tracking in a
   dedicated cleanup change.
9. Replace the Mini App development trust key and define key rotation before
   accepting production packages.
10. Publish a versioned release, checksums, recovery requirements, and an
   installation procedure.

Until these are complete, CrazyPod should be described as experimental
firmware, not a stable end-user release.
