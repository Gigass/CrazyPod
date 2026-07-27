# CrazyPod independent product firmware

This revision replaces the static launcher with a persistent, configurable
main menu and implements the firmware-capable MaxPod applications.

Implemented:

- silent black iPod 6G boot surface with a centered white Apple mark, continuous
  CrazyPod LCD handoff, and a 220 ms desktop fade
- LVGL 9.5.0 low-level integration
- dedicated RGB565 framebuffer and panic display
- MaxPodApp-derived animated 16-application icon carousel
- click-wheel focus, Select, Menu, Left, Right, and Play handling
- animated 3D carousel focus transitions, reflections, position indicators,
  default MaxPod wallpaper, and the compressed 6G now-playing capsule
- live battery and clock status
- minimal USB mass-storage configuration
- recursive local metadata scan on a background thread
- artists, albums, songs, M3U/M3U8 playlists, and local text search
- album artwork, queue, shuffle, repeat, resume persistence, codec decoding,
  buffering, and PCM output
- synchronized sidecar `.lrc` / `.LRC` lyrics
- atomic Now Playing artwork handoff: the foreground cover, blurred background,
  and contrast color change together after the new presentation is ready
- native 50fps Cover Flow with a 25-slot artwork window, bidirectional
  prefetch, RGB565 subpixel edge coverage, C2-continuous Q16 Hermite pose
  curves, accumulated wheel momentum, preserved release velocity, and 200ms
  projected snapping
- full-frame LCD submission synchronized to the 8-bit panel scan position to
  address intermittent Cover Flow tearing
- a wallpaper-aware frosted Home capsule, configurable sound-wave style, and
  direct Home playback controls
- MaxPod-derived icon, detail, background, and appearance-preset controls
- validated, versioned `.upodtheme` import/export
- 16 packaged 72×72 application icon themes
- independent Home, Menu, and Lock Screen wallpaper controls
- independent top and bottom screen corner radii applied to the desktop,
  application routes, and lock screen
- Settings > Main Menu visibility and order controls
- Sound, EQ Studio, Display, Playback, Power, Controls, USB charging, click
  feedback, sleep-timer, and Reduce Motion settings
- interruptible two-layer skeuomorphic previews for every first-level Music,
  Media, Notes, and Books menu item, with object-specific entrance and exit
  motion
- procedural preview surfaces that remain complete while artwork loads;
  optional album-art and photo skins fade in without spinners or replaying the
  scene, with a bounded four-cover cache budget for All Music
- persisted Settings > Display > Reduce Motion support
- deterministic simulator snapshot routes for all new preview variants
- More Features as the live list of hidden main-menu applications
- restricted signed native `.cpk` installation with exact manifest, payload,
  icon, target, ABI, hash, and Ed25519 verification
- a shared Mini App scene and click-wheel input ABI with appearance-token
  rendering
- ABI 1 revision 2 with a stable legacy host-table prefix, capability-gated
  system status, date/time and duration formatting, four alarm slots, host
  toasts, asynchronous close requests, dividers, and linear progress
- backward loading of older ABI 1 packages whose required host table is a
  valid prefix of the current SDK
- manifest-derived Mini App package filenames, so version upgrades no longer
  require a hard-coded builder filename change
- a standard Calculator with chaining, contextual percent, repeat equals,
  sign, decimal, backspace, and error recovery
- exact one-control Mini App wheel navigation at every scroll speed, while
  numeric Pomodoro setup edits retain accelerated adjustment
- a bounded host-side Mini App input queue that consumes at most one discrete
  focus event per presentable frame, discards stale movement on direction
  reversal, and prevents Select from targeting a focus state not yet visible
- explicit 8-byte iPod 6G main, IRQ, and FIQ stack alignment so EABI `double`
  and variadic formatting remain valid on hardware
- a Pomodoro timer with editable focus/break durations and rounds, persisted
  deadlines, background alarms, pause, skip, reset, and manual phase advance
- local notes with drafts, pinning, search, duplication, trash, restore, and
  hold-confirmed deletion
- EPUB, TXT, and Markdown reading with progress, recents, favorites,
  bookmarks, EPUB chapters, font sizes, paper themes, confirmed deletion, and
  automatic UTF-8 or GBK/GB2312 (CP936) text decoding
- local podcasts from `/Podcasts`
- a unified Media app with Photos, Videos, and Favorites
- JPG, JPEG, and BMP photo browsing with favorites, full-screen viewing, zoom,
  pan, cached thumbnails, and wallpaper crop previews
- pre-transcoded MPEG-1/2 video playback from `/Videos`, with same-basename BMP
  posters, play/pause, 10-second seek, volume, and persistent resume
- a desktop FFmpeg converter for producing device-ready 320×240 MPEG files and
  128×96 poster sidecars
- VCF contacts with folded-line and escaped-text import
- calendar Today, Upcoming, month, local event editing, and read-only ICS import
- live clock, Rockbox sleep timer, and lap stopwatch
- time-only workouts with 20 activity types, pause/resume, persistent history,
  summaries, and confirmed deletion
- immediate manual and automatic lock, wake-key isolation, configurable lock
  appearance, and clockwise 19-step wheel-distance unlock with progress decay
- an insertion-time USB prompt for Charge or Data mode
- a three-second Play-hold power menu with Shut Down and Restart choices;
  CrazyPod owns the hold gesture so the committed Rockbox shutdown broadcast
  cannot reinitialize the LCD before the prompt
- iPod 6G charging control restored to SUSP/HPWR current limiting instead of
  the broken C1/CHRG shutdown path
- hardware package containing playback codecs, wallpaper, and CrazyPod icon
  resources

Removed from the product:

- Rockbox root menu and browser UI
- WPS and skin engine
- Apple2026 shell, assets, fonts, and generators
- plugins and the recording/encoder pipeline
- USB Audio, HID, and iPod accessory protocol
- iPod 5G product target
- camera and voice recording applications

Intentionally not implemented:

- network music, online lyrics, and network import
- physical chassis and click-wheel DIY options
- third-party Mini App sandboxing or capability isolation
- a supported end-user installer

Validation:

- simulator and iPod 6G ARM builds pass
- SDK ABI-prefix, Calculator, Pomodoro, input-pacing, and package-crypto host
  tests pass normally and under ASan/UBSan
- instruction-level execution of the real ARM Calculator payload renders `0`
  both initially and after AC with a deliberately dirty BSS before load
- the generated zip passes an archive integrity test
- simulator MPEG playback, poster rendering, and persisted resume pass; a
  physical device listed the converted files, and the AppleDouble false-entry
  fix was installed, but post-reboot playback and synchronization remain
  unconfirmed
- the prior Calculator zero/step build was installed on an iPod 6G; all 300
  packaged files matched after copy and the post-write FAT32 check exited 0
- the current Mini App SDK revision 2 build passes simulator and ARM builds,
  host tests, visual frames, and package validation but has not yet been
  installed
- the corrected Calculator display and visible wheel sequence still require a
  post-boot physical interaction report
- the optional bootloader builds but has not been flashed or physically
  regression-tested
- the current source still requires release-grade hardware regression testing
