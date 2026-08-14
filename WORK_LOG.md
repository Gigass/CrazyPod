# WORK_LOG.md

Entries are point-in-time records. Use the newest entry and
[PROJECT_STATUS.md](PROJECT_STATUS.md) for current status; older open items may
have been completed later.

## 2026-08-10 (Lock-screen power and idle shutdown)

Goal: stop indefinite battery drain after the display is locked while keeping
screen-off music playback correct.

Changed:

- added Settings → Power → Idle Power Off with Off and 1–60 minute choices;
- defaulted fresh and v1–v10 migrated state to 10 minutes and upgraded state
  persistence to v11;
- restored real `audio_status()` use in the iPod 6G automatic-poweroff path so
  active playback inhibits shutdown while paused/stopped playback may time out;
- applied LCD sleep and all other runtime settings even when `state.bin` is
  absent, removing the first-boot 1-second/10-second LCD mismatch;
- made an externally powered sleep timer pause audio when it expires;
- replaced the locked, backlight-off 1 Hz UI loop with an event/deadline-driven
  wait: no playback, pending save, or alarm now blocks indefinitely until a
  button or Rockbox system event arrives;
- stopped LVGL timers, display presentation and ordinary runtime services while
  the locked display is off; playback checkpoints wake at most every 30 seconds
  and Mini App alarms wake at their nearest absolute deadline;
- kept click-wheel hardware event ownership unchanged because
  `wheel_send_events(false)` still generates wheel interrupts and `BUTTON_NONE`
  wakeups, so it does not provide the claimed hardware power saving;
- added host coverage for indefinite waits, minimum-deadline selection, due
  alarms and timeout overflow;
- documented the lock, LCD, audio, storage, idle-poweroff and verification
  contracts in `docs/CRAZYPOD_POWER_MANAGEMENT.zh-CN.md`.

Verified and installed:

- UI architecture and UI host tests pass;
- strict nine-language localization audit passes with no errors or warnings;
- incremental iPod 6G build completes and packages a valid release archive;
- repaired one orphaned FAT32 cluster on `CRAZYPOD` (`/dev/disk18s1`) and passed
  read-only filesystem verification before and after installation;
- backed up and checksum-compared the active device data under
  `~/CrazyPod-device-backups/20260810-before-idle-poweroff`;
- installed and byte-compared all 452 packaged files without delete-mirroring;
  installed `rockbox.ipod` SHA-256 is
  `874da2a7db5e95eb09219e269fa7dd92467972cacf082839d6e4acc6b6a22ded`;
- preserved the 268-byte v10 `state.bin` unchanged so the first device boot can
  exercise the real v10-to-v11 migration path;
- after the event/deadline screen-off change, repeated the architecture, UI,
  Mini App, EPUB and strict localization suites plus simulator and iPod 6G
  builds; the final installed `rockbox.ipod` SHA-256 is
  `7c087360a9c1cad32d5061b4eb325dfe36f2dc1f0ae227350d4686252d4db86e`,
  and the release ZIP passes its compressed-data integrity check;
- backed up and content-compared the active device data under
  `~/CrazyPod-device-backups/20260810-before-screen-off-idle`, installed and
  content-compared all 452 release files with firmware last, preserved
  `state.bin` byte-for-byte, passed the post-install FAT32 check and safely
  ejected `/dev/disk18`;
- physical idle-timeout, playback-inhibit and external-power behavior still
  require post-boot timing tests on the device; build, installation and media
  integrity do not prove those hardware-time behaviors.

## 2026-07-31 (Mini App Native AOT phases 1–7)

Goal: replace the device-side script runtime with React-style TSX compiled to
native ARM while preserving host-owned LVGL and reproducing the 2048-to-Lab
hang/latency path before optimizing it.

Changed:

- implemented CPK5, Native ABI 1 and direct Host UI calls
- implemented the constrained React Profile TSX-to-C compiler and CLI
- compiled the same generated C to simulator dylibs and hardware app.arm
- migrated Native Reference, Capability Lab and 2048
- added native focus/input routing and CRC-checked 2048 state
- removed QuickJS, Solid, ABI3, CPK4 and UI command batching
- changed AOT rendering from full-tree replacement to retained handles and
  dynamic-property updates
- rewrote host, simulator matrix and end-to-end reproduction tests

Measured:

- the full-tree renderer failed the five-cycle reproduction at p95 360ms,
  maximum 390ms and heartbeat 310ms
- retained handles reduced the measured movement path to p95 60ms and maximum
  70ms; first dylib cold load remains about 290–300ms and is measured
  separately

Limit:

- React Profile v1 is a documented subset, not arbitrary TypeScript-to-C;
  2048 domain logic currently uses a build-time platform intrinsic
- current physical-device validation must be rerun after the final ARM build

## 2026-07-31 (Mini App fixed-arena transition fix)

Goal: address the physical-device failure where Capability Lab does not open
after 2048 has processed gameplay input.

Changed:

- allocate the same fixed 8 MiB immovable core arena for every QuickJS session
  while continuing to enforce the 4–8 MiB manifest limit inside QuickJS; this
  makes every close leave an arena-sized hole that the next application can
  reuse instead of depending on a larger contiguous allocation after a smaller
  session
- reduce Capability Lab's logical heap from 8 MiB to 4 MiB and publish it as
  `3.0.3`; measured Lab JS usage remains below 1 MiB
- add a capped `/.crazypod/miniapp-runtime.log` recording every open/close
  phase, result, logical heap, JS usage, Host usage and free core memory
- replace the ineffective immediate-open/immediate-close transition test with
  three cycles that enter 2048, execute 16 moves with animation/timer service,
  close while a debounced state write is pending, then open and operate Lab

Verified:

- all 26 builder tests and the complete Mini App host test script pass
- the native simulator transition test passes all three active cycles; each
  2048 close returns JS, scheduled work, Host memory and scene content to zero
  before Lab opens
- incremental simulator and ARMv5 builds pass; the 324-entry release ZIP passes
  `unzip -tq`
- the Target 71 / 64 MiB FAT32 volume was backed up with 5,178 non-AppleDouble
  files, checked before and after the write, updated with firmware hash
  `096f7914...f767` and Lab 3.0.3 hash `13143c8d...728f`, read back
  byte-for-byte and safely ejected
- physical postboot reproduction remains unverified until the device boots and
  the exact active 2048 → exit → Capability Lab sequence is repeated

## 2026-07-31 (Mini App device performance hot-path fix)

Goal: remove the real-device stalls observed while opening, using and switching
between 2048 and Capability Lab.

Changed:

- boost the iPod CPU before synchronous QuickJS application bootstrap, direct
  input callbacks and confirmed application close
- index compiled CSS rules by the rightmost selector token and cache the
  focusable-node list instead of scanning every rule and rebuilding the full
  tree on each input event
- debounce 2048 gameplay persistence so every move no longer performs a
  synchronous temporary-file write, `fsync` and rename; application close still
  commits the latest state
- derive CPK filenames from each manifest version in both build scripts,
  eliminating the duplicate version source that initially repackaged stale
  `4.0.0`/`3.0.1` names
- publish the fixes as 2048 `4.0.1` and Capability Lab `3.0.2`

Verified:

- focused builder runtime/CSS/build tests pass 11/11; Mini App C host tests and
  the 798-line UI architecture gate pass
- native simulator lifecycle/fault/import/migration tests pass, including the
  2048-to-Lab transition, and the 29-process isolated scene matrix passes
- ARMv5 firmware build and the 324-entry release ZIP pass; the ZIP contains
  only the new reference-package versions
- the connected Target 71 FAT32 volume was backed up, checked before and after
  the write, updated by merge plus an atomic firmware replacement, read back
  byte-for-byte and safely ejected
- physical postboot responsiveness remains unverified until the device boots
  and the exact 2048 → exit → Capability Lab sequence is repeated

## 2026-07-31 (Mini App ABI 3 production candidate)

Goal: replace the native/MicroQuickJS Mini App experiments with the locked
ABI 3 design and exercise the full supported capability surface.

Changed:

- replaced ABI 1/2 and CPK2/3 with CPK4, QuickJS `2025-09-13`, compiled Solid
  JSX/CSS, generation handles and host-owned LVGL 9.5 objects
- added the complete UI, animation, Canvas, Tilemap, storage, files, player,
  audio, device, alarm and notification SDK surface
- added desktop PNG/GIF/Lottie/Tone conversion, strict TypeScript testing,
  2048 and Capability Lab
- removed the old crypto/signing key, native reference apps and stale
  Calculator/Pomodoro localization path
- documented the exact reliability boundary and kept SQLite, Chinese IME,
  networking, browser DOM and 3D as explicit exclusions

Verified:

- 26/26 builder tests, Mini App/QuickJS/CPK4 C host tests, UI/EPUB/font host
  suites, strict localization and architecture gates pass; the QuickJS test
  executes the freestanding `sin`, `pow` and `hypot` path
- simulator and ARMv5 builds pass; the release ZIP contains 324 entries and
  passes `unzip -tq`
- 2048 and all Capability Lab simulator actions execute; the alarm produces a
  Host notification and the export payload matches byte-for-byte
- native simulator fault tests reject stale handles, duplicate deletion,
  parent cycles and stale parents; execution failure tears down the scene
- ten Capability Lab open/Game/close cycles leave no timer, rAF, scene or
  external Host memory; QuickJS version, source CRC, truncation and bytecode
  corruption each force a successful cache rebuild
- the simulator creates a user fixture, selects it through the native picker,
  and asserts that the Capability Lab JavaScript import reads all 24 bytes
- simulator-native fault injection proves the 500 ms watchdog, real QuickJS
  OOM teardown and active FX/PCM panic cleanup
- the real QuickJS 2048 startup migrates the previous ABI 2 CP24 save into a
  CRC-validated ABI 3 JSON state without losing board, score or best score
- Capability Lab exercises all supported input callbacks and runs a two-layer,
  32-sprite Tilemap workload
- Capability Lab 3.0.1 replaces the four grouped pages with a scrollable,
  numbered 14-entry directory; UI controls, navigation/overlays, data/images,
  Canvas, animation, Tilemap and every Host capability now have independent
  pages
- an automated 17-process simulator matrix validates the directory start and
  end, every reference page, RGB565 screenshot structure, Host PASS logs,
  exact export bytes and the visible alarm notification badge
- DeviceInfo now reports bounded QuickJS usage/capacity, Host external buffers,
  the 12 MiB session budget and Rockbox free memory; Capability Lab validates
  and logs those values
- added a target-validated device certification tool for read-only preflight,
  opt-in atomic CPK staging and evidence collection without copying private
  application data
- validated a mounted iPod Classic 6G/Target 71, created a recoverable local
  backup, deployed the exact firmware, 2048 and Capability Lab artifacts,
  verified each deployed file byte-for-byte, collected preboot evidence and
  safely ejected the device
- moved the two incompatible ABI 2 CPKs out of the device install inbox into
  the local recovery backup; a read-only FAT32 check found nine orphan
  clusters and no repair was attempted
- the first ABI 3 firmware deployment triggered Rockbox's native `Stkov`
  guard before useful postboot evidence could be collected; inspection found
  that the 256 KiB QuickJS C-stack allowance and ABI 3 UI still ran on the
  iPod's 8 KiB main thread stack
- moved the complete native CrazyPod UI, LVGL and Mini App runtime onto a
  dedicated 384 KiB Rockbox thread while retaining the 256 KiB QuickJS limit
  and at least 128 KiB for surrounding Host/LVGL calls; an architecture gate
  now prevents those reviewed limits and the dedicated thread from drifting
- rebuilt ARM and simulator targets, confirmed the ARM stack symbol is exactly
  `0x60000` bytes, passed Mini App host and simulator fault tests, rebuilt the
  324-entry release archive, backed up the failed binary, deployed the fixed
  firmware byte-for-byte and safely ejected the iPod
- inspected the postboot `package error -12` directly on the device: both CPKs
  were fully published and had valid `app.qbc` caches, while `game2048` logged
  `InternalError: interrupted` from selector matching during initial render
- separated the initial module/Solid/CSS bootstrap watchdog (5 s) from normal
  callback execution (500 ms), batched style invalidation at the end of the JS
  task, and cached parsed class tokens instead of rebuilding them for every
  selector check
- rebuilt both reference packages and ARM/simulator binaries, passed all 25
  builder tests, Mini App C host tests, architecture gate and native simulator
  fault/import/migration tests, then staged the new firmware and both packages
  byte-for-byte on the validated Target 71 device

Not yet verified:

- successful first boot after the native stack-isolation fix, CPK4 publication
  and postboot logs on the deployed iPod
- the physical performance, 100-cycle lifecycle, one-hour audio, eight-hour
  graphics and real power-cut matrices remain `NOT RUN`

## 2026-07-30 (Nine-language runtime and interaction polish)

Goal: reconcile the CrazyPod tasks completed after the 01:25 documentation
sync with the current source, artifacts, and device-copy records.

Changed:

- implemented English, Simplified Chinese, Traditional Chinese, Japanese,
  Korean, German, French, Spanish, and Brazilian Portuguese runtime
  localization
- added a persistent Settings language selector, upgraded CrazyPod state from
  version 9 to 10, and defaulted version 1–9 migrations to English
- generated 856 firmware translation entries and 27 Calculator/Pomodoro
  entries; added strict missing-key, placeholder, marker, and bare-string gates
- regenerated the 8, 10, 12, 14, and 16px font subsets after adding native
  language names to the manifest; this fixed `简体中文` rendering as `体中文`
- added UTF-8 decoding to the non-LVGL LCD text path
- replaced wheel-distance unlock with a 0.5-second Select hold, early-release
  cancellation, an opening-shackle animation, and residual-input protection
- added a temporary left-edge Now Playing volume HUD and normalized the optical
  size and centering of playback, favorite, shuffle, repeat, lyrics, progress,
  and queue icons
- unified menu icon/text vertical centering and removed fixed text clipping
  from search and choice overlays
- synchronized product, build, contribution, architecture, Mini App,
  localization-font, status, release, and work-log documentation

Verified:

- strict localization audit: 0 errors and 0 warnings
- all five generated font sizes cover the complete 1,344-character manifest
- simulator, UI host, Mini App, EPUB, architecture, ARM, stack-alignment, and
  packaging checks passed in the recorded implementation tasks
- `CrazyPod-6G.zip` contains 324 entries and passes `unzip -tq`
- latest installed `rockbox.ipod` SHA-256:
  `6e087dc7202a8fce085c7a78be45ad01ece66b33bc4710bc20f06b54f6f62b9d`
- the device copy retained music, `iPod_Control`, and `.crazypod`, passed the
  FAT32 check, and was safely ejected without changing the bootloader

Still open:

- review and commit the broad localization and UI working tree in coherent
  changes
- run a complete physical nine-language, unlock-hold, volume-HUD, and icon
  alignment regression matrix
- complete diagnosis of intermittent white block flicker seen in both CrazyPod
  and the stock firmware; no hardware root cause is confirmed
- implement the Mikey headset-remote driver and CrazyPod remote input mapping
- resolve branch cleanup around `origin/dev`, `stablerelease1`, and the three
  `release`-only commits

## 2026-07-30 (Architecture, interaction, music, and documentation sync)

Goal: record the implemented work from 2026-07-29 through 2026-07-30 and align
all CrazyPod-owned documentation with the current source.

Changed:

- split `crazypod_ui.c` from 18,456 lines into vertical features, Shell,
  Navigation, Presentation, Platform, and domain infrastructure; the current
  composition root is 800 lines and passes the strengthened architecture gate
- optimized Home click-wheel motion, renderer hot paths, and all six waveform
  styles; added small-movement filtering
- replaced the artwork pipeline with resumable CV8 unique-album caching and
  source-signature invalidation
- added artwork-derived waveform palettes, persistent `My Favorites`, queue
  auto-close, Now Playing progress seek, direct wheel volume, and embedded
  `Sharp_Pop` track-skip audio
- added Latin/CJK collation, pinyin A-Z grouping, fast-wheel section jumps,
  letter HUD, and shared marquee behavior
- fixed Settings/More short-list corruption, lock/wake ordering, Home MENU hold,
  PLAY hold, USB Data input blocking, and centered empty states
- detached `Gigass/CrazyPod` from the upstream fork network and disabled pushes
  to `upstream`
- synchronized README, build, contribution, architecture, Mini App, status,
  release, and work-log documentation

Verified:

- current simulator build and all existing host tests pass
- architecture gate passes with an 800-line composition root
- current marquee fix passes pixel-level short/long text checks, ARM packaging,
  301-file device verification, and safe eject
- latest recorded installed firmware hash is
  `1b939f9266c89aac4d13635a3da5755189d59e7a9d6b956328fd655fb3c0440b`

Still open:

- commit the two-file marquee overflow fix and record post-boot interaction
- implement nine-language localization; the current firmware remains
  English-only
- implement the Mikey headset-remote driver and CrazyPod remote input mapping
- normalize `main`, `dev`, and `stable` only after deciding whether to preserve
  the three commits unique to `release`
- complete the release-grade physical iPod regression matrix

## 2026-07-27 (Project documentation synchronization)

Goal: align public project documentation with the current source tree, the
current Codex task window, generated artifacts, and recorded device tests.

Changed:

- documented the implemented Media video route, CP936 book decoding, optional
  bootloader, Play-hold power menu, 19-step lock screen, and Mini App SDK
  revision 2
- separated current artifact validation from earlier physical firmware copies
- recorded Mini App revision 3 as a proposal rather than an implemented API
- corrected Cover Flow's release projection from 120 ms to 200 ms
- corrected the current hardware archive count from 322 to 324 ZIP entries
- updated simulator route names and the Mini App tutorial's ABI-prefix check

Verified:

- all public Markdown links resolve to files in the repository
- the simulator and iPod 6G incremental builds pass
- `CrazyPod-6G.zip` passes `unzip -tq`
- documented artifact sizes and SHA-256 values match the generated files

Limits:

- the current Codex index exposes 27 CrazyPod tasks and has no cursor for
  tasks that have fallen outside the 50-task window
- the current SDK revision 2 artifact has not been installed on the device
- no commit, push, or release was performed by this documentation pass

## 2026-07-27 (Mini App SDK revision 2)

Goal: expand the native Mini App SDK without invalidating existing ABI 1
packages.

Changed:

- changed Loader host-table validation from exact size equality to a bounded
  stable-prefix check; the original ABI 1 host table remains 44 bytes on ARM
- added capability-gated system information, duration/date/time formatting,
  four persistent alarm slots, host-rendered Toast, and asynchronous app-close
  requests
- added semantic Divider and Progress drawing commands without changing the
  fixed ABI 1 draw-command layout
- upgraded Calculator and Pomodoro to 1.1.0; Calculator exercises Divider,
  while Pomodoro exercises host duration formatting, Progress, and Toast
- changed package filenames to derive from manifest versions and removed stale
  reference-package versions during normal builds
- updated the Mini App reference and Chinese tutorial

Verified:

- SDK prefix, Calculator, Pomodoro, input-pacing, and crypto host tests pass
  with strict warnings and under ASan/UBSan
- ARM layout check confirms the ABI 1 prefix is 44 bytes and the revision 2
  host table is 84 bytes
- simulator and iPod 6G incremental builds pass
- both 1.1.0 CPK files contain exactly four stored entries and have no
  unresolved native symbols
- the generated hardware ZIP passes `unzip -tq`

Not yet verified:

- physical iPod interaction and multi-alarm delivery under simultaneous
  deadlines
- text input, confirmation dialogs, package resources, and mediated file
  selection remain future SDK work

## 2026-07-27 (Silent Apple boot surface and CrazyPod handoff)

Goal: replace the normal iPod 6G boot console with one continuous black boot
surface, a centered white Apple mark, and a short transition into the CrazyPod
desktop.

Changed:

- normal bootloader progress output no longer updates the LCD; USB, low-power,
  fatal, and recovery paths still expose diagnostics
- fatal and USB paths clear the hidden normal-boot framebuffer content before
  presenting text
- added a compact 33x40 antialiased boot mark shared by the bootloader and
  CrazyPod, avoiding the approximately 60 KiB cost of compiling the surrounding
  black pixels into the constrained bootloader move area
- CrazyPod redraws the same mark immediately after LCD initialization and
  keeps it visible through platform and LVGL initialization
- the desktop enters with a 220 ms LVGL fade from the matching boot screen
- restored the standard LCD/font path for iPod 6G bootloader builds while
  keeping the CrazyPod framebuffer and panic UI firmware-only
- added a reproducible `build-bootloader.sh`; it builds but never flashes the
  device

Verified:

- clean iPod 6G bootloader build passes and produces
  `build-bootloader-ipod6g/bootloader-ipod6g.ipod`
- incremental hardware build passes and produces `rockbox.ipod` and the
  validated `CrazyPod-6G.zip`
- simulator main binary links and runs through the startup handoff; a
  deterministic post-start screenshot renders the desktop normally
- full simulator packaging still stops at the pre-existing KSS codec link
  failure; the simulator main program is unaffected

Deployment note:

- installing `CrazyPod-6G.zip` updates only the firmware-side handoff
- removing the power-on boot console requires separately installing the built
  bootloader with the established iPod 6G recovery procedure

## 2026-07-27 (Mini Apps reference build and iPod 6G runtime fix)

Goal: replace duplicated placeholder Mini App content with two complete
reference packages and validate the shared click-wheel runtime on hardware.

Changed:

- added signed Calculator and Pomodoro ABI 1 packages
- made Calculator a standard four-function calculator with clear/all-clear,
  contextual percent, repeat equals, decimal, sign, backspace, and error
  recovery
- made Pomodoro configurable and persistent, with focus/short-break/long-break
  phases, rounds, pause/resume, skip, reset, background deadlines, and alarms
- defined one-adjacent-control wheel navigation for discrete focus while
  retaining acceleration for Pomodoro numeric editing
- added an eight-event host input queue after physical retesting showed that
  multiple correct focus changes could still occur before one screen refresh;
  the queue now consumes one discrete event per presentable frame, bounds
  after-scroll, and switches direction without replaying stale input
- aligned iPod 6G main, IRQ, and FIQ stack boundaries to 8 bytes and added a
  build-time symbol gate, fixing invalid ARM EABI `double` formatting that
  appeared as `2.371515` on Calculator startup
- documented the `.cpk` format, install lifecycle, native-code security
  boundary, and development signing-key limitation

Verified:

- Calculator, Pomodoro, and crypto host tests pass normally and under
  ASan/UBSan
- host input-pacing tests pass normally and under ASan/UBSan, including burst
  input, frame gating, bounded backlog, and direction reversal
- simulator and iPod 6G incremental builds pass
- instruction-level execution of the real ARM Calculator payload renders `0`
  initially and after AC
- the hardware ZIP contains 300 files and passes archive validation
- all 300 files from the prior zero/step build matched after device
  installation
- the prior post-write FAT32 filesystem check exited 0 and the device was
  safely ejected
- the current frame-paced build contains 300 files and passes archive and stack
  alignment checks; it has not yet been installed

Still required:

- install the current frame-paced build and confirm Calculator startup, AC,
  fast-wheel visible focus order, reversal, and Select behavior on the physical
  click wheel
- complete foreground/background Pomodoro and alarm coexistence testing on
  physical hardware

## 2026-04-13 (README release sync: Apple2026 Beta Update 2 feature surface)

Goal: bring `README.md` back in line with the live branch state before the next
release so the public-facing feature summary stops lagging behind the actual
repo.

Changed:
- `README.md`
  - refreshed the opening branch summary around the current Apple2026 beta
    state instead of older generic branch wording
  - added a dedicated `What's New in Beta Update 2` section grounded in the
    current release-facing changes
  - added an `Apple2026 UI` feature section covering:
    - fixed Quick Settings mapping/surface
    - WPS short-`Select` -> Lyrics / LRC default behavior
    - refreshed WPS / mini-player presentation
    - Apple2026 theme/version staging
  - updated the Cover Flow section to reflect:
    - full-screen by default
    - optional status bar instead of status-bar-first wording
    - current WPS return/tracklist re-entry behavior
  - replaced the old minimal menu/rendering sections with current
    navigation/release-hardening summaries
  - updated installation text to match the current beta release guidance:
    - back up `.rockbox`
    - replace the folder cleanly from the release zip
    - re-check Battery/Disk settings on modded iPods
  - tightened `Supported Models`, `At a Glance`, and `Known Limitations` so
    they better match the current branch reality

Verified against live repo:
- Quick Settings defaults/contract:
  - `apps/gui/quickscreen.c`
  - `wps/Apple2026.sbs`
  - `wps/WPSLIST`
- WPS short-`Select` / Lyrics hotkey flow:
  - `apps/keymaps/keymap-ipod.c`
  - `apps/settings_list.c`
  - `apps/onplay.c`
  - `apps/gui/wps.c`
  - `apps/plugins/lrcplayer.c`
- Cover Flow current behavior:
  - `apps/plugins/pictureflow/pictureflow.c`
- Apple2026 release staging / self-heal:
  - `apps/main.c`
  - `build-sim.sh`
  - `build-hw.sh`
  - `tools/apple2026_skin_audit.py`

## 2026-04-13 (Apple2026 WPS return + quick settings exit hardening)

Exact issues:
- returning from WPS to a file-browser tracklist could drop back into the wrong
  list style instead of the active Apple2026 Music surface
- the returned list could show filename number prefixes and stale list geometry
  after a WPS round-trip
- Apple2026 quick settings could sometimes feel trapped because dismiss relied
  too heavily on release timing

Changed:
- `apps/tree.c`
  - `ACTION_TREE_WPS` now restamps playback context on Play-to-WPS entry when
    audio is active, so WPS short `Menu` returns to the current browser surface
    instead of an older playback origin
  - Apple2026 playback-context stamping now prefers
    `BROWSE_APPLE2026_MUSICLIB` over a raw `/Music` path guess, so return-to-list
    preserves the curated Music browser semantics instead of falling back to a
    generic filesystem list
  - list setup now refreshes viewport/item-height layout after the Apple2026
    font tier is chosen, which targets stale row geometry after WPS return
- `apps/gui/list.c`
  - added `gui_synclist_refresh_layout(...)` so list surfaces can explicitly
    rebuild viewport and item-height geometry after dynamic styling changes
- `apps/gui/list.h`
  - exported the new `gui_synclist_refresh_layout(...)` helper
- `apps/keymaps/keymap-ipod.c`
  - Apple2026 quick settings top/bottom actions now fire on short
    `Menu`/`Play` release instead of the initial press
  - held `Menu` now remains a clean cancel/dismiss path
  - held `Play` now dismisses quick settings as the "second entry-button press"
    path, matching the Apple2026 quick settings plan
  - remote quick settings map was updated to the same contract
- `apps/gui/quickscreen.c`
  - `ACTION_STD_OK` now exits quick settings immediately, so short `Select`
    dismisses cleanly without depending on release timing
- `DESIGN_SYSTEM.md`
  - documented the Apple2026 quick settings exit contract
  - documented that Play-to-WPS from a live browser re-stamps current browse
    context for WPS short `Menu`

Verified:
- `.\build-sim.ps1 -Incremental` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `python tools\verify_plugin_package.py --build-dir build-sim --package-root build-sim\simdisk\.rockbox` -> `OK`

Manual verification still required:
- Files or Music -> WPS -> short `Menu` should return to the same styled track
  list instead of a generic numbered filesystem list
- scrolling/selecting after that return should no longer flash white header /
  mini-player chrome
- Apple2026 quick settings should always dismiss via short `Select`, held
  `Menu`, or held `Play`

## 2026-04-13 (Apple2026 mini-player shadow pass: raised shadow opacity to 35%)

Exact request:
- raise the Apple2026 mini-player drop shadow opacity from the prior `13.5%`
  pass to `35%`

Changed:
- `tools/apple2026_wps_art_frame.py`
  - changed `PILL_SHADOW_OPACITY` from `0.135` to `0.35`
  - kept `PILL_SHADOW_DY = 2` and `PILL_SHADOW_BLUR = 2.8` unchanged
- `DESIGN_SYSTEM.md`
  - updated the locked mini-player shadow token to `35% black, dy=2, blur=2.8`

Expected result:
- same mini-player geometry
- same shell/body color
- much stronger drop shadow under the mini-player

## 2026-04-13 (Apple2026 WPS hero shadow retune: 35% opacity)

Exact request:
- raise the WPS hero-art shadow opacity to `35%`

Changed:
- `tools/apple2026_wps_art_frame.py`
  - increased `WPS_SHADOW_OPACITY` from `0.18` to `0.35`
  - left `WPS_SHADOW_DY = 2` and `WPS_SHADOW_BLUR = 3.2` unchanged
- `wps/Apple2026/wpsBackdrop.bmp`
  - regenerated with the stronger hero-art shadow density
- `wps/Apple2026/wpsArtCorners.bmp`
  - regenerated from the updated backdrop crop so the corner cutout matches the
    stronger shadowed perimeter exactly
- `DESIGN_SYSTEM.md`
  - locked `wps_art_shadow = 35% black, dy=2, blur=3.2`
- `WPS_REGRESSION_AUDIT.md`
  - recorded the `18% -> 35%` shadow retune on top of the fixed cutout path

Expected result:
- same rounded-perimeter compositing behavior as the previous fix
- materially darker hero shadow
- unchanged blur/spread profile
- stronger depth without changing geometry

## 2026-04-13 (Apple2026 WPS album-art corner compositing fix + darker rounded-perimeter shadow)

Exact request:
- fix the hero album-art corners so they stop reading as hard white caps over
  the shadow
- preserve a true rounded-object feel where the shadow wraps the curved edge
- darken the WPS hero shadow by about 50% without making it heavier or blurrier

Exact root cause:
- `wps/Apple2026/wpsArtCorners.bmp` was still a flat white corner matte:
  - magenta only inside the rounded silhouette
  - pure shell-white in the opaque corner pixels
- `tools/apple2026_wps_art_frame.py` also cleared the full `150x150` hero-art
  box in `wpsBackdrop.bmp` back to shell white after shadow compositing
- that meant there was no shadow information left in the square corner pockets
  for the trim to reveal, so the final edge could only read as white corner
  blocks sitting on top of the shadow

Changed:
- `tools/apple2026_wps_art_frame.py`
  - increased `WPS_SHADOW_OPACITY` from `0.12` to `0.18`
  - added `_clear_wps_rounded_interior()` so only the rounded silhouette
    interior is flattened back to the shell tone
  - changed `generate_corners()` to crop the already-shadowed hero backdrop and
    punch `FF00FF` only through the rounded silhouette instead of filling the
    corner pixels with flat white
- `wps/Apple2026/wpsBackdrop.bmp`
  - regenerated so the rounded silhouette interior stays flat shell white while
    the square corner pockets retain the baked shadow
- `wps/Apple2026/wpsArtCorners.bmp`
  - regenerated as a backdrop-matched cutout overlay; opaque corner pixels now
    carry the real shadowed-shell pixels instead of white fill
- `wps/Apple2026.wps`
  - updated the hero-art viewport comments to document the backdrop-matched
    cutout behavior
- `tools/apple2026_skin_audit.py`
  - made `wpsBackdrop.bmp` and `wpsArtCorners.bmp` required Apple2026 assets
  - added hero-art semantic checks for:
    - no flat shell-white trim corner pixels
    - transparent rounded interior
    - preserved shadowed corner pocket in `wpsBackdrop.bmp`
    - flat shell-tone rounded interior in `wpsBackdrop.bmp`
- `DESIGN_SYSTEM.md`
  - locked the new rule that `wpsArtCorners.bmp` is a backdrop-matched cutout,
    not a flat white trim
- `WPS_REGRESSION_AUDIT.md`
  - added the white-corner-cap root cause and the chosen fix path

Verified:
- source pixel audit:
  - `wpsArtCorners.bmp` opaque top-left corner now samples grayscale shadow
    pixels (`250,250,250`, `248,248,248`, `247,247,247`, `245,245,245`) instead
    of flat white
  - `wpsBackdrop.bmp` now retains darker corner-pocket pixels at the hero art
    origin (`85,8 = 250,250,250`) while keeping interior rounded-area pixels
    flat white (`95,15 = 255,255,255`)
- `python tools/apple2026_wps_art_frame.py`
- `python tools/apple2026_skin_audit.py --source-only` -> OK
- `.\build-sim.ps1 -Incremental` -> OK
- `python tools/apple2026_skin_audit.py` -> OK
- `.\tools\verify-sim-install.ps1` -> OK
- simulator smoke launch (`build-sim\rockboxui.exe`, 5 seconds) -> `SIM_OK`
- `.\build-hw.ps1 -Target ipod6g -Incremental` -> OK
- `python tools/apple2026_skin_audit.py --zip-artifact build-hw-ipod6g/rockbox.zip` -> OK
- `python tools/verify_plugin_package.py --build-dir build-hw-ipod6g --zip-artifact build-hw-ipod6g/rockbox.zip` -> OK
  - existing warning only: `h264_poc.rock` not mapped by `viewers.config` / `CATEGORIES`

What to verify first in the next build:
- the hero corners should now read as a true rounded cutout with no hard white
  caps at the top corners
- the shadow should remain soft but visibly richer than the previous 12% pass
- non-square album art padding should still reveal a flat shell interior inside
  the rounded silhouette, not shadow residue
- no-art placeholder should still feel clean because the same trim now rounds it
  against the backdrop-matched cutout path

## 2026-04-13 (Apple2026 mini-player shadow pass: 50% darker drop shadow)

Exact request:
- Darken the Apple2026 mini-player drop shadow by roughly 50% without changing
  the bar geometry or the rest of the shell.

Changed:
- `tools/apple2026_wps_art_frame.py`
  - increased `PILL_SHADOW_OPACITY` from `0.09` to `0.135`
  - left `PILL_SHADOW_DY = 2` and `PILL_SHADOW_BLUR = 2.8` unchanged
- `DESIGN_SYSTEM.md`
  - updated the locked mini-player shadow token to `13.5% black, dy=2, blur=2.8`

Expected result:
- Same rounded-rectangle mini-player shape
- Same shell/body color
- Noticeably stronger drop shadow under the mini-player, approximately 50%
  darker than the previous pass

## 2026-04-13 (Apple2026 WPS playback-state icon refinement: larger status glyphs + Interpod lossless badge)

Exact current layout problem:
- the WPS right-side status column was structurally cleaner than before, but the
  actual shuffle and repeat glyphs were still too small for a 320x240 WPS
- the repeat lane still treated `REPEAT_SHUFFLE` as a blank repeat state and
  relied on a separate shuffle fallback lane, so combined-state ownership was
  split instead of explicit
- the existing Apple2026 `losslessIndicator.bmp` had drifted into a weak
  waveform strip that did not read clearly as a premium codec/lossless badge
- the bottom-right transport icon also needed more visual weight now that it
  owns a dedicated playback-state slot

Exact root cause:
- Apple2026 WPS was still using tiny legacy-status assets (`shuffle.bmp`,
  `repeat.bmp`, `playerStatus.bmp`) designed around a lighter-weight layout
- repeat/shuffle logic was spread across two different branches:
  - shuffle lane: `%ps` plus a special repeat-shuffle fallback
  - repeat lane: blanked the repeat-shuffle branch completely
- that split meant repeat-shuffle had no single authoritative representation and
  could read as incomplete even when a small shuffle glyph still existed
- `losslessIndicator.bmp` had been regenerated from a generic waveform symbol
  instead of preserving the stronger Interpod-derived `Lossless` badge

Changed:
- `wps/Apple2026.wps`
  - loaded slot 6: `13-SFCompactText-Regular.fnt` for compact repeat-state text
  - swapped WPS to dedicated larger assets:
    - `playerStatusLarge.bmp`
    - `repeatLarge.bmp`
    - `shuffleLarge.bmp`
  - enlarged and repositioned the right-side status column:
    - shuffle icon: `%Vl(shuffle_state,281,103,20,15,-)`
    - repeat icon: `%Vl(repeat_icon_state,281,124,20,15,-)`
    - repeat label: `%Vl(repeat_label_state,264,140,54,15,6)`
    - transport: `%Vl(player_status_lane,280,178,20,20,-)`
  - moved the lossless badge into the bottom pad:
    - `%Vl(lossless_ind,127,-12,66,11,-)`
  - fixed combined-state logic:
    - shuffle lane now lights for `%ps == s` or `%mm == REPEAT_SHUFFLE`
    - repeat icon lane stays populated for every repeat-enabled mode
    - repeat label maps `REPEAT_SHUFFLE` to `All` so repeat+shuffle always
      presents both pieces of state instead of falling partly blank
- `wps/Apple2026.sbs`
  - loaded the larger `playerStatusLarge.bmp` strip for the mini-player
  - enlarged the mini-player transport slot to `%V(280,203,20,20,-)`
  - fixed the paused-state mapping so the mini-player now shows pause bars
    instead of always falling back to the play triangle
- `tools/apple2026_symbol_assets.py`
  - fixed the canonical small `playerStatus.bmp` tile order to match the live
    WPS/SBS mapping: stop / pause / play / forward
  - added `playerStatusLarge.bmp` (20x20 per frame) for WPS-only transport
  - added `repeatLarge.bmp` and `shuffleLarge.bmp` (20x15) for the larger
    right-column state icons
  - changed `losslessIndicator.bmp` generation to copy the imported Interpod
    badge instead of regenerating the old waveform strip
- `tools/apple2026_skin_audit.py`
  - now requires slot 6, the larger WPS icon assets, and the new geometry
  - now requires the larger SBS mini-player transport strip and 20x20 transport
    slot
  - now forbids the old small WPS transport/repeat/shuffle preload path
  - now forbids the old SBS play-only mini-player transport mapping
  - now forbids the old split shuffle fallback and combined repeat icon/text
    lane
- `tools/verify-sim-install.ps1`
  - now verifies the compact status font, larger WPS icon assets, and the
    Interpod-derived lossless badge

Verified:
- `playerStatusLarge.bmp` generated as `20x80`
- `repeatLarge.bmp` generated as `20x15`
- `shuffleLarge.bmp` generated as `20x15`
- `losslessIndicator.bmp` now visually matches the imported Interpod `Lossless`
  badge instead of the old waveform strip
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -Incremental` -> `OK`
- `python tools/apple2026_skin_audit.py` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `build-sim\rockboxui.exe` launched and stayed alive for 5 seconds -> `SIM_OK`

Manual verification still required after the next build:
- confirm the larger shuffle and repeat icons read clearly at runtime
- confirm repeat text sits under the repeat icon without touching the title lane
- confirm repeat + shuffle now shows a complete combined state
- confirm the bottom-pad Interpod lossless badge feels secondary, not headline
- confirm the larger transport glyph still feels calm against progress/time
- confirm the mini-player paused state now shows pause bars at the larger scale

## 2026-04-13 (Apple2026 WPS volume icon cleanup: matched speaker pair, no warning stripe)

Exact issue from the current screenshot:
- the WPS left volume speaker was rendering far too small and sat lower than the
  right speaker
- the right speaker was also picking up a vertical red accent bar that read like
  a rendering defect rather than a deliberate state

Exact root cause:
- `speaker_mute.bmp` had been regenerated as an `8x13` glyph while
  `speaker_loud.bmp` stayed at `19x17`
- the WPS volume lane still placed the mute icon at `(46,5)`, which amplified
  the size mismatch and vertical misalignment
- the lane also still drew `speaker_too_loud.bmp` through `%xd(D)` whenever
  `%pv > 0`, so the red stripe was effectively always on during normal volume
  changes

Changed:
- `wps/Apple2026.wps`
  - removed the deprecated `speaker_too_loud.bmp` preload from the WPS volume
    row
  - removed the `%xd(D)` warning-stripe overlay from the volume row
  - moved the mute icon draw to `%x(M,speaker_mute.bmp,35,1)` so it aligns with
    the right speaker and the rail
- `tools/apple2026_symbol_assets.py`
  - regenerated `speaker_mute.bmp` as a matched `19x17` glyph instead of the
    tiny `8x13` tile
- `tools/apple2026_skin_audit.py`
  - now requires the corrected mute icon placement
  - now forbids the old tiny mute layout and the deprecated red warning-stripe
    overlay path
- `DESIGN_SYSTEM.md`
  - locked the WPS volume row speaker-pair rule and the no-warning-stripe rule

Verified:
- `speaker_mute.bmp` regenerated to `19x17`
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`
- `python tools/apple2026_skin_audit.py` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `build-sim\rockboxui.exe` launched and stayed alive for 5 seconds -> `SIM_OK`

## 2026-04-13 (Apple2026 WPS layout rebalance: right-side status lane + metadata safe area)

Exact current layout issue:
- WPS right-edge ownership had drifted into a collision-prone state:
  - the text-only shuffle lane occupied the same vertical band as the song title
  - repeat/shuffle state did not read as a coherent playback-status family
  - the player-status icon still lived in the top-left chrome instead of the
    playback-information zone
  - title and artist/album lines had no hard stop before the right status area

Reference conclusion:
- Interpod is the better behavioral reference for compact playback-state
  ownership.
- Themeify is the better structural reference for explicit status modules and
  bounded metadata width.
- Apple2026 should copy those ownership patterns, not Themeify's heavier visual
  framing.

Changed:
- `wps/Apple2026.wps`
  - added a dedicated `shuffle.bmp` preload (`S`)
  - moved playback-state ownership into a three-slot right column:
    - shuffle icon at `x=284 y=112`
    - repeat label lane at `x=242 y=142`
    - player status at `x=284 y=181`
  - removed the old top-left player-status ownership
  - removed the old text-only `SHUFFLE` lane
  - removed the old repeat `SHU` branch
  - locked title and artist/album safe-area widths to `258px`
- `tools/apple2026_symbol_assets.py`
  - now generates `wps/Apple2026/shuffle.bmp`
- `tools/apple2026_skin_audit.py`
  - now requires the dedicated WPS shuffle asset
  - now locks the new title/artist safe-area widths
  - now locks the new shuffle/repeat/player-status lane geometry
  - now forbids the old top-left status icon layout and text-only shuffle lane
- `tools/verify-sim-install.ps1`
  - now verifies `wps/Apple2026/shuffle.bmp`
- `WPS_REGRESSION_AUDIT.md`
  - expanded with a detailed layout-rebalance audit addendum, including
    geometry evidence, hypotheses, and the chosen right-lane contract
- `DESIGN_SYSTEM.md`
  - locked the WPS metadata safe-area and right-lane playback-state contract

Verified:
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`, but did not stage the
  new WPS payload on its first pass
- `.\build-sim.ps1 -Incremental` -> `OK`
- `python tools/apple2026_skin_audit.py` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `build-sim\rockboxui.exe` launched and stayed alive for 5 seconds -> `SIM_OK`

Manual verification still required after the next build:
- confirm long title/artist strings stop cleanly before the right lane
- confirm shuffle/repeat never overlap
- confirm the bottom-right player-status icon feels intentional and does not
  collide with time/progress
- confirm the right lane stays calm when inactive
## 2026-04-13 (Apple2026 quick settings second recovery pass: centered wheel + horizontal utility bars)

Exact current clue:
- The first Themeify-style recovery improved behavior, but the quickscreen still
  felt too split:
  - the right-side vertical dock kept the surface looking separate from Apple2026 WPS
  - the header was still taller than necessary
  - bottom brightness text remained too literal
  - the wheel still carried too much edge definition

User-directed layout correction:
- remove the outer clickwheel stroke
- keep only the top `Brightness` label
- use `sun.max.fill` at the top and `sun.min.fill` at the bottom
- center the wheel and its option states
- move BRT and VOL underneath as thin stacked horizontal bars
- align volume value to the right side of the volume bar

Polish follow-up:
- use `sun.min` instead of `sun.min.fill` for the lower brightness cue
- remove the lower brightness number completely
- widen the BRT / VOL bars and round their ends so they read closer to the WPS rail family

Changed:
- `tools/apple2026_quickscreen_assets.py`
  - removed the outer wheel stroke
  - added generated quickscreen assets:
    - `qs_bar_fill.bmp`
    - `qs_bar_track.bmp`
    - `qs_sun_max.bmp`
    - `qs_sun_min.bmp`
  - now generates `qs_sun_min.bmp` from `sun.min`
  - widened `qs_bar_fill.bmp` / `qs_bar_track.bmp` and converted them to rounded horizontal rails
- `wps/Apple2026.sbs`
  - reduced the quickscreen header height
  - centered the wheel at the middle of the surface
  - moved shuffle / repeat to balanced left/right positions around the wheel
  - kept only the top `Brightness` label
  - replaced the bottom label with the down-brightness sun symbol only
  - replaced the right-side vertical dock with stacked horizontal BRT / VOL bars
  - moved the volume value to the right side of the volume bar
  - widened the lower bar lane so the bottom section feels less cramped
  - widened the left/right quickscreen state lanes so longer values like `Shuffle` no longer clip on the right side
- `tools/apple2026_skin_audit.py`
  - updated quickscreen contract checks for the centered-wheel + horizontal-bar layout
  - added checks for the new bar and sun assets
  - now forbids the lower quickscreen numeric brightness readout and validates the wider rounded bar assets
- `tools/verify-sim-install.ps1`
  - updated required quickscreen asset checks for the new bar and sun assets
- `DESIGN_SYSTEM.md`
  - updated quickscreen visual rules to match the centered control stack

Verified:
- `python tools/apple2026_quickscreen_assets.py` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`
- `python tools/apple2026_skin_audit.py` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `build-sim\\rockboxui.exe` launched and stayed alive for 5 seconds
- `.\build-hw.ps1 -Target ipod6g -Incremental` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only --zip-artifact build-hw-ipod6g\\rockbox.zip` -> `OK`
- `build-hw-ipod6g\\rockbox.zip` contains:
  - `qs_wheel.bmp`
  - `qs_bar_fill.bmp`
  - `qs_bar_track.bmp`
  - `qs_sun_max.bmp`
  - `qs_sun_min.bmp`
- Remaining manual check:
  - confirm the bottom cue reads clearly with icon only and no value
  - confirm the wider rounded bar family reduces the cramped feel on hardware and sim

## 2026-04-13 (Apple2026 simulator recovery: restored menu icon strip and unified staged shell background)

Exact current regression:
- Apple2026 menu icons had collapsed into tiny fragmented pink glyphs instead of
  the original red menu icons.
- The simulator shell was still vulnerable to a header/body tone split because
  the staged simulator/hardware configs were writing `background color:
  fafaf8` even after the neutral-white reset.

Exact root cause:
- `tools/apple2026_symbol_assets.py` had overwritten the checked-in
  `icons/Apple2026Icons.bmp` menu strip with a generated `15x512` strip.
- The real Apple2026 menu strip is the checked-in `30x1020` asset.
- `build-sim.sh` and `build-hw.sh` were still hardcoding `fafaf8` into staged
  Apple2026 config output, so runtime staging could reintroduce the old warm
  shell even after source theme files had been reset to `ffffff`.

Changed:
- Restored `icons/Apple2026Icons.bmp` from the committed Apple2026 baseline and
  synced it into `build-sim\simdisk\.rockbox\icons\Apple2026Icons.bmp`.
- Updated `tools/apple2026_symbol_assets.py` so it no longer regenerates or
  owns the main Apple2026 menu icon strip.
- Updated `build-sim.sh` and `build-hw.sh` staged Apple2026 configs from
  `fafaf8` to `ffffff`.
- Synced simulator runtime config/theme files back to the corrected Apple2026
  config.
- Updated `tools/apple2026_skin_audit.py` to lock the restored icon-strip
  contract (`30x1020` Apple2026 icon strip).
- Updated `tools/verify-sim-install.ps1` to stop expecting obsolete quick
  settings assets that Apple2026 no longer ships.

Verified:
- Source icon strip: `icons/Apple2026Icons.bmp` = `30x1020`
- Runtime icon strip: `build-sim\simdisk\.rockbox\icons\Apple2026Icons.bmp` =
  `30x1020`
- Runtime red-icon sample check passed
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- Simulator still loads:
  - `A26 skin_load ... Apple2026.sbs loaded=1`
  - `A26 skin_load ... Apple2026.wps loaded=1`

## 2026-04-13 (Apple2026 quick settings regression recovery: Themeify layout rebase + payload hardening)

Exact current clue:
- The screenshot-driven recovery pass confirmed that the previous Apple2026
  quickscreen still read as a regression:
  - the wheel area was oversized and visually heavy
  - the stacked middle text column made the screen feel improvised
  - the surface no longer matched Themeify's cleaner grouping or Apple2026's
    calmer shell rhythm

What Themeify was doing better:
- Brightness was the primary focus above / below the wheel instead of being
  buried in a side stack.
- Shuffle and Repeat lived around the wheel as side states, not in a central
  dump of unrelated values.
- The right slider lane carried the utility-surface feel with minimal text.
- The wheel itself remained directional and legible without becoming the hero.

Changed:
- `wps/Apple2026.sbs`
  - removed the stacked middle quickscreen value list
  - kept the Themeify-style right slider dock
  - reworked the layout to use:
    - top brightness label + value
    - smaller left wheel
    - shuffle / repeat around the wheel
    - bottom brightness label + value
  - removed the textual `BRT +` / `BRT -` callouts
- `tools/apple2026_quickscreen_assets.py`
  - regenerated `qs_wheel.bmp` as a smaller `92x92` directional grey wheel
- `tools/apple2026_skin_audit.py`
  - locked the recovered quickscreen placement
  - locked `qs_wheel.bmp` to `92x92`
  - locked Apple2026 theme background expectation to `fafaf8`
- `wps/WPSLIST`
  - updated Apple2026 theme background token to `fafaf8`
- `build-sim.sh`
  - now explicitly stages Apple2026 `.sbs/.wps/.bmp` payloads into
    `simdisk/.rockbox`
  - now explicitly stages `themes/Apple2026.cfg`
- `build-hw.sh`
  - now explicitly stages:
    - `themes/Apple2026.cfg`
    - `wps/Apple2026.sbs`
    - `wps/Apple2026.wps`
    - `wps/Apple2026/*.bmp`
  - this prevents stale quickscreen payloads from surviving in `rockbox.zip`

Verified:
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `python tools/apple2026_skin_audit.py` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only --zip-artifact build-hw-ipod6g/rockbox.zip` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `build-sim\\rockboxui.exe` launched and stayed alive for 5 seconds
- simulator runtime `qs_wheel.bmp` -> `92x92`
- hardware zip `qs_wheel.bmp` -> `92x92`
- hardware zip staged `themes/Apple2026.cfg` now carries `background color: fafaf8`

## 2026-04-13 (Apple2026 full-system light-palette reset: stock neutral white baseline restored)

Exact root cause of the color drift:
- Apple2026 no longer had one trustworthy light-surface source of truth.
- The live shell family had drifted across multiple layers:
  - `wps/Apple2026.sbs`, `wps/Apple2026.wps`, and `wps/WPSLIST` were carrying
    the warmed `FAFAF8` experiment,
  - `tools/apple2026_wps_art_frame.py` was generating WPS and mini-player mattes
    against that same warm base,
  - other generator paths still owned neutral-white assets independently,
  - quick settings assets did not yet have a dedicated checked-in generator,
  - audit/docs still described the warmed split shell/body model as active.
- Result: header, list body, mini-player, WPS matte, quick settings, and utility
  assets could all be "close" whites while still failing to read as one system.

Reset target locked:
- Apple2026 light-shell baseline is now reset to stock neutral white:
  - `#FFFFFF`
  - palette source of truth: `tools/apple2026_palette.py`

Changed:
- Added `tools/apple2026_palette.py` as the shared generator/audit palette
  authority for Apple2026 light-shell surfaces.
- Reset live skin/theme files back to stock neutral white:
  - `wps/Apple2026.sbs`
  - `wps/Apple2026.wps`
  - `wps/WPSLIST`
- Split generator ownership so asset families stop overwriting each other:
  - `tools/apple2026_wps_art_frame.py`
    - owns `wpsBackdrop.bmp`, `wpsArtCorners.bmp`, `albumPlaceholder.bmp`,
      `miniplayer_bg.bmp`, and `art_mask.bmp`
  - `tools/apple2026_symbol_assets.py`
    - owns symbol/icon sheets plus `usbBackdrop.bmp`
  - `tools/apple2026_lockscreen_assets.py`
    - owns `Wallpaper.bmp` and lock notification assets
  - `tools/apple2026_wps_pill_bars.py`
    - owns `pb.bmp`, `pb_backdrop.bmp`, `pb_active.bmp`,
      `pb_active_backdrop.bmp`, `vb.bmp`, and `vb_backdrop.bmp`
  - `tools/apple2026_quickscreen_assets.py`
    - newly added owner for `qs_wheel.bmp`, `qs_slider_fill.bmp`,
      and `qs_slider_track.bmp`
- Regenerated the full neutral-shell asset family from the corrected base:
  - `wps/Apple2026/wpsBackdrop.bmp`
  - `wps/Apple2026/wpsArtCorners.bmp`
  - `wps/Apple2026/albumPlaceholder.bmp`
  - `wps/Apple2026/miniplayer_bg.bmp`
  - `wps/Apple2026/art_mask.bmp`
  - `wps/Apple2026/usbBackdrop.bmp`
  - `wps/Apple2026/Wallpaper.bmp`
  - `wps/Apple2026/LockNotification.bmp`
  - `wps/Apple2026/LockNotifPlay.bmp`
  - `wps/Apple2026/pb.bmp`
  - `wps/Apple2026/pb_backdrop.bmp`
  - `wps/Apple2026/pb_active.bmp`
  - `wps/Apple2026/pb_active_backdrop.bmp`
  - `wps/Apple2026/vb.bmp`
  - `wps/Apple2026/vb_backdrop.bmp`
  - `wps/Apple2026/qs_wheel.bmp`
  - `wps/Apple2026/qs_slider_fill.bmp`
  - `wps/Apple2026/qs_slider_track.bmp`
- Updated `tools/apple2026_skin_audit.py` so the reset is enforced:
  - required theme background = `ffffff`
  - `FAFAF8` / `FBFBF9` forbidden in the active Apple2026 SBS contract
  - source/runtime asset sample expectations now point at stock neutral white

Verified:
- `python tools/apple2026_symbol_assets.py` -> `OK`
- `python tools/apple2026_wps_art_frame.py` -> `OK`
- `python tools/apple2026_lockscreen_assets.py` -> `OK`
- `python tools/apple2026_wps_pill_bars.py` -> `OK`
- `python tools/apple2026_quickscreen_assets.py` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall`
  - initial reset install pass completed successfully
  - final follow-up install pass hit an unrelated `make install` move failure for
    `chessbox.rock`
- Manual simulator sync completed for the Apple2026 payload after that unrelated
  install failure:
  - copied source `Apple2026.sbs`, `Apple2026.wps`, and `wps/Apple2026/*`
    directly into `build-sim\simdisk\.rockbox`
  - patched installed `themes/Apple2026.cfg` background to `ffffff`
- `.\tools\verify-sim-install.ps1` -> `OK`
- Runtime file/hash spot-checks confirmed installed `Apple2026.sbs`,
  `Apple2026.wps`, `miniplayer_bg.bmp`, `wpsBackdrop.bmp`, and quick settings
  assets match the source reset outputs.
- `build-sim\rockboxui.exe` launched and stayed alive for 5 seconds.

What to verify first in the next build:
1. Quick settings, SBS, and WPS must all read as one neutral-white family with
   no residual green/yellow drift.
2. Header, list body, mini-player, WPS art matte, and quick settings shell must
   no longer show adjacent off-whites.
3. Mini-player and quick settings edges must read as clean shell-matched
   surfaces with shadow doing the separation, not haloed matte drift.

## 2026-04-13 (Apple2026 quick settings execution: flicker hardening + Themeify right-side slider adaptation)

Exact current clue:
- User feedback on the first implementation pass was specific:
  - quick settings flickered between the new surface and the old one
  - the center clickwheel treatment felt too complicated
  - Themeify's calmer layout, especially the right-side brightness/volume lane,
    was the preferred direction

Exact root-cause model:
- The quickscreen runtime mapping itself was not the fragile piece.
- The higher-risk issue was redraw ownership:
  - the skin-only quickscreen path had been pushed toward timer-driven full SBS
    redraw, which is too aggressive for a control surface that should remain
    visually stable between actual state changes
  - the underlying content viewport was still eligible to exist behind the SBS
    overlay while `%cs = 10`, which increased the chance of visible surface
    contention
- The first visual pass also over-explained the wheel area instead of letting
  the Themeify-style slider lane do the heavy lifting

Changed:
- `apps/gui/quickscreen.c`
  - Removed the timer-driven "always redraw while skin_only" behavior.
  - Kept the Apple2026 skin-only/native-body suppression path.
  - Added explicit `skin_request_full_update(CUSTOM_STATUSBAR)` only:
    - on quickscreen entry
    - on actual redraw-triggering state changes
  - Restored normal quickscreen polling cadence so the overlay is event-driven
    instead of constantly repainting.
- `wps/Apple2026.sbs`
  - Added `qs_blank` as a dedicated 1x1 quickscreen info viewport to suppress
    underlying list ownership while `%cs = 10`.
  - Updated the `%VI(...)` routing so quick settings activates `qs_blank`
    instead of leaving the list/content viewport live underneath the overlay.
  - Reworked the quick settings surface to be more Themeify-aligned:
    - simplified left-side grey wheel
    - compact middle value stack
    - right-side vertical brightness + volume pill sliders
  - Removed the old horizontal quickscreen slider rows and the center "Select"
    label clutter.
- `wps/Apple2026/qs_wheel.bmp`
  - Regenerated as a simpler Apple2026-neutral grey wheel with no crosshair
    detailing.
- `wps/Apple2026/qs_slider_fill.bmp`
- `wps/Apple2026/qs_slider_track.bmp`
  - Regenerated as shell-matched neutral vertical pill assets (`20x132`) for
    the new right-side slider lane.
- `tools/apple2026_skin_audit.py`
  - Added quick settings asset presence + dimension checks.
  - Added quick settings SBS contract checks for:
    - `qs_blank`
    - the Apple2026 wheel asset
    - the vertical brightness/volume pill bars
  - Added forbids for the old horizontal quickscreen rails and stale center
    label.
- `DESIGN_SYSTEM.md`
  - Updated quick settings rules to the implemented hybrid architecture and
    the new Themeify-inspired left-wheel/right-slider composition.
- `QuickScreenRegressionAudit.md`
  - Replaced the old native-body-only guidance with the current hybrid
    anti-regression rules:
    - core owns mapping + body suppression
    - SBS owns the visible overlay
    - no continuous full-screen timer redraw
- `Apple2026QuickSettingsPlan.md`
  - Updated the implementation plan so it no longer describes horizontal v1
    quick settings sliders.

Verified:
- `.\build-sim.ps1 -Incremental` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `python tools/apple2026_skin_audit.py` -> `OK`
- `build-sim\\rockboxui.exe` launched and stayed alive for 5 seconds
- `.\build-hw.ps1 -Target ipod6g -Incremental` -> `OK`
- `python tools/verify_plugin_package.py --build-dir build-hw-ipod6g --zip-artifact build-hw-ipod6g\\rockbox.zip` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only --zip-artifact build-hw-ipod6g\\rockbox.zip` -> `OK`
- New quick settings assets built and are present in source/runtime trees:
  - `qs_wheel.bmp`
  - `qs_slider_fill.bmp`
  - `qs_slider_track.bmp`
- New quick settings assets are also present in `build-hw-ipod6g\\rockbox.zip`:
  - `.rockbox/wps/Apple2026/qs_wheel.bmp`
  - `.rockbox/wps/Apple2026/qs_slider_fill.bmp`
  - `.rockbox/wps/Apple2026/qs_slider_track.bmp`

Assumed / still requires live interaction proof:
1. The `%VI(qs_blank)` suppression plus event-driven redraw is expected to fix
   the visible flicker, but this still needs an actual interactive quickscreen
   check in the simulator or on hardware.
2. The new simplified wheel should feel calmer in motion, but that is still a
   visual/interaction judgment pending live use.
3. The vertical pill sliders compile and package correctly; final tactile feel
   still needs direct use confirmation.
4. `verify_plugin_package.py` still reports the pre-existing `h264_poc.rock`
   mapping warning, but that is unrelated to Apple2026 quick settings.

## 2026-04-13 (Apple2026 mini-player follow-up: removed launch strip, title wipe slab, and edge-fringe regression)

Exact current clue:
- The new user screenshots showed a different post-fix failure shape:
  - launch/resume state still showed a bottom bar when no song was actively playing,
  - playing state showed a blank white slab across the title area,
  - the pill edge still had a visible fringe/halo against the SBS shell.

Exact root cause:
- `wps/Apple2026.sbs` still had an unconditional `%dr(0,0,320,50,FAFAF8,FAFAF8)`
  in the body lane, so the bottom strip stayed visible even when the pill image
  itself was hidden.
- The normal mini-player volume overlay still lived in ordinary always-rendered
  viewports at `x=60..256, y=203..223`, which overlaps the persistent title lane.
  That overlay could therefore blank the title area in steady state.
- `tools/apple2026_wps_art_frame.py` was still producing a soft anti-aliased body
  pass for `miniplayer_bg.bmp`, which left a visible fringe at the rounded edge
  instead of a clean same-tone shell/body boundary with shadow-only separation.

Changed:
- `wps/Apple2026.sbs`
  - Removed the unconditional `50px` bottom-strip clear from the mini-player body
    lane.
  - Removed background clears from persistent art/title/icon lanes so the body
    asset owns the surface and the content lanes only draw content.
  - Converted the mini-player volume overlay into transient labeled viewports:
    - `mp_volume_bg`
    - `mp_volume_clip`
  - Added a dedicated `%Vd(...)` trigger for those overlay lanes during
    `%?mv(1.2)` so the volume UI can temporarily replace title/art without
    permanently occupying the title rectangle.
- `tools/apple2026_wps_art_frame.py`
  - Rebuilt the mini-player body pass to use a hard rounded-rect mask over the
    shell-tone fill, so the pill interior erases shadow cleanly without a bright
    or tinted fringe.
- `tools/apple2026_skin_audit.py`
  - Updated Apple2026 contracts to guard against:
    - the old full-strip `%dr(0,0,320,50,FAFAF8,FAFAF8)` clear,
    - the old always-rendered volume-overlay viewport at `60,203,196,20`,
    - stale quick-settings expectation drift for `qs bottom`.
  - Added edge-pixel checks for `miniplayer_bg.bmp`.

Verified:
- `python tools/apple2026_wps_art_frame.py` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK` (hashes match; timestamp warning was stale)
- `build-sim\rockboxui.exe` launched and stayed alive for 5 seconds

What to verify first in the next build:
1. Cold launch / Resume Playback state must show no mini-player band at all.
2. Active playback must no longer show a blank white strip across the title lane.
3. The pill edge should read as shell-matched `FAFAF8` with only a subtle shadow,
   not a white fringe.
4. Volume adjustment should temporarily replace title/art, then fully return the
   title lane without leaving a blank slab behind.

## 2026-04-13 (Apple2026 mini-player steady-state fix: removed hideable lane ownership)

Exact current clue:
- The mini-player body and right play icon remained visible, but title and album
  art only appeared during brief menu-action redraws and then disappeared again.
- That meant the steady-state SBS path was not missing playback state entirely;
  it was specifically losing the content-owning lanes after transient redraws.

Exact root cause:
- `wps/Apple2026.sbs` was still using a `%Vl/%Vd` hideable-lane model for the
  mini-player content lanes.
- On transient/full redraws those lanes could be revealed correctly.
- On the next ordinary SBS action update, the skin engine's hidden-conditional
  cleanup re-evaluated those hideable child lanes and blanked the title/art
  ownership back out of the bottom bar.
- The body lane and play icon path were structurally simpler, which is why the
  bar still looked "alive" while the actual content disappeared.

Changed:
- `wps/Apple2026.sbs`
  - Removed the `%Vd(...)`-driven mini-player reveal path for:
    - body
    - album art
    - title-with-art
    - title-without-art
    - transport icon
    - volume overlay
  - Rebuilt those lanes as ordinary always-rendered SBS viewports.
  - Each lane now explicitly clears itself to `FAFAF8` and then conditionally
    redraws its content with `%?mp / %?mv / %?C`.
  - The mini-player body now clears the whole 50px lane to shell tone before
    conditionally drawing `miniplayer_bg.bmp`, so stop-state and volume-state
    transitions no longer depend on hidden viewport state.
- `MINIPLAYER_REGRESSION_AUDIT.md`
  - Recorded the hideable-lane ownership bug and the new ordinary-viewport
    recovery model.
- `DESIGN_SYSTEM.md`
  - Updated `54.5` so the mini-player contract now explicitly forbids `%Vl/%Vd`
    ownership for the Apple2026 bottom bar.

Verified:
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `build-sim\rockboxui.exe` launched and stayed alive for 5 seconds

What to verify first in the next build:
1. Start playback and return to Library without entering WPS first.
2. Confirm title stays visible after the first steady-state idle update, not just
   during the transition flash.
3. Confirm album art stays visible after the same idle/update boundary.
4. Confirm the bottom bar still suppresses correctly when stopped and swaps
   cleanly to the volume overlay while adjusting volume.

## 2026-04-13 (Apple2026 mini-player steady-state pass: static refresh + same-tone shell/body)

Exact current clue:
- The mini-player title and album art now both appear briefly during menu-action
  refreshes, then disappear again.
- That invalidates the earlier "missing content" theory and confirms the content
  path exists on the transient full-refresh path.

Exact steady-state root-cause model:
- Apple2026 mini-player content is owned by static SBS tags:
  - `%it` / `%fn`
  - `%?C`
  - `%Cd`
  - `%xd(...)`
- The normal SBS redraw loop in `apps/gui/statusbar-skinned.c` had been using
  `SKIN_REFRESH_NON_STATIC | SKIN_REFRESH_SCROLL`.
- Result: transient/full redraws could show the correct mini-player content, but
  the ordinary steady-state SBS pass still excluded the static-owned title/art
  path.

Changed:
- `apps/gui/statusbar-skinned.c`
  - SBS steady-state updates now request:
    `SKIN_REFRESH_NON_STATIC | SKIN_REFRESH_STATIC | SKIN_REFRESH_SCROLL`
- `wps/Apple2026.sbs`
  - Mini-player-owned lanes now clear to `FAFAF8` instead of `FBFBF9`:
    - `mp_albumart`
    - `mp_track`
    - `mp_track_noart`
    - `mp_playpause`
    - `volumebar_small_*`
- `tools/apple2026_wps_art_frame.py`
  - Mini-player body fill now matches shell `FAFAF8`
  - Mini-player art-mask corner fill now matches shell `FAFAF8`
- Regenerated assets:
  - `wps/Apple2026/miniplayer_bg.bmp`
  - `wps/Apple2026/art_mask.bmp`

Verified:
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -Incremental` -> `OK`
- asset pixel audit:
  - `miniplayer_bg.bmp` center now reads `FAFAF8`
  - `art_mask.bmp` opaque corner fill now reads `FAFAF8`

Still to verify manually in next build:
1. start playback and return to Library without opening WPS first
2. confirm title stays visible after back/up actions, not just during the flash
3. confirm album art stays visible after the transient refresh
4. confirm shell and bar now read as one warm surface family, separated only by shadow

## 2026-04-13 (Apple2026 quick settings planning pass: implementation-grade spec locked before coding)

Exact current status:
- No production code was changed in this pass.
- No build, simulator, or hardware validation was run in this pass.
- This pass was used to convert the earlier quick settings audit into an
  implementation-grade planning document with exact hook sites, code sketches,
  gating rules, and regression protections.

Changed:
- Added `Apple2026QuickSettingsPlan.md`
  - Documents:
    - Themeify quick settings audit findings
    - Apple2026 current-state audit
    - gap analysis
    - hybrid architecture recommendation
    - fixed Apple2026 interaction model
    - exact file/function targets
    - code sketches for runtime mapping, theme gating, `%Q*` token routing,
      native-body suppression, and menu lockout
    - staged implementation plan
    - validation matrix
    - regression gates
    - regression-prevention architecture
    - symptom-to-cause triage matrix
    - patch-level smoke tests
    - planned debug instrumentation
    - release-blocker checklist
    - final regression sign-off matrix
    - prior failed quickscreen implementation patterns that are now explicitly banned
    - exact Apple2026 visual-consistency lock for fonts, wheel restraint, and
      pill-slider behavior
    - verified brightness-setting-bar path so the future brightness slider can
      use a real skin-engine setting bar rather than a speculative custom path
    - a dedicated `DESIGN_SYSTEM.md` reference lock so quick settings typography,
      gutter rhythm, muted color hierarchy, and pill-slider behavior are tied
      to existing Apple2026 rules instead of memory or taste
    - a Themeify compatibility hardening pass separating:
      - what Apple2026 should borrow from Themeify
      - what Apple2026 must explicitly reject from Themeify's suppression-heavy
        theme architecture
    - mirrored that Themeify compatibility rule into `DESIGN_SYSTEM.md` and
      `QuickScreenRegressionAudit.md` so it remains part of the branch's design
      and anti-regression authority, not just the feature plan
- Updated `DESIGN_SYSTEM.md`
  - Added a planned Apple2026 quick settings target note so project docs no
    longer imply that the long-term down-button behavior should remain
    `sleeptimer duration`.

Verified:
- `Apple2026QuickSettingsPlan.md` did not previously exist.
- Current live quickscreen ownership is still:
  - core quickscreen lifecycle and native body in `apps/gui/quickscreen.c`
  - `%Q*` token truth in `apps/gui/skin_engine/skin_tokens.c`
  - no custom quick settings overlay in `wps/Apple2026.sbs`
- The Apple2026 path can use the existing quickscreen button semantics for the
  desired brightness behavior:
  - top/right increment
  - left/bottom decrement
- There is already a real Apple2026 theme-selection helper pattern in
  `apps/main.c`, so the future implementation should promote that into shared
  ownership instead of duplicating theme-path checks.

Assumed / not yet executed:
- The exact final quick settings copy and asset set may still tighten during the
  coding phase.
- The final SBS slider composition may need small syntax/layout adjustments once
  implementation begins.
- No runtime behavior was validated in this pass because this was a planning-only
  stage.

Next:
- If the user authorizes coding, start with:
  1. shared Apple2026 theme gate
  2. quickscreen runtime contract
  3. `%Q*` runtime token source
  4. native quickscreen-body suppression
  5. only then the Apple2026 SBS quick settings surface

## 2026-04-13 (Apple2026 mini-player recovery execution: metadata bootstrap fallback implemented)

Exact current status:
- Executed the first recovery branch from `MINIPLAYER_REGRESSION_AUDIT.md`:
  remove the WPS-only metadata dependency from the generic skin lookup path.
- This change is aimed at the mini-player title regression specifically.
- Album-art visibility is still treated as a separate, not-yet-proven runtime
  path until the proof matrix is exercised.

Changed:
- `apps/gui/wps.h`
  - Added shared skin-safe metadata accessors:
    - `wps_get_current_id3()`
    - `wps_get_next_id3()`
- `apps/gui/wps.c`
  - Implemented those accessors so SBS and other skinned surfaces can fall back
    to `audio_current_track()` / `audio_next_track()` when `wps_state_init()`
    has not yet populated `state->id3` / `state->nid3`.
- `apps/gui/skin_engine/skin_tokens.c`
  - Updated current/next track lookup to use the new fallback helpers instead of
    directly depending on `get_wps_state()->id3` / `nid3`.
  - Updated the cuesheet path check to use the same fallback-aware current-track
    source.
- `apps/gui/skin_engine/skin_display.c`
  - Updated generic skin update and progressbar code to use the fallback-aware
    current-track source as well, so non-WPS skinned surfaces stop assuming WPS
    has already bootstrapped metadata state.

Verified:
- `.\build-sim.ps1 -Incremental` -> `OK`
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- Simulator startup sanity check:
  - launched `build-sim\rockboxui.exe`
  - kept alive for 5 seconds
  - no immediate skin-load crash (`SIM_OK`)

What this should fix:
- The Apple2026 SBS title path should no longer require WPS to have been opened
  first in order for `%it` / `%fn` to resolve against the current track.

What remains unproven:
- Whether album art is still missing after the title path is repaired.
- Whether any remaining blank/flash behavior is now purely a viewport/order issue.
- Whether the current image-order engine patch affects SBS art visibility.

Next:
- Run the runtime proof matrix, starting with:
  1. playback -> Library before WPS entry,
  2. playback -> WPS -> back to Library,
  3. track change while staying in Library.
- If title is restored but art is still absent, move directly to the album-art
  handle/visibility branch in the audit ladder.

## 2026-04-13 (WPS rounded-art regression recovery: missing no-token post-AA tagging fixed)

Exact current regression:
- The Apple2026 WPS hero art lost its rounded edges entirely.
- This invalidated the earlier assumption that moving the trim into the same
  viewport as `%Cd` was enough by itself.

Exact root cause:
- The engine-side post-album-art image ordering patch was incomplete.
- `apps/gui/skin_engine/skin_display.c` already supports two image passes:
  pre-album-art and post-album-art.
- `apps/gui/skin_engine/skin_render.c` already tagged several image-display
  paths with `draw_after_albumart`, but it missed the plain no-token
  `%xd(...)` path (`SKIN_TOKEN_IMAGE_PRELOAD_DISPLAY` with `id->token == NULL`).
- Apple2026 now uses that exact path for:
  - WPS hero trim: `%Cd%xd(K)`
  - SBS mini-player mask: `%Cd%xd(Y)`
- Result: the trim and mask images were still rendered in the pre-art pass and
  then buried by the live album art, which made the hero corners disappear.

Changed:
- `apps/gui/skin_engine/skin_render.c`
  - Plain `%xd(...)` image-display calls now inherit
    `draw_after_albumart = info->albumart_seen` just like the token-driven
    image-display paths.
- Added `WPS_REGRESSION_AUDIT.md`
  - Documents the full WPS pipeline, hypothesis tree, Interpod and Themeify
    comparison, shared mini-player risk, and recovery ladder in the same
    forensic style as `MINIPLAYER_REGRESSION_AUDIT.md`.
- Updated `DESIGN_SYSTEM.md`
  - Locked the missing engine invariant: same-viewport trim after `%Cd` only
    works if plain `%xd(...)` is also promoted to the post-album-art draw pass.

Verified:
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -Incremental` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- simulator smoke launch (`build-sim\rockboxui.exe`, 5s) -> `SIM_OK`
- pixel audit re-confirmed:
  - `wpsBackdrop.bmp` art-box matte = `FAFAF8`
  - `wpsArtCorners.bmp` corner wedges = `FAFAF8`
  - `art_mask.bmp` corner wedges = `FBFBF9`
- Current source therefore points to ordering, not matte mismatch, as the main
  regression cause.

Next:
- Manual runtime proof is still required:
  1. open WPS on a track with art and confirm corners are visible immediately
  2. change tracks repeatedly and watch for any remaining pop-in
  3. repeat on same-album changes
  4. re-check the mini-player art lane because it shares the same `%Cd%xd(...)`
     engine path

## 2026-04-13 (Apple2026 mini-player audit pass: reference-theme taxonomy and non-regression rules)

Exact current status:
- The audit now goes beyond local Apple2026 symptoms and documents which SBS
  structural patterns are common across the reference themes, which ones are high
  risk for Apple2026, and which Rockbox engine invariants the final fix must
  satisfy to avoid regressing again.

Changed:
- Expanded `MINIPLAYER_REGRESSION_AUDIT.md`
  - Added a reference-theme taxonomy separating:
    - lane-based SBS themes (`Interpod`, `Interpod 2`, `iClassic`, `iPone`,
      `Obsede'`)
    - compositor-heavy SBS themes (`Themify`, `Themify_2`, `adwaitapod`,
      `adwaitapod_simplified`, `amusicpodds`)
  - Added deeper Rockbox lifecycle findings:
    - SBS playback redraw callbacks do not populate `get_wps_state()->id3`
    - WPS still owns the bootstrap and upkeep of `id3` / `nid3`
    - album-art handle lookup is more independent from WPS state than title text
    - album-art slot reuse is dimension-based
    - themed SBS event delivery depends on viewport/theme enable state
  - Added a non-regression hardening checklist and runtime proof matrix for the
    final sign-off.

Verified:
- Re-read the imported SBS references and confirmed Apple2026 is structurally
  closest to the non-`%VB` Interpod family, not the compositor-heavy Themeify
  family.
- Re-confirmed from Rockbox source that:
  - `do_sbs_update_callback()` requests redraws but does not populate WPS state
  - `wps_state_init()` and `track_info_callback()` are still the owners of WPS
    metadata bootstrap/upkeep
  - `playback_current_aa_hid()` does not depend on `get_wps_state()->id3`

Next:
- Keep the implementation branch ordered around the audit ladder:
  1. prove or disprove cold-path metadata bootstrap failure,
  2. prove album-art handle validity separately,
  3. only then decide whether the remaining issue is viewport visibility/order or
     the current skin-engine image-order patch,
  4. apply visual unification only after content survives the full runtime proof
     matrix.

## 2026-04-13 (Apple2026 mini-player audit pass: render pipeline and alternate recovery ladder)

Exact current status:
- The audit now explicitly maps how Apple2026 SBS content is rendered, hidden,
  cleared, and layered.
- The current working view is that the regression may be a combination of blank
  metadata source plus viewport clear/visibility behavior, not one simple SBS
  route bug.

Changed:
- Expanded `MINIPLAYER_REGRESSION_AUDIT.md`
  - Added a render pipeline map covering:
    - parse-time hidden viewport state,
    - `%Vd(...)` visibility behavior,
    - file-order viewport paint order,
    - per-viewport `%Vb(...)` clears,
    - text -> pre-AA images -> album art -> post-AA image ordering,
    - the fact that `draw_after_albumart` only has scope inside the same
      viewport as `%Cd`.
  - Added an order-failure inventory covering:
    - `%Vd` order vs actual paint order,
    - hidden-branch cleanup clears,
    - child lane clears repainting over `miniplayer_bg.bmp`,
    - same-viewport image-over-text risks,
    - later viewport coverage over album art,
    - wash-hidden/full-refresh timing.
  - Added an explicit recovery ladder with primary and alternate plans:
    - Plan A: metadata bootstrap fix
    - Plan B: viewport visibility/text-source instrumentation
    - Plan C: album-art handle/slot audit
    - Plan D: isolate current skin-engine image-order patch
    - Plan E: visual unification only after content proof

Verified:
- Re-confirmed from engine source that labeled `%Vl(...)` viewports start hidden
  and `%Vd(...)` enables visibility but does not directly paint in that textual
  order.
- Re-confirmed that `skin_render()` paints viewports in tree/file order.
- Re-confirmed that mini-player child viewports currently clear with `FBFBF9`
  during full refresh, which means they repaint portions of the mini-player body
  instead of remaining visually transparent over it.

Next:
- Use the audit ladder during the next implementation/debug pass.
- Do not resume visual tuning until the runtime proof stage resolves whether the
  title and art are absent because their data source is empty, because their
  viewports are being cleared/hidden, or both.

## 2026-04-13 (Apple2026 mini-player audit expanded: full failure tree, color-match findings, Themeify review)

Exact current status:
- The mini-player audit has been expanded from a single leading hypothesis into a
  full failure tree covering metadata bootstrap, album-art slot/handle timing,
  render-order regressions, hidden viewport state, routing/overlay edge cases,
  and low-confidence process mismatches.
- The color mismatch is now confirmed from source, not just from screenshots:
  the Apple2026 shell clears to `FAFAF8`, while the current mini-player body and
  child lanes clear to `FBFBF9`.

Changed:
- Expanded `MINIPLAYER_REGRESSION_AUDIT.md`
  - Added the complete hypothesis inventory for why the title and album art can
    still fail even after the prior SBS refresh changes landed.
  - Added a dedicated color-match audit with measured source values from
    `wpsBackdrop.bmp`, `miniplayer_bg.bmp`, and `wps/Apple2026.sbs`.
  - Added a stronger Themeify comparison section documenting what is safe to
    borrow structurally and what is unsafe to port into Apple2026 SBS.
- Updated `DESIGN_SYSTEM.md`
  - Marked the current `FBFBF9` mini-player body split as provisional and
    recorded the active target direction: shell and mini-player body should
    ultimately share the same base tone, with only a subtle shadow creating
    separation.

Verified:
- Re-sampled current Apple2026 generated assets:
  - `wpsBackdrop.bmp` base = `FAFAF8` (`250,250,248`)
  - `miniplayer_bg.bmp` body = `FBFBF9` (`251,251,249`)
  - lower mini-player shadow tail reaches approximately `F7F7F5`
- Re-checked `wps/Apple2026.sbs` and confirmed the lane clears match that split:
  shell-owned viewports use `FAFAF8`, mini-player-owned child lanes use
  `FBFBF9`.
- Re-read Themeify source from
  `Imported Reference Themes/themify-v.2.0-dev.2/.../Themify_2/.rockbox/`
  and confirmed:
  - `statusbar: off` in `Themify_2.cfg`
  - SBS uses a `%VB` compositor/reveal pattern for the miniplayer
  - Its structural lesson is unified ownership, not a direct `%VB` SBS port for
    Apple2026.

Next:
- Keep visual changes frozen until the content path is proven.
- Run the next recovery pass in this order:
  1. reproduce before and after opening WPS once,
  2. prove or disprove the metadata-bootstrap hypothesis,
  3. audit SBS album-art slot/handle/paint behavior separately,
  4. only after content works, re-baseline the mini-player to a same-tone
     shell/body rule with subtle shadow-only separation.

## 2026-04-13 (Apple2026 mini-player audit reset: prior fix claims invalidated, root audit doc created)

Exact current status:
- The latest user screenshot shows the mini-player still rendering as a largely
  blank bar: transport icon visible, but no persistent song title and no album art.
- This invalidates the earlier claim that the title/art restore, warm-body fix,
  and rounded-rectangle refinement were already verified.

Changed:
- Added root-level audit doc: `MINIPLAYER_REGRESSION_AUDIT.md`
  - Records the current bad-state repro and screenshot baseline.
  - Lists which recent `WORK_LOG.md` / `DESIGN_SYSTEM.md` mini-player claims are
    contradicted by reality.
  - Captures the current Apple2026 ownership map for the mini-player.
  - Ranks the current root-cause candidates, with the metadata-bootstrap issue as
    the leading explanation for the missing title.
  - Splits album-art investigation from title investigation instead of treating
    them as one bug.
  - Compares Apple2026 against `Themify_2_dev_build_2` as a structural reference,
    while explicitly ruling out a blind `%VB` port into Apple2026 SBS.
- Updated `DESIGN_SYSTEM.md`
  - Marked the current Apple2026 mini-player geometry/color block as provisional
    and under active audit, not a verified runtime baseline.

Verified:
- Re-inspected current Apple2026 SBS/WPS and skin-engine source.
- Confirmed the current tree already contains the attempted `SKIN_REFRESH_SCROLL`
  and `PLAYBACK_EVENT_CUR_TRACK_READY` fixes, so the current regression is deeper
  than the last SBS-only hypothesis.
- Confirmed `Themify_2_dev_build_2` uses a materially different `%VB`-backed
  mini-player compositor model.
- Confirmed `apps/gui/skin_engine/skin_tokens.c` resolves current-track metadata
  through `get_wps_state()->id3`, while `wps_state_init()` is explicitly called
  from WPS entry in `apps/gui/wps.c`.

Next:
- Run the recovery pass from `MINIPLAYER_REGRESSION_AUDIT.md` in order:
  1. reproduce before/after opening WPS once,
  2. prove or disprove the metadata-bootstrap hypothesis,
  3. audit SBS album-art slot/handle/paint path separately,
  4. revisit color/shape only after content is restored.

## 2026-04-13 (WPS transition polish: same-pass rounded art trim, subtle shadow lift, tighter metadata)

Exact regression symptoms:
- On song change, the new WPS hero cover could appear with sharp square corners first, then the rounded treatment would visibly re-assert itself a moment later.
- The effect was jarring even when moving between tracks that shared the same album art.
- The title/artist stack still read a touch too airy relative to the art box, and the hero shadow wanted a very small depth increase to sit more comfortably against the Now Playing pill.

Exact root cause:
- `wps/Apple2026.wps` drew the hero album art via `%Cd` inside a `%VB` backdrop-buffer viewport, but the rounded corner treatment lived in a separate normal-plane `album_art_mask` viewport.
- Rockbox does not update those as one atomic surface: the art compositor and the normal image plane have different refresh timing, and `%Cd` is drawn after ordinary skin images.
- That made the rounded trim structurally fragile. The cover art could update first in the backdrop buffer while the corner trim waited for a later viewport refresh on the main plane.
- Interpod avoided this class of bug because it did not try to repair hero-art corners from a second viewport; it kept the album-art path structurally stable and used a safer framed-underlay approach instead.
- Pixel audit showed no true live-corner color mismatch in `wpsArtCorners.bmp`; the real visual instability was the split render path. The one nearby matte mismatch was the no-art placeholder, which was still cooler/purer white than the `FAFAF8` shell.

Changed:
- `apps/gui/skin_engine/wps_internals.h`
  - Added a per-image runtime flag so image displays can intentionally render after `%Cd` when they appear later in token order.
- `apps/gui/skin_engine/skin_render.c`
  - Tracks whether album art has already been encountered while rendering a viewport and tags later image displays as post-album-art draws.
- `apps/gui/skin_engine/skin_display.c`
  - Splits image drawing into pre-AA and post-AA passes so an image placed after `%Cd` in the same viewport is composited after the live album art instead of before it.
- `wps/Apple2026.wps`
  - Removed the separate `album_art_mask` viewport from the hero art path.
  - Moved `wpsArtCorners.bmp` into the `%VB` hero-art viewport and now draw it immediately after `%Cd` as `%Cd%xd(K)`.
  - Kept the hero art on the Interpod-proven `%VB` compositor path, but stopped splitting trim and art across different planes.
  - Lowered the title lane by `1px` (`y=160 -> 161`) so the art/shadow gets a touch more breathing room while the title/artist stack reads tighter.
- `wps/Apple2026.sbs`
  - Moved the mini-player art mask into the same viewport as `%Cd` so the compact art path does not carry the same structural drift bug.
- `tools/apple2026_wps_art_frame.py`
  - Raised the WPS hero-art shadow very slightly (`11% -> 12%`, blur `3.0 -> 3.2`) while preserving the mini-player pill shadow at its previous values.
  - Changed the generated `albumPlaceholder.bmp` background to `FAFAF8` so the no-art state no longer introduces a cooler matte block.
- Regenerated assets:
  - `wps/Apple2026/wpsBackdrop.bmp`
  - `wps/Apple2026/wpsArtCorners.bmp`
  - `wps/Apple2026/albumPlaceholder.bmp`
  - `wps/Apple2026/miniplayer_bg.bmp`
- `DESIGN_SYSTEM.md`
  - Locked the rule that hero-art trim must render after `%Cd` inside the same `%VB` viewport and must never return to a separate mask viewport.

Verified:
- `python tools/apple2026_wps_art_frame.py` -> regenerated cleanly.
- `.\build-sim.ps1 -Incremental` -> `OK`.
- `.\tools\verify-sim-install.ps1` -> `OK`.
- Simulator startup sanity check: launched `build-sim\rockboxui.exe`, allowed it to run for 5 seconds, no immediate skin-load crash (`SIM_OK`).
- Pixel spot checks:
  - `wpsBackdrop.bmp` still keeps the live art box interior flat `FAFAF8` with shadow only outside the box.
  - `albumPlaceholder.bmp` outer pixels now match `FAFAF8` instead of pure white.

Files/assets involved:
- `apps/gui/skin_engine/wps_internals.h`
- `apps/gui/skin_engine/skin_render.c`
- `apps/gui/skin_engine/skin_display.c`
- `wps/Apple2026.wps`
- `wps/Apple2026.sbs`
- `tools/apple2026_wps_art_frame.py`
- `wps/Apple2026/wpsBackdrop.bmp`
- `wps/Apple2026/wpsArtCorners.bmp`
- `wps/Apple2026/albumPlaceholder.bmp`
- `wps/Apple2026/miniplayer_bg.bmp`

What to verify first in the next build:
- Change tracks repeatedly from the WPS and watch the hero cover corners during the first half-second after each change.
- Repeat the check on two tracks from the same album; the rounded treatment should stay visually stable rather than reappearing.
- Confirm the shadow feels only slightly richer, not heavier.
- Confirm the title/artist stack reads a touch tighter and that the extra air above the title helps the art box breathe.
- Confirm the no-art placeholder now sits on the same shell matte as the rounded trim.

## 2026-04-13 (Apple2026 mini-player regression fix: persistent title/art restore, warm body unification, less-pill shape)

Exact repro:
- Start playback, return to the Library/menu shell, and look at the bottom Now Playing bar.
- Song title could appear briefly during back/up or other menu-action transitions, then disappear again in steady state.
- Album art stayed missing instead of appearing after playback settled.
- The mini-player body read cooler/brighter than the warmed `FAFAF8` shell and still felt too capsule-like compared with the Apple Music reference.

Exact root cause:
- `apps/gui/statusbar-skinned.c` repainted SBS via `SKIN_REFRESH_NON_STATIC`, which excludes `SKIN_REFRESH_SCROLL`. The Apple2026 mini-player title lanes (`mp_track` / `mp_track_noart`) are `%s` scrolling lines backed by static metadata (`%it|%fn`), so they could appear on a forced redraw and then get wiped by the next ordinary SBS repaint.
- `apps/gui/viewport.c` subscribed SBS refreshes to `PLAYBACK_EVENT_TRACK_CHANGE` and `PLAYBACK_EVENT_NEXTTRACKID3_AVAILABLE`, but not `PLAYBACK_EVENT_CUR_TRACK_READY`. That left the mini-player vulnerable to missing the redraw where current-track album art actually became ready for `%?C/%Cd`.
- `wps/Apple2026.sbs` still cleared the mini-player-owned sublanes to mixed `FFFFFF` / `F2F2F2`, while the shell itself was already `FAFAF8`. The visible mismatch was therefore not "SBS forgot to use `FAFAF8`"; it was the bar asset and lane clears drifting cooler than the shell.
- `tools/apple2026_wps_art_frame.py` generated `miniplayer_bg.bmp` as a literal capsule (`radius = pill_h // 2`), which made the bar feel too bubbly versus the Apple Music rounded-rectangle reference.

Changed:
- `apps/gui/statusbar-skinned.c`
  - Expanded SBS action refreshes from `SKIN_REFRESH_NON_STATIC` to `SKIN_REFRESH_NON_STATIC | SKIN_REFRESH_SCROLL` so the mini-player title redraws on normal menu/action updates instead of disappearing after the transition flash.
- `apps/gui/viewport.c`
  - Added `PLAYBACK_EVENT_CUR_TRACK_READY` to the SBS refresh subscriptions so the mini-player gets the full redraw that exposes current-track album art as soon as it is ready.
- `wps/Apple2026.sbs`
  - Kept the current ownership model where `bottombarsmall` owns the bar surface, `mp_albumart` owns `%Cl/%Cd` plus the mask draw, `mp_track` / `mp_track_noart` own text, and `mp_playpause` owns the transport icon.
  - Normalized the mini-player-owned lane backgrounds from `FFFFFF` / `F2F2F2` to `FBFBF9` so redraw clears no longer create cool-white patches inside the warm shell.
  - Updated the branch comments to reflect the real 50px rounded-rectangle mini-player lane.
- `tools/apple2026_wps_art_frame.py`
  - Refined `miniplayer_bg.bmp` from a full capsule to a rounded rectangle: `300x40` body, `x=10`, `y=4`, `r=13`, `FBFBF9` fill, existing soft shadow preserved.
  - Added generator-backed output for `art_mask.bmp` so the 32x32 mini-player album-art corners now match the new warm mini-player body instead of an older pure-white mask asset.
- `tools/apple2026_skin_audit.py`
  - Added `art_mask.bmp` and `miniplayer_bg.bmp` to the required Apple2026 source/runtime assets and dimension checks.
- Regenerated assets:
  - `wps/Apple2026/miniplayer_bg.bmp`
  - `wps/Apple2026/art_mask.bmp`

Verified:
- `python tools/apple2026_wps_art_frame.py` regenerates the mini-player surface and mini-player art mask with the new rounded-rectangle/body-fill contract.
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `python tools/apple2026_skin_audit.py` -> `OK`
- `.\build-sim.ps1 -Incremental -SkipDep` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- `build-sim\rockboxui.exe` launch smoke test -> process started successfully and remained alive for 5s before forced termination
- Installed simulator assets now include the regenerated `art_mask.bmp` and `miniplayer_bg.bmp`.

How to verify first in the next build:
- Start playback, return to Library, and repeatedly press back/up/context-menu actions.
- Confirm the song title stays visible instead of only flashing during the transition redraw.
- Confirm mini-player album art appears without needing to enter full WPS first.
- Check that the mini-player reads as a softer rounded rectangle rather than a full pill.
- Compare the bar body against the shell: shell should remain `FAFAF8`; mini-player body should read as the slightly brighter `FBFBF9` surface, with the shadow doing most of the separation.

## 2026-04-13 (WPS render-path fix: hero art + title moved to explicit named viewport routing)

Exact regression symptoms:
- WPS hero album art was missing during normal playback updates.
- WPS song title was also missing during steady-state playback, but could flash briefly during a menu-triggered/full redraw.
- Other explicitly routed WPS lanes such as artist/progress remained more stable, which isolated the regression to the hero-art/title render path itself.

Exact root cause:
- `wps/Apple2026.wps` still relied on the unlabeled auto-draw path for the two key hero elements:
  - the `150x150` album-art viewport,
  - the main song-title viewport.
- The rest of the WPS already used explicit named viewport routing via `%Vd(...)` (`artist_line`, `pb`, `elapsed_time`, `remain_time`, etc.).
- In practice, the unlabeled hero art/title lanes were not surviving steady-state WPS refreshes reliably; they could appear on a full redraw, then disappear again on normal update cycles.
- This matched the user-visible behavior exactly: title briefly visible during a menu-triggered redraw, then gone again.

Changed:
- `wps/Apple2026.wps`
  - Added explicit logic calls for `%Vd(album_art)`, `%Vd(album_art_mask)`, and `%Vd(title_line)`.
  - Converted the hero art viewport from unlabeled `%V(...)` to named `%Vl(album_art,...)`.
  - Converted the art corner overlay viewport from unlabeled `%V(...)` to named `%Vl(album_art_mask,...)`.
  - Converted the song title viewport from unlabeled `%V(...)` to named `%Vl(title_line,...)`.

Verified:
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`
- `.\tools\verify-sim-install.ps1` -> `OK`
- Runtime/source hash check: installed `build-sim/simdisk/.rockbox/wps/Apple2026.wps` matches repo source after install.

How to verify first in the next run:
- Open WPS while playback is already active.
- Confirm the hero art and title stay visible without needing a menu/back redraw.
- Press `MENU` and return once to confirm the title no longer only appears during the transition flash.

## 2026-04-13 (WPS album-art edge cleanup pass: mask corner fix, flat matte interior, exact-fit placeholder)

Exact artifact symptoms:
- Rounded album-art corners still showed tiny dark specks at the left corners.
- Some art/placeholder edges read slightly dirty against the warm-white shell instead of optically clean.
- The no-art placeholder looked heavier than the live art treatment, with visible border/card lines in the WPS screenshot.

Exact root cause:
- `wps/Apple2026/wpsArtCorners.bmp` was generated with both `(0,0)` and `(0,h-1)` forced to the magenta transparency key. Rockbox color-LCD transparency is literal `FF00FF`, not a "first pixel declares the key" rule, so the top-left art pixel was being exposed unnecessarily.
- `wps/Apple2026/wpsBackdrop.bmp` still used a rounded clear inside the art box. That left slightly shadow-tinted pixels at the inner art corners, which became visible whenever the corner mask exposed or failed to fully cover a corner pixel.
- `wps/Apple2026/albumPlaceholder.bmp` was an oversized pre-framed asset (`156x156`) being drawn inside the live `150x150` art slot, so the placeholder state carried its own border/card edge and fought the new WPS framing treatment.

Changed:
- `tools/apple2026_wps_art_frame.py`
  - Clears the full `150x150` art box back to flat `FAFAF8` after shadow compositing, so only the outside shadow remains.
  - Generates `wpsArtCorners.bmp` with literal magenta transparency only; no real corner pixel is sacrificed to declare transparency.
  - Generates `albumPlaceholder.bmp` as an exact-fit `150x150` neutral placeholder with no embedded frame/border.
- `tools/apple2026_symbol_assets.py`
  - Updated the placeholder generator path to the same exact-fit, no-border `150x150` placeholder contract so the older framed placeholder is not regenerated later.
- `tools/apple2026_skin_audit.py`
  - Added explicit size checks for `albumPlaceholder.bmp`, `wpsArtCorners.bmp`, and `wpsBackdrop.bmp`.
  - Added `wps/Apple2026/albumPlaceholder.bmp` to the required Apple2026 source/runtime asset set.
- Regenerated assets:
  - `wps/Apple2026/wpsBackdrop.bmp`
  - `wps/Apple2026/wpsArtCorners.bmp`
  - `wps/Apple2026/albumPlaceholder.bmp`
  - `wps/Apple2026/miniplayer_bg.bmp`

Verified:
- Source audit: `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- Simulator install: `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`
- Simulator package check: `.\tools\verify-sim-install.ps1` -> `OK`
- Pixel-level verification:
  - `wpsArtCorners.bmp` now covers the true top-left corner pixel instead of leaving it transparent.
  - `wpsBackdrop.bmp` now reports flat `FAFAF8` samples inside the art box corners; shadow remains outside the box only.
  - `albumPlaceholder.bmp` is now `150x150` with no border pixels on the outer edges.
- Preview composites:
  - synthetic solid-color art now shows clean rounded corners with no exposed dark corner pixel.
  - no-art placeholder composite no longer carries the old inset frame/border lines.

Files/assets involved:
- `wps/Apple2026.wps`
- `wps/Apple2026/wpsBackdrop.bmp`
- `wps/Apple2026/wpsArtCorners.bmp`
- `wps/Apple2026/albumPlaceholder.bmp`
- `tools/apple2026_wps_art_frame.py`
- `tools/apple2026_symbol_assets.py`
- `tools/apple2026_skin_audit.py`

How to verify first in the next build:
- Open WPS on a track with real album art and inspect all four corners at 1:1.
- Open WPS on a no-art track and confirm the placeholder shows no gray border/frame lines.
- Check the top-left and bottom-left art corners first; those were the pixels previously most likely to show the defect.
- Confirm any non-square art padding reveals flat `FAFAF8`, not shadow residue.
- Note: the automated simulator capture helper still uses an outdated root-menu navigation path for "Resume Playback", so manual WPS navigation is more reliable for the visual sign-off pass.

## 2026-04-13 (Apple2026 regression recovery: SBS backdrop bleed, mini-player title scale, album-art restore)

Exact regression symptoms:
- Boot/loading screen pixels remained visible behind the Library/menu shell on launch.
- Starting playback caused WPS art/backdrop pixels to appear behind the menu list.
- The bottom mini-player title read too large/heavy for the pill.
- Mini-player album art failed to appear, leaving the no-art/title-heavy fallback look.

Exact root cause:
- The simulator install tree was still shipping a bad `Apple2026.sbs` variant where
  `bottombarsmall` had been converted to `%VB`.
- In Rockbox, a `%VB` viewport becomes the active LCD backdrop buffer. Menu/list
  clears then restore from that backdrop instead of a flat background color.
- Because the SBS mini-player only repainted the pill region, the rest of the
  backdrop buffer preserved stale framebuffer content:
  - boot/loading pixels on first launch,
  - WPS pixels after playback started.
- The same broken SBS path destabilized mini-player ownership: the menu shell was
  restoring the backdrop independently of the art/title lanes, so the mini-player
  art never became a stable visible thumbnail.
- The pill title also stayed on the old 20px semibold header tier, which reads too
  large once the pill is treated as a compact shell element.

Changed:
- `wps/Apple2026.sbs`
  - Kept the mini-player in the normal SBS draw plane and documented that `%VB`
    must never be used there.
  - Reduced the mini-player title to the compact 14px tier (`slot 8`) so the pill
    reads as a compact now-playing bar instead of header-sized chrome.
- `tools/apple2026_skin_audit.py`
  - Added a hard failure if `Apple2026.sbs` contains `%VB`, preventing this exact
    backdrop-ownership regression from shipping again.
- `DESIGN_SYSTEM.md`
  - Documented the intended ownership model: menu/list shell owns the screen in SBS
    state; only full-screen WPS is allowed to own backdrop-buffer composition.

Rendering ownership model:
- Menu state:
  - SBS owns header chrome and the mini-player overlay.
  - The active `%Vi(...)` UI viewport owns the list body.
  - No SBS backdrop buffer is allowed; list clears must resolve to the shell color.
- Playback while browsing:
  - WPS must not appear behind the menu.
  - Only the SBS mini-player may appear at the bottom of the menu shell.
- WPS / Now Playing:
  - WPS owns the full-screen backdrop/art composition.
  - WPS album art remains the only full-screen backdrop-buffer owner.
- Mini-player:
  - SBS owns the 32x32 thumbnail, compact title lane, and play/pause glyph inside
    the pill.

Verification plan:
- `python tools/apple2026_skin_audit.py --source-only`
- `.\build-sim.ps1 -InstallOnly -StrictInstall`
- Confirm installed `build-sim/simdisk/.rockbox/wps/Apple2026.sbs` contains no `%VB`
  and ships the compact mini-player title lane.
- Re-test launch and browse-while-playing to confirm:
  - no boot screen bleed,
  - no WPS bleed behind the menu,
  - visible mini-player album art,
  - smaller compact mini-player title.

Verified:
- `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- `.\build-sim.ps1 -InstallOnly -StrictInstall` -> `OK`
- Installed `build-sim/simdisk/.rockbox/wps/Apple2026.sbs` now ships:
  - no live `%VB` tags,
  - compact mini-player title lanes on `slot 8`,
  - the standard SBS mini-player album-art lane (`%Cl(0,0,32,32)` + `%Cd`).

## 2026-04-13 (WPS Album Art Styling Pass: Apple Music Rounded Corners + Soft Shadow)

Fixes for UI Divergence and Shadow Bleed:
- **Dark Shadow Ring Artifact**: The 11% black drop shadow was previously baked entirely underneath the 150x150 album art area on `wpsBackdrop.bmp`. When Rockbox scaled non-square album art down, the padding revealed the dark shadow interior. **Fix**: Updated `apple2026_wps_art_frame.py` to draw the 150x150 interior of the shadow back to pure `FAFAF8` so only the blurred *outside* shadow remains.
- **List / Pill Background Mismatch**: The mini-player pill background was `FAFAF8` but `WPSLIST` instructed the UI list to draw on `FFFFFF`, creating an ugly visual banding around the pill. **Fix**: Changed `WPSLIST` background color from `ffffff` to `fafaf8`.
- **Shadow Parameter Sync**: Added `generate_miniplayer()` to `apple2026_wps_art_frame.py` so the mini-player pill generated uses the exact same `SHADOW_DY=2`, `SHADOW_OPACITY=11%`, and `SHADOW_BLUR=3px` settings as the album art frame, ensuring complete aesthetic cohesion.
- **Corner Mask Transparency**: The first-pixel Rockbox transparency key on `wpsArtCorners.bmp` was prone to row-order failure; forced both `(0,0)` and `(0,H-1)` to `FF00FF` (magenta) to guarantee rock-solid corner rendering.

Changed:
- `tools/apple2026_wps_art_frame.py` [NEW]
  - Python generator for two album art framing assets.
  - `wpsArtShadow.bmp` (158x160): soft drop shadow backdrop with 6px rounded rect, 2px downward offset, 3px gaussian blur, 11% opacity black on F2F2F2 background.
  - `wpsArtCorners.bmp` (150x150): corner mask overlay with 6px radius arcs. Transparent center (Rockbox first-pixel key = F1F1F1), F2F2F2 corner wedges that mask album art edges to create rounded corners.
  - Parameters extracted from Apple Music Now Playing reference SVG (`iOS Stocks 07` Figma export): `rx=6` on 358px => 6px on 150px, `feOffset dy=2`, `feGaussianBlur stdDeviation=4` => 3px at target res, `feColorMatrix opacity=0.12` => 11%.
- `wps/Apple2026/wpsArtShadow.bmp` [NEW]
  - 158x160 pre-rendered soft shadow backdrop.
- `wps/Apple2026/wpsArtCorners.bmp` [NEW]
  - 150x150 corner mask overlay with 31 corner pixels in a smooth arc.
- `wps/Apple2026.wps`
  - Added `%xl(K,wpsArtCorners.bmp,0,0)` preload for corner mask.
  - Added shadow backdrop viewport `%V(81,4,158,160,-)%VB` behind album art with `%x(ArtShadow,wpsArtShadow.bmp,0,0)` -- positions the 158x160 shadow canvas so the 150x150 art area sits centered within it.
  - Added corner mask overlay viewport `%V(85,8,150,150,-)` with `%xd(K)` after `%Cd` -- overlays the transparent-center corner mask on top of album art to create rounded corners.
  - Album art viewport position and size (85,8,150,150) unchanged -- no layout shift.

SVG Reference Analysis:
- Album art in Apple Music reference uses `rx=6` corner radius on 358px element (~1.7% proportional).
- Drop shadow: `feOffset dy=2` (barely offset downward), `feGaussianBlur stdDeviation=4` (soft diffuse spread), 12% opacity black (extremely subtle).
- Visual character: ambient depth, NOT a visible shadow edge. Art sits calmly elevated. No border, glow, or card treatment.
- Scaled for 320x240 target: 6px radius, 2px offset, 3px blur, 11% opacity.

Verified:
- Static: `python tools/apple2026_skin_audit.py --source-only` -> `OK`
- Asset: `wpsArtShadow.bmp` (158x160, RGB, F2F2F2 background with soft shadow visible only against pure white)
- Asset: `wpsArtCorners.bmp` (150x150, RGB, 31 corner fill pixels in smooth arcs, first-pixel transparency key at (0,0) = F1F1F1)
- Layout: Album art position unchanged at (85,8,150,150). Shadow viewport at (81,4,158,160) provides 4px padding all sides + 2px extra bottom for offset. No metadata/progress bar shift.

Docs:
- `WORK_LOG.md`

Next:
- Build and install: `.\build-sim.ps1 -Incremental` or `-InstallOnly`
- Visual verification in simulator: confirm rounded corners are gentle/Apple-like, shadow is barely perceptible, no layout shift in metadata area
- Compare against the reference screenshot to validate "this is the right amount of polish"
- If shadow feels too strong or too weak on target LCD, adjust `SHADOW_OPACITY` in generator and regenerate

## 2026-04-13 (Apple2026 Shell Polish Pass: Chevrons, White Tone, Lockscreen)

Changed:
- `apps/menu.c`
  - Fixed a navigation-arrow regression where chevrons successfully appeared on `MT_MENU` submenus, but missing on `MT_RETURN_VALUE` (e.g. Music, Database) and `MT_FUNCTION_CALL` (e.g. Files) items that make up the root and extras menus.
  - Expanded `menu_item_is_navigable` to return true for these functional items, immediately restoring correct chevron rendering on all list menu items while safely omitting them on pure setting variants and toggles.
- `wps/Apple2026.wps` and `wps/Apple2026.sbs`
  - Warmed the harsh cool-white cast (historically problematic on Apple LCD hardware) from pure `#FFFFFF` to a soft, tasteful warm-white `#FAFAF8`.
  - Harmonized the secondary backdrop gray from a cool `#F2F2F7` to a slightly warmer, balanced neutral `#F2F2F2`.
  - Permanently simplified the lockscreen architecture: deleted the buggy, pseudo-notification lockscreen overlay geometry completely from both `.wps` and `.sbs`. Engaging the hold switch now lets the system default behavior take hold natively: no overlay blocks the current menu or WPS surfaces, the UI simply dims/locks gracefully alongside the top-right `lock` indicator icon.

Verified:
- Static: `menu.c` now evaluates `MT_RETURN_VALUE` | `MT_FUNCTION_CALL` correctly without drawing chevrons on context toggles.
- Static: `Apple2026.wps/.sbs` replaced all `FFFFFF` geometry backdrops/text with `FAFAF8` and `F2F2F7` with `F2F2F2`.
- Static: The complex `lock_notif`, `lock_artist` and `home_lock_*` viewport structures have been deleted, leaving only lock icons and simplified status header routing.

Next:
- Monitor next build/sim for warm-white hardware visibility (color representation on target device LCD matrix vs 888 sim).
- Interact with hold-switch locally to confirm lockscreen simplification feels as native and non-blocking as expected without the custom pseudo-card.


## 2026-04-12 (Lockscreen disablement for stability - Simplified approach)

Changed:
- Branch: Renamed local branch from `Update3` to `BetaUpdate1` to match GitHub.
- `wps/Apple2026.sbs`
  - Reset to known-good version from `public/BetaUpdate1`.
  - Disabled homescreen lockscreen graphics by commenting out the draw line (line 86).
  - Maintained core SBS structure and header indicator logic for maximum stability.
- `DESIGN_SYSTEM.md`
  - Updated §14A to reflect that the lockscreen graphics are currently disabled for stability.

Verified:
- Static: SBS code reset; only the lockscreen draw viewport block is commented out.
- Build: `.\build-hw.ps1 -Target ipod6g -Incremental` -> `OK`.
- Build: `.\build-hw.ps1 -Target ipodvideo -Incremental` -> `OK`.
- Audit: `python tools/apple2026_skin_audit.py` -> `OK`.

Next:
- Monitor for UI glitches on hold switch in this minimal configuration.
- Investigate true root cause of lockscreen freezing in a future branch.


## 2026-04-12 (Apple2026 plugin / missing-file / packaging audit hardening)

Changed:
- `tools/buildzip.pl`
  - Fixed CRLF-sensitive `apps/plugins/CATEGORIES` parsing so plugin category
    moves no longer target bogus directories like `rocks/apps\r/`.
  - Added explicit move helpers that create destination directories and fail the
    package step immediately if a plugin move cannot be completed.
- `tools/verify_plugin_package.py`
  - Added a package audit that compares built `apps/plugins/*.rock` outputs to
    the final simulator install tree or hardware `rockbox.zip`.
  - Checks category/viewer placement plus key plugin sidecars
    (`disktidy.config`, `open_plugins.opx`, `snake2.levels`, `sokoban.levels`,
    `pictureflow` bitmaps, `lua/actions.lua`, Picross defaults).
- `tools/ovl_offset.pl`
  - Fixed CRLF-sensitive map parsing so overlay link scripts no longer inherit
    literal `Could not analyze map file.` text on Windows-generated `.refmap`
    files.
  - Moved failure output to `stderr` and now include the map filename in the
    error.
- `build-sim.sh`
  - Now runs `tools/verify_plugin_package.py` against
    `build-sim/simdisk/.rockbox` after `make install`.
- `build-hw.sh`
  - Now uses `set -e` so the script stops on the first failing command instead
    of continuing into zip injection with a broken build.
  - Deletes stale `rockbox.zip` before rebuilding it so failed incremental runs
    cannot leave a misleading previous archive behind.
  - Now runs `tools/verify_plugin_package.py` against the generated
    `rockbox.zip`.
- `apps/menus/main_menu.c`
  - `Main Menu` settings entry now resolves `main_menu_config.rock` by actual
    filesystem presence, prefers the correct `rocks/apps/` path, falls back to
    legacy `rocks/`, hides the item if absent, and shows a normal plugin error
    splash instead of surfacing a raw load failure.
- `apps/onplay.c`
  - `Playing Time` now resolves `playing_time.rock` by actual presence, prefers
    `rocks/apps/`, hides the item if absent, and fails gracefully.
  - `View Album Art` now checks for `imageviewer.rock` first, hides the item if
    missing, and uses a normal plugin error splash instead of a raw load
    failure.
- `apps/menus/settings_menu.c`
  - `Select Database Dirs` now resolves `db_folder_select.rock` by actual
    presence, hides the menu item if absent, and splashes a normal plugin error
    instead of loading a broken path.
  - `Autoresume -> custom` now rolls back to the previous value if
    `db_folder_select.rock` is unavailable.

Verified:
- Root-cause chain for the original `main_menu_config.rock` error:
  - `apps/plugins/main_menu_config.c` was built for simulator, 6G, and 5G.
  - `apps/menus/main_menu.c` expected `main_menu_config.rock` under
    `/.rockbox/rocks/apps/`.
  - `tools/buildzip.pl` was parsing CRLF `apps/plugins/CATEGORIES` lines
    without stripping `\r`, so category moves silently failed and many app/demo/
    game plugins never reached their final packaged paths.
  - Before the fix, simulator install and hardware zip audits both showed
    missing/misplaced plugins including `main_menu_config.rock`,
    `playing_time.rock`, `alarmclock.rock`, `dict.rock`, `stats.rock`,
    `random_folder_advance_config.rock`, and many more.
- Broader packaging class confirmed:
  - simulator runtime had only a small subset of `rocks/apps/*.rock`
  - both hardware zips had many category plugins stranded at `rocks/*.rock`
    instead of `rocks/apps|games|demos/...`
- Additional Windows-specific release blocker confirmed:
  - `tools/ovl_offset.pl` was also CRLF-sensitive; on freshly regenerated
    6G overlay refmaps it emitted `Could not analyze map file.` into generated
    linker scripts, breaking SDL/imageviewer overlays and leaving a partial
    `rockbox.zip`.
  - `build-hw.sh` previously kept running after those build failures and still
    injected `config.cfg` / fonts, which made the broken 6G zip look superficially
    valid.
- Current rebuilt artifacts:
  - `python tools\verify_plugin_package.py --build-dir build-sim --package-root build-sim\simdisk\.rockbox` -> `Result: OK`
  - `python tools\verify_plugin_package.py --build-dir build-hw-ipod6g --zip-artifact build-hw-ipod6g\rockbox.zip` -> `Result: OK`
  - `python tools\verify_plugin_package.py --build-dir build-hw-ipodvideo --zip-artifact build-hw-ipodvideo\rockbox.zip` -> `Result: OK`
  - `powershell -ExecutionPolicy Bypass -File .\tools\verify-sim-install.ps1` -> `Result: OK`
  - Clean hardware rebuilds completed successfully for both `ipod6g` and
    `ipodvideo` after the overlay parser fix.
  - Incremental follow-up rebuilds completed successfully for sim, 6G, and 5G
    after the menu fallback changes.
- Artifact spot checks:
  - `build-sim/simdisk/.rockbox/rocks/apps/main_menu_config.rock` exists
  - `build-sim/simdisk/.rockbox/rocks/apps/playing_time.rock` exists
  - `build-hw-ipod6g/rockbox.zip` contains:
    - `.rockbox/rocks/apps/main_menu_config.rock`
    - `.rockbox/rocks/apps/playing_time.rock`
    - `.rockbox/rocks/apps/random_folder_advance_config.rock`
    - `.rockbox/rocks/viewers/open_plugins.rock`
    - `.rockbox/rocks/apps/disktidy.config`
  - `build-hw-ipodvideo/rockbox.zip` contains the same checked paths.
- Remaining audit note:
  - `h264_poc.rock` is built on 6G but is not mapped by `viewers.config` or
    `CATEGORIES`; the new audit reports this as a warning only, and it is not a
    known menu-exposed Apple2026 failure in this pass.

Docs:
- `BUILD.md`
- `OpusAuditPriorityProgress.md`
- `WORK_LOG.md`

Next:
- Retest the user-facing plugin surfaces on current artifacts:
  - Settings -> Main Menu
  - WPS on-play -> Playing Time
  - Settings -> Select Database Dirs
  - WPS on-play -> View Album Art
- On device, replace the `.rockbox` tree from the freshly rebuilt
  `build-hw-ipod6g/rockbox.zip` or `build-hw-ipodvideo/rockbox.zip` instead of
  reusing an older extracted package.
- If a future build shows a plugin missing again, check in this order:
  - `tools/verify_plugin_package.py`
  - whether `build-hw.sh`/`build-sim.sh` stopped on a prior compile error
  - whether the installed device tree came from the current `rockbox.zip`
    instead of a stale extracted `.rockbox`

## 2026-04-12 (PictureFlow tracklist return freeze fix)

Changed:
- `apps/plugins/pictureflow/pictureflow.c`
  - Split PictureFlow tracklist state into:
    - one-time current-song focus on tracklist entry
    - normal user-owned browsing after entry
  - Removed the redraw-time behavior in `show_track_list()` that kept forcing
    `pf_tracks.sel` back to the currently playing file whenever
    `PLAYBACK_CONTEXT_PICTUREFLOW_TRACKLIST` was active.
  - Added a one-shot tracklist-entry focus helper, triggered when entering
    `pf_show_tracks`, so WPS return can still land on the current song without
    pinning the list there permanently.
  - Cleared that one-shot focus state in `select_next_track()` /
    `select_prev_track()` so manual scrolling keeps ownership immediately after
    the first user move.

Verified:
- Source trace confirmed the exact regression path:
  - `wps_handle_browse_parent()` routes WPS back to PictureFlow tracklist via
    the Apple2026 return token.
  - `plugin_start()` re-enters PictureFlow in tracklist mode.
  - `show_track_list()` was then reasserting the playing-track selection on
    every redraw, which overwrote user scroll on the album containing the
    currently playing track.
- Same-album vs different-album explanation is now explicit in source:
  - current album matched `audio_current_track()->path`, so selection was
    forcibly restored every frame
  - different album had no match, so scrolling kept working
- Static validation:
  - `.\build-sim.ps1 -Incremental -SkipDep -StrictInstall` completed successfully
  - `python tools/apple2026_skin_audit.py --source-only` -> `OK`

Docs:
- `OpusAuditPriorityProgress.md`
- `WORK_LOG.md`

Next:
- Runtime-check the exact flow on the next build:
  - Cover Flow -> album tracklist -> WPS -> back -> same album
  - confirm initial focus may land on the current song once
  - confirm up/down immediately moves off that song and remains scrollable
  - confirm different-album tracklists still behave normally
  - confirm selecting another song from the same album still starts playback cleanly

## 2026-04-11 (Apple2026 transport icon mapping swap)

Changed:
- `wps/Apple2026.wps`
  - Swapped the top-left WPS transport-state icon to the iPod-style convention:
    playing now shows the play icon and paused now shows the pause icon.
  - Swapped the WPS lock notification playback-state indicator to the same convention so the lock overlay does not disagree with the main WPS.
- `wps/Apple2026.sbs`
  - Swapped the mini-player transport-state icon to the same iPod-style convention.
  - Swapped the homescreen lock notification playback-state indicator to the same convention.
- `tools/apple2026_skin_audit.py`
  - Added explicit required/forbidden transport-state patterns so the old Apple-style mapping fails the Apple2026 audit if it reappears in source or packaged artifacts.
- `Update1Worklog.md`
  - Marked the older Apple-style transport convention note as superseded.

Verified:
- Static source search found four Apple2026 state-dispatch surfaces using this convention:
  - WPS status icon
  - SBS mini-player status icon
  - WPS lock notification status icon
  - SBS homescreen lock notification status icon
- `python tools/apple2026_skin_audit.py --package-root .does-not-exist` -> `OK`

Next:
- Refresh simulator/runtime and verify the play icon appears while actively playing and the pause icon appears while paused in both the WPS and the SBS mini-player shell.

## 2026-04-11 (Apple2026 build-vs-claims verification audit)

Changed:
- `wps/Apple2026.sbs`
  - Corrected the shipped SBS header contract to match the previously claimed Update 1 fixes:
    battery text now uses slot `6`, numeric battery formatting no longer inserts a space before `%`, and the hold/sleep icons now live in the right-side battery cluster (`lock=-82,6`, `sleep=-94,6`).
- `wps/Apple2026.wps`
  - Corrected the shipped WPS contract to match the previously claimed Update 1 fixes:
    numeric battery formatting no longer inserts a space before `%`, repeat mode now renders one icon per mode (`all`, `one`, `shuffle`, `A-B`) instead of layered composites, and `speaker_mute` is aligned at `y=5`.
- `tools/apple2026_skin_audit.py`
  - Added explicit claim-contract checks for the four audited regressions so source or packaged artifacts fail if old battery formatting, old hold/sleep coordinates, layered repeat logic, or mute `y=8` reappear.
  - Retained package-root and zip drift checks so repo source, sim install, and release zips can be compared directly.
- `build-sim.sh`, `build-hw.sh`
  - Added automatic Apple2026 audit runs after simulator install and after hardware `rockbox.zip` packaging so stale or mismatched artifacts fail in the build pipeline.
- Release artifacts
  - Rebuilt `build-sim/simdisk/.rockbox`, `build-hw-ipod6g/rockbox.zip`, and `build-hw-ipodvideo/rockbox.zip`.
  - Removed stale release zips: `build-hw-ipod6g/rockbox_2026_beta_update1_6G.zip`, `build-hw-ipodvideo/rockbox_2026_beta_update1_5G.zip`, and `build-hw-ipodvideo/rockbox_5G_2026.zip`.

Verified:
- Credible user report confirmed:
  - committed/source Apple2026 skins did not contain the four claimed fixes before this pass
  - simulator runtime matched that unfixed source exactly
  - published custom-named 6G and 5G Update 1 zips shipped a stale Apple2026 skin pair that also lacked the four claimed fixes
  - the older `rockbox_5G_2026.zip` shipped an even earlier Apple2026 variant
- Static audit:
  - `python tools/apple2026_skin_audit.py --package-root .does-not-exist` -> `OK`
  - `python tools/apple2026_skin_audit.py --package-root build-sim/simdisk/.rockbox --zip-artifact build-hw-ipod6g/rockbox.zip --zip-artifact build-hw-ipodvideo/rockbox.zip` -> `OK`
- Build/install:
  - `.\build-sim.ps1 -Incremental -InstallOnly` completed and audited `build-sim/simdisk/.rockbox`
  - `.\build-hw.ps1 -Target ipod6g -Incremental -SkipDep` completed and audited `build-hw-ipod6g/rockbox.zip`
  - `.\build-hw.ps1 -Target ipodvideo -Incremental -SkipDep` completed and audited `build-hw-ipodvideo/rockbox.zip`
- Artifact evidence:
  - both current hardware zips now contain the same corrected Apple2026 skin hashes:
    - `.rockbox/wps/Apple2026.sbs` -> `421acf1c11e3a4b92b555a1bd9f2c7546102c8d3`
    - `.rockbox/wps/Apple2026.wps` -> `402d9fd0bdf72f358e733f6932337d6dd3362695`

Docs:
- `BuildClaimMismatchAudit.md`
- `WORK_LOG.md`
- `Update1Worklog.md`

Next:
- Interactive sim/device confirmation for the four corrected visuals:
  battery text density, right-cluster hold/sleep icons, single repeat-state icon, and mute-rail alignment.
- If a public GitHub release note or issue response references the removed custom-named zips, update that external release text to point to the current `build-hw-*/rockbox.zip` artifacts only.

## 2026-04-11 (Lockscreen system audit and artifact recovery)

Changed:
- `wps/Apple2026.sbs`
  - Fixed double battery/clock indicators during lockscreen by moving all header chrome (battery, text, lock, sleep timer, busy indicator, main title) inside `%?if(%cs, =, 1)<%?mh<|...>>` (suppress on active lockscreen) statements.
  - Resolved black-box rendering artifacts and background list bleeding by routing the list content viewport (`%VI`) to `lock_empty` (1x1 px) during lockscreen (`cs=1 + hold`).
- `wps/Apple2026.wps`
  - Suppressed WPS player status icon, battery icon, and battery text dispatch during hold state by gating them inside `%?mh<|...>`. This ensures cleanly separated draws under the primary lockscreen layout, stopping background bleeding and visual clashes.
- `Apple2026SystemAudit.md`
  - Formally compiled a system-wide audit of UI cohesiveness and stability, and defined the `apple2026-ui-core` future state to drop legacy Skin Engine ties.

Verified:
- Static: `wps` files strictly enforce the `%mh` bounds check.
- CI: Sim environment built successfully (`build-sim.ps1 -Incremental`). Tested with `-InstallOnly` sync pipeline; verification script tests OK.

Next:
- Interactive simulator verification: Launch simulator to confirm single indicators on the lockscreen, and valid navigation out of lockscreen state seamlessly.

## 2026-04-11 (Music vs Files routing audit — curated Music browser + Files in Extras)


Changed:
- `apps/tree.c`
  - Removed the fragile `tc.dirlevel <= 1` constraint from the Apple2026
    Music back-navigation guard.  The guard now relies solely on
    `path_is_curated_music_library_root(tc.currdir)` (exact `/Music` match
    with trailing-slash tolerance) and the `BROWSE_APPLE2026_MUSICLIB` flag.
  - Root cause of the intermittent regression: `dirlevel` could desynchronize
    from `currdir` after stale-path resumes, playback-context redirects, or
    deep navigation via `set_current_file()`.  When `dirlevel` exceeded 1
    at the `/Music/` boundary, the guard did not fire, allowing `ft_exit()`
    to strip `/Music` from currdir and briefly show the raw filesystem root
    (`/`) before the standard root check returned to the main menu.
- `apps/root_menu.c`
  - Added `files_browser` (`GO_TO_FILEBROWSER`) `MENUITEM_RETURNVALUE`
    with `Icon_file_view_menu`.
  - Added `&files_browser` as the first item in `extras_submenu`.
  - Updated the Extras comment to document the architectural split:
    Music = bounded `/Music/` curated browser; Files = standard full-disk
    browser with separate state (`last_folder`).
- `MusicVsFilesRoutingAudit.md`
  - Created a dedicated audit document covering: current behavior, root
    cause analysis, desired behavior, Music vs Files architectural model,
    implementation details, anti-regression safeguards.

Verified:
- The `BROWSE_APPLE2026_MUSICLIB` flag is only set when dispatching
  `GO_TO_MUSICLIB` from `root_menu.c` line 372.  It cannot leak into
  Files or other browse sessions.
- `path_is_curated_music_library_root()` compares trimmed path to exactly
  6 characters `/Music` — it cannot false-positive on `/Music/Artist/` or
  any deeper path.
- `GO_TO_FILEBROWSER` already has full separate state management
  (`last_folder`, no musiclib flag, standard `ft_exit()` back-navigation).
  No new code needed for the Files browser itself.
- The standard Rockbox root check at `!strcmp(currdir,"/")` remains as a
  safety net in `dirbrowse()` for the Files browser's own back behavior.

Docs:
- `MusicVsFilesRoutingAudit.md`
- `WORK_LOG.md`

Next:
- Build (`.\build-sim.ps1 -Incremental -SkipDep`) and verify:
  - Music → artist list → back → main menu (no raw `/` flash)
  - Music → deep album/track → back repeatedly → main menu (never see `/`)
  - Extras → Files → shows full filesystem root → back → Extras
  - Resume Music from WPS → back all the way → main menu (not `/`)
- Update `DESIGN_SYSTEM.md` with the finalized Music vs Files product model.


## 2026-04-11 (Quick Screen regression stabilization)

Changed:
- `apps/gui/quickscreen.c`
  - Removed Apple2026 quickscreen body dependence on the SBS-selected info
    viewport.
  - Added a fixed Apple2026 quickscreen parent viewport below the 30 px shell
    header with explicit font/color setup.
  - Recompute that parent/body geometry on redraw so quickscreen does not keep
    stale viewport state from the prior surface.
- `DESIGN_SYSTEM.md`
  - Updated the quickscreen contract to describe the fixed core body viewport
    instead of a normal themed content viewport.
- `QuickScreenRegressionAudit.md`
  - Added a dedicated root-cause and anti-regression document for the white
    quickscreen failure.

Verified:
- Source-level root cause is now isolated to the viewport handoff ordering:
  - previous code captured the quickscreen parent viewport before `skin_update()`
    switched the SBS quickscreen lane.
  - SBS still painted a white quickscreen shell region, so stale body geometry
    could present as a white unreadable screen.
- The Apple2026 quickscreen body no longer depends on that SBS selection timing.

Docs:
- `QuickScreenRegressionAudit.md`
- `DESIGN_SYSTEM.md`
- `Update1Worklog.md`

Next:
- Build/install the simulator artifacts.
- Re-run quickscreen entry checks from WPS and validate the white-body failure
  is gone.

## 2026-04-11 (Quick Screen fallback rebase to native/interpod baseline)

Changed:
- `apps/gui/quickscreen.c`
  - Removed the active Apple2026-specific quickscreen body renderer and
    viewport layout helpers from the runtime path.
  - Kept only a fixed Apple2026 parent viewport below the shell header, then
    handed body layout/draw back to the native Rockbox quickscreen path.
- `wps/Apple2026.sbs`
  - Removed the dedicated quickscreen body viewport (`qs_content`) from the
    active shell contract.
  - Kept quickscreen shell suppression behavior only, so the SBS no longer
    owns quickscreen body placement.
- `QuickScreenRegressionAudit.md`
  - Documented the fallback rebase decision and the anti-regression rule that
    Apple2026 quickscreen should stay on the native/interpod-style baseline
    until a later rebuild proves itself.
- `DESIGN_SYSTEM.md`
  - Updated the quickscreen rule to state that the current Apple2026 baseline
    is fixed-parent + native body rendering.

Verified:
- `.\build-sim.ps1 -Incremental -SkipDep` completed successfully.
- `.\build-sim.ps1 -InstallOnly` completed successfully.
- `.\tools\verify-sim-install.ps1` returned `Result: OK`.
- Source/runtime contract no longer contains `qs_content` routing in
  `wps/Apple2026.sbs`, and the active Apple2026 quickscreen path in
  `apps/gui/quickscreen.c` now uses `quickscreen_fix_viewports()` plus
  `gui_quickscreen_draw()`.
- Direct simulator window-handle capture works for visual inspection, but this
  CLI pass could not yet drive long-PLAY reliably through the SDL window, so
  final on-screen quickscreen confirmation is still a manual next-build check.

Docs:
- `QuickScreenRegressionAudit.md`
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`
- `Update1Worklog.md`

Next:
- Manually open quickscreen in the simulator/device with long PLAY and confirm
  the native body is now readable under the Apple2026 header.
- If a future Apple2026 custom quickscreen is attempted, layer it on top of
  the native quickscreen body contract instead of replacing that renderer.

## 2026-04-11 (Quick Screen rewrite pass)

Changed:
- `apps/gui/quickscreen.c`
  - Rebuilt Apple2026 Quick Screen as a single core-rendered surface inside
    the themed content viewport instead of letting the SBS and native
    quickscreen draw on top of each other.
  - Added a minimal Apple2026 layout: four text-first directional zones,
    restrained center indicators, and a thin bottom volume rail.
  - Changed redraw sequencing so the SBS refresh happens before Quick Screen
    content draw, removing the prior overlay-order fight.
- `wps/Apple2026.sbs`
  - Removed the custom Quick Screen body, pill-button assets, and the
    `qs_empty` fake-hidden viewport workaround.
  - Added a real `qs_content` viewport so Quick Screen now renders in a
    normal Apple2026 content lane below the compact header.
- `apps/apple2026_shell.h`
  - Added shared Apple2026 shell color tokens used by the rebuilt utility
    surface path.
- `tools/apple2026_lockscreen_assets.py`
  - Dropped Quick Screen pill/arrow assets from the active generation path;
    the current Quick Screen no longer depends on bitmap chrome.

Verified:
- `.\build-sim.ps1 -Incremental -SkipDep` completed successfully after the
  Quick Screen rewrite.
- `.\build-sim.ps1 -InstallOnly` synced the rebuilt Apple2026 assets and SBS
  into the simulator tree.
- `.\tools\verify-sim-install.ps1` returned `Result: OK`.
- Apple2026 source/runtime SBS and WPS copies are current after install sync.

Docs:
- `DESIGN_SYSTEM.md`
  - Replaced the old hidden-viewport Quick Screen rule with the new
    shell-only SBS + core-rendered content rule.
- `Update1Worklog.md`
  - Added a dedicated entry for the Quick Screen rebuild, its failure mode,
    and the new simplified architecture.

Next:
- Interactively verify Quick Screen in the simulator on:
  - entry from stopped state
  - entry while playback is active
  - repeated left/right/top/bottom changes
  - volume adjustment while Quick Screen is open
- Confirm there is no residual black-box flicker, no text collision, and no
  stale header/icon state after sleep-timer or shuffle/repeat changes.

## 2026-04-11 (Apple2026 default-settings first-run pass)

Changed:
- `apps/settings_list.c`
  - Set Apple2026 iPod default `backlight on button hold` to `normal`
    instead of the generic hold-device default `off`.
  - Set Apple2026 iPod compiled default `list separator height` to `1` so
    reset/first-run behavior matches the Apple2026 theme spec.
- `wps/WPSLIST`
  - Aligned the generated Apple2026 theme font from
    `20-SFProText-Regular.fnt` to `18-SFProText-Regular.fnt` so
    `Apple2026.cfg`, `settings_list.c`, SBS slot 5, and the design system
    all describe the same list baseline.

Verified:
- Root cause of the "lockscreen disappears too fast" behavior is confirmed in
  source:
  - `apps/settings_list.c` was defaulting `backlight on button hold` to
    `off` on hold-switch targets.
  - `firmware/backlight.c` maps that setting to timeout `-1` while hold is
    engaged, which turns the backlight off immediately.
  - Apple2026 shell rules already treat `backlight on button hold = off` as
    the minimal AOD path, so the shipped defaults were forcing the lockscreen
    straight into that mode.
- `.\build-sim.ps1 -Incremental -SkipDep` completed successfully.
- `.\build-sim.ps1 -InstallOnly` completed successfully.
- `.\tools\verify-sim-install.ps1` returned `Result: OK`.
- Generated `build-sim/simdisk/.rockbox/themes/Apple2026.cfg` now contains:
  - `font: /.rockbox/fonts/18-SFProText-Regular.fnt`
  - `list separator height: 1`
  - existing Apple2026 Quick Screen and menu-scrolling defaults.
- `build-sim/simdisk/.rockbox/config.cfg` still reports
  `20-SFProText-Regular.fnt`, but this is verified runtime user state, not a
  shipped default.

Docs:
- `DESIGN_SYSTEM.md`
  - Recorded that Apple2026 first-run hold behavior now ships with
    `backlight on button hold = normal`.
  - Synced the menu-list font token to `18-SFProText-Regular.fnt`.
- `Update1Worklog.md`
  - Added the default-settings / first-run pass with verified cause,
    changes, and validation status.

Next:
- On the next live simulator or device pass, close hold from a clean config
  and confirm the full Apple2026 lockscreen remains visible for the normal
  backlight timeout before any AOD transition.
- On that same clean-config pass, confirm first-run list typography and 1 px
  separators match the regenerated Apple2026 theme defaults without manual
  theme re-selection.

## 2026-04-11 (Header regression forensic pass -- current simulator state)

Changed:
- `apps/gui/skin_engine/skin_parser.c`
  - Added simulator parse-failure logging with line/column/message so SBS/WPS
    parse failures can be distinguished from asset-load failures.
- `tools/verify-sim-install.ps1`
  - Added Apple2026 WPS freshness checking alongside the existing SBS
    freshness check.
- `HeaderRegressionAudit.md`
  - Added a dedicated header-regression investigation document with current
    evidence, findings, and anti-regression notes.

Verified:
- Fresh simulator run now shows:
  - `A26 skin_load skin=0 ... Apple2026.sbs loaded=1 fallback=0 failsafe=0`
  - `A26 skin_load skin=1 ... Apple2026.wps loaded=1 fallback=0 failsafe=0`
- `sb_skin_get_info_vp()` in the fresh run returns Apple2026 geometry, not
  the generated fallback viewport.
- Runtime/source copies of both `wps/Apple2026.sbs` and
  `wps/Apple2026.wps` are byte-identical.
- Simulator install verification passes and now checks both SBS and WPS
  freshness explicitly.
- SDL simulator plugin buffer (`0x80000`) matches iPod Video plugin buffer;
  iPod 6G is larger (`0x300000`), so "sim passes but hardware fails because
  less skin memory" is not the leading explanation.

Next:
- If the user still reproduces the broken header, compare device install
  state and runtime config against the simulator before changing theme
  geometry again.
- Continue tracing any remaining root-header startup/routing instability from
  live runtime state rather than from fallback/load assumptions.

## 2026-04-10 - Adwaitapod extraction continuation

Goal: finish the Apple2026 pieces that were still only partially ported
from the Adwaitapod analysis, with emphasis on the root lockscreen and the
custom Quick Screen.

Changed:
- `wps/Apple2026.sbs`
  - Added a real root lockscreen / AOD layer for `ACTIVITY_MAINMENU` when
    hold is engaged.
  - Added Apple2026 quickscreen pills, arrows, `%Q*` labels/values, center
    now-playing summary, and bottom volume rail.
  - Added hidden `lock_empty` and `qs_empty` list viewports so the normal
    root list and hardcoded quickscreen no longer bleed through the overlay.
- `wps/Apple2026.wps`
  - Removed the accidental dependency on `battery display = graphic` for
    lockscreen behavior.
  - Added locale-aware date ordering and a fuller low-battery card.
  - Added AOD artist metadata line.
- `wps/WPSLIST`
  - Added generated theme defaults for `disable main menu scrolling` and
    the Apple2026 Quick Screen assignments.
- `wps/wpsbuild.pl`
  - Extended the generated theme cfg whitelist so those new Apple2026
    `WPSLIST` settings are no longer silently dropped during `make install`.

Audit result before build:
- WPS lockscreen work was already partly present.
- SBS lockscreen and full custom quickscreen were not actually implemented.
- Theme defaults required by the lockscreen/quickscreen behavior were also
  missing.

Next:
- rebuild the simulator, inspect the generated `Apple2026.cfg`, and verify
  that the new SBS/WPS assets and skin files install cleanly.

## 2026-04-10 - Update 1 audit completion pass

Goal: convert documented Update 1 items from "present in diff" to
"actually implemented and internally consistent" before build validation.

Changed:
- `wps/Apple2026.sbs`
  - Quick Screen now suppresses the mini-player and volume strip via a
    `current screen == ACTIVITY_QUICKSCREEN` gate.
  - Quick Screen now uses the real SBS header title lane (`Quick Screen`)
    instead of drawing a title inside the content viewport.
  - Hold and sleep icons moved farther left so they no longer overlap the
    numeric battery text.
- `apps/plugins/pictureflow/pictureflow.c`
  - Tracklist wraparound now blocks on repeated wheel scrolls, which
    matches the main list behavior more closely.
- `apps/root_menu.c`
  - Extras now includes Recording on recording-capable targets.
- `apps/gui/quickscreen.c`
  - Removed the temporary content-overlay title because the SBS header now
    owns the Quick Screen title.

Audit result before build:
- Quick Screen was previously only partially implemented.
- Cover Flow wraparound was previously only partially implemented.
- The first hold/sleep reposition created a new battery-text overlap.
- Extras was previously missing Recording on supported targets.

Next:
- rebuild the Apple2026 fonts so the expanded Unicode range exists at
  runtime, then run simulator build/install verification.

Verification update:
- `python tools/apple2026_rebuild_fonts_from_otf.py` completed
  successfully and rebuilt all 19 Apple2026 `.fnt` files.
- `.\build-sim.ps1 -Incremental -SkipDep` completed successfully.
- `.\tools\verify-sim-install.ps1` returned `Result: OK`.
- Interactive simulator walkthrough and screenshots were not captured in
  this CLI pass, so visual confirmation for the touched screens remains a
  manual follow-up.

Cold-start follow-up:
- Root large-title cropping on fresh sim launch was traced to `%Lo`
  depending on title text before the root title had settled.
- `apps/gui/skin_engine/skin_tokens.c` now treats
  `ACTIVITY_MAINMENU` as root immediately, which should stabilize the
  first Apple2026 root render before any navigation occurs.

---

## 2026-04-09 — Typography hierarchy pass (18pt / 16pt two-tier system)

**Goal:** Establish a robust, context-aware typographic hierarchy across the entire Apple2026 shell.

**Final hierarchy:**

| Role | Font | Size |
|---|---|---|
| Large title ("Library") | SFProDisplay-Bold | 28px |
| Compact header ("Settings", etc.) | SFProText-Semibold | 16px |
| Standard list (menus, settings, DB categories) | SFProText-Regular | **18px** (was 20px) |
| Dense list (songs, track files, playlist viewer, Cover Flow) | SFProText-Regular | **16px** (new) |
| WPS song title | SFProText-Medium | 16px |
| Secondary metadata (artist/album/time) | SFProText-Regular | 14px |

**New architecture — `rockpod_list_font_tier`:**
- Two-tier enum (`NORMAL` / `DENSE`) declared in `root_menu.h`, defined in `root_menu.c`.
- `NORMAL` (18pt, 32px row floor): main menu, settings, database top-level, music artist folders.
- `DENSE` (16pt, 28px row floor): music album/track folders (dirlevel≥2), playlist viewer, Cover Flow tracklist.
- Tier set by `root_menu.c` at surface dispatch + by `tree.c` after each directory load.
- `apps/gui/list.c`: lazy-loads `16-SFProText-Regular.fnt` on first dense access; adjusts row floor per tier in `list_init_item_height`.
- `apps/gui/bitmap/list.c`: switches `lcd_setfont` around `list_draw` when tier is DENSE; restores after.
- `apps/plugins/pictureflow/pictureflow.c`: both tracklist and album-title fonts updated to `16-SFProText-Regular.fnt`.

**Font rebuild:**
- `16-SFProText-Regular.fnt` added to `tools/apple2026_rebuild_fonts_from_otf.py` JOBS.
- Run `python tools/apple2026_rebuild_fonts_from_otf.py` to generate.

**Files changed:**
- `wps/Apple2026.sbs` — slot 5: 20px → 18px Regular
- `apps/settings_list.c` — DEFAULT_FONTNAME: 20-SFProText-Regular → 18-SFProText-Regular
- `apps/root_menu.h` — `rockpod_list_font_tier_t` enum + extern declaration
- `apps/root_menu.c` — definition + setters at GO_TO_ROOT, GO_TO_MUSICLIB, GO_TO_DBBROWSER, GO_TO_PLAYLIST_VIEWER
- `apps/tree.c` — depth-based tier setter after each directory load
- `apps/gui/list.c` — dense font lazy-load + per-tier floor in `list_init_item_height`
- `apps/gui/bitmap/list.c` — `lcd_setfont` font switch around `list_draw`
- `apps/plugins/pictureflow/pictureflow.c` — 16pt for tracklist + album title
- `tools/apple2026_rebuild_fonts_from_otf.py` — added `16-SFProText-Regular.fnt`
- `DESIGN_SYSTEM.md` — §27.1 and §27.1.1 updated

**Build note:** Requires `python tools/apple2026_rebuild_fonts_from_otf.py` then full rebuild.

**Next:** Build + simulator verification. Screenshots of main menu, settings, music folder (track level), and Cover Flow to confirm hierarchy.

---

## 2026-04-09 (Mini-player bar: premature display fix)

### Root Cause Diagnosed
The bottom "What's Playing" bar was appearing before any song was played due to two compounding bugs in `wps/Apple2026.sbs`:

1. **`%mp` branch ordering was inverted** — The `%mp` skin tag is numeric (0=stop, 1=play, 2=pause). `%?mp<A|B>` with two branches selects A when stopped and B when playing — opposite of the SBS comment which described A as "playing/paused". The 44px mini-player (`%Vd(bottombarsmall)`) was in branch A, so it rendered during **stop** state, not play. The stopped-bar (`%?fn<bottombar|>`) was in branch B, so it rendered during **play** state.

2. **Stale `state->id3` made `%fn` always truthy after first play** — `state->id3` in `wps_state` is only cleared in `wps_state_init()` (called on WPS entry), never when audio stops. After any track played and stopped, `%fn` returned the last track filename (truthy), triggering the 36px stopped-bar on every subsequent cold screen — even before any new track was selected.

### Changed
- **`wps/Apple2026.sbs`** — **CRITICAL FIX (inverted branch logic):** Rewrote `%?mp` VI selection and bottom-bar routing lines to use correct 3-branch structure:
  - Branch 0 (stop): `main_full` viewport, no bottom bar
  - Branch 1 (play): `mainlarge` viewport, 44px mini-player
  - Branch 2 (pause): `mainlarge` viewport, 44px mini-player (paused state)
- **`wps/Apple2026.sbs`** — **CRITICAL FIX (stale-id3 gate removed):** Removed `%?fn<bottombar|>` stopped-bar logic entirely. The stopped state now always yields no bar, unconditionally. The `%fn` stale-id3 ghost is permanently eliminated.
- **`wps/Apple2026.sbs`** — **CLEANUP:** Removed dead `main`/`main_lt` (36px-reserved) viewports, `bottombar` (36px stopped-bar background), `albumart` (stopped-bar album art), `currenttrack` / `currenttrackshiftedleft` viewports — all now unreferenced.
- **`wps/Apple2026.sbs`** — Updated KEY BEHAVIOUR header and all routing comments to accurately describe corrected `%mp` branch semantics.

### Verified (static)
- `%?mp<|play|pause>` (line 50): branch 0 is empty (no bar when stopped), branches 1+2 render `%Vd(bottombarsmall)` + mini-player content.
- `%?mp<stop_full|play_large|pause_large>` (line 42): all three branches correctly route to `main_full_*` or `mainlarge_*` viewports.
- No `%fn` or stale id3 dependency remains in bottom-bar visibility path.
- `player_status_bottom` viewport remains defined but is orphaned (never `%Vd`'d); benign, no display effect.

### Behaviour Contract (post-fix)
| State | Bottom bar | Content viewport |
|---|---|---|
| Cold boot / stopped | None | `main_full` (full height) |
| Stopped after play | None | `main_full` (full height) |
| Playing | 44px mini-player | `mainlarge` |
| Paused (resumable) | 44px mini-player | `mainlarge` |
| FF / Rew | 44px mini-player | `mainlarge` (pause branch) |

### Next
- Install-only build to push SBS change to simdisk: `.\build-sim.ps1 -InstallOnly`
- Launch sim, verify cold screen has no bottom bar, confirm bar appears on play start.

---



### Changed
- **`wps/Apple2026.wps`** — **CRITICAL FIX:** Renamed 4 duplicate `%Vl(timeprogresslossless,...)` viewport labels to unique names (`pl_counter`, `elapsed_time`, `lossless_ind`, `remain_time`). All 4 labels were identical, causing the skin parser to match only the first and silently drop elapsed time, lossless indicator, and remaining time displays. Updated `%Vd` dispatch line to call all 4 by their new names.
- **`wps/Apple2026.wps`** — **ANTI-REGRESSION FIX:** Replaced `%x(ArtCorners,wpsArtCorners.bmp,0,0)` with `%xd(ArtCorners)` to comply with anti-regression contract rule #4 (preloaded images must be drawn via `%xd` only, not `%x` after `%xl`).
- **`wps/Apple2026.wps`** — **PARITY FIX:** Restored WPS artist color from `8E8E93` (secondary gray) to `FF2D55` (Apple red accent). DESIGN_SYSTEM specifies `#FF2D55` for active music artist emphasis on WPS; the gray value was inconsistent with the design contract and Interpod DNA.
- **`wps/Apple2026.wps`** — **NON-ASCII FIX:** Replaced 3 em-dash characters (U+2014) in WPS comments with ASCII hyphen-minus. Complies with anti-regression rule prohibiting non-ASCII in WPS files.
- **`apps/root_menu.c`** — **ICON FIX:** Replaced `Icon_Rockbox` with `Icon_Playlist` for the Cover Flow menu item. `Icon_Rockbox` rendered as the house logo, visually wrong for Cover Flow. `Icon_Playlist` is semantically closer (album/track list).
- **`apps/root_menu.c`** — **ICON FIX:** Replaced `Icon_Audio` with `Icon_Config` for the Database menu item. Database and Music were both showing the red music note (`Icon_Audio`), causing identical icons for two different functions on the root menu.
- **`wps/WPSLIST`** — **COMMENT FIX:** Updated Apple2026 comment to reflect actual SBS large-title viewport dimensions (`28px` SF Pro Display Bold, `30px` viewport band at `y=6`, list content at `y=40`) rather than stale "35px / 52px" from an earlier pass.

### Verified
- **`python tools/audit_apple2026_wps.py`** — exit 0, ALL CHECKS PASSED: non-ASCII (0), no duplicate image IDs, all `%xd` refs resolve, all 3 font slots present, all 15 BMP assets present, `%Cl` inside `%VB` viewport.
- Static: `pl_counter` / `elapsed_time` / `lossless_ind` / `remain_time` labels unique; `%Vd` dispatch references all 4.
- Static: `%xd(ArtCorners)` references the `%xl(ArtCorners,wpsArtCorners.bmp,...)` preload correctly.

### Next
- Build sim (incremental): confirm WPS elapsed + remaining times now render; confirm artist appears in Apple red; confirm Cover Flow and Database icons on root menu.
- Check: WPS volume bar still replaces time/progress when adjusting volume.
- Check: Lossless indicator appears only for lossless formats.

---

## 2026-04-09 (Build system / toolchain audit — second pass, deeper)

### Audit findings — second pass

Fresh skeptical audit of the full build pipeline, toolchain, and asset-sync chain.

**Newly confirmed root causes of slowness (concrete):**
1. `prepare_core_generated_headers()` runs `make -j1` on 10 targets **every incremental build**, even when all are up-to-date. On MSYS2/Windows, make process startup is expensive. Adds ~15-25 sec per build regardless of whether headers changed. Added a stamp to skip when sources haven't changed.
2. `make install` runs unconditionally on every incremental build. `wpsbuild.pl` copies all fonts + WPS + icons on every invocation even when nothing changed. On Windows NTFS this takes 30-60 sec. Resolved by `-InstallOnly` path and by `sync_all_fonts()` being fast (skips unchanged files).
3. `make dep` without `ROCKPOD_SKIP_DEP`: adds 30-60 sec when no headers changed.
4. 7,905 tracked text files have CRLF in working copy (stored LF in index). Each `make` run triggers git warnings for any source file touched. These are noise but cause confusion and slow down parsing of build output.
5. `rockpod-msys.ps1` buffers all output to a temp file and replays at end → builds look completely silent until done; Cursor terminal tracking shows `exit_code: unknown` because the shell session doesn't close.

**Newly confirmed fragility (concrete):**
1. `wpsbuild.pl` (called by `make install`) only copies `%Fl`-referenced fonts. Any font used by PictureFlow plugin, fallback WPS, or future skins is silently NOT copied. Root cause of 17 stale / 1 missing fonts found today.
2. `check_asset_freshness()` in `build-sim.sh` only checked 5 hardcoded fonts — missed the entire PictureFlow font set and any newly added fonts.
3. `verify-sim-install.ps1` staleness check used `rockboxui.exe` mtime as reference, but the binary is built BEFORE `make install` — fonts installed by the same build would always appear "newer than binary" and trigger false [WARN] spam.
4. `22-SFProText-Medium.fnt` was completely absent from runtime — not referenced by any `%Fl` directive, never copied by `wpsbuild.pl`. No automated check caught it.

**Runtime state at start of this session:**
- 17 of 18 repo fonts: STALE (fonts regenerated at 23:02 but `make install` not re-run)
- `22-SFProText-Medium.fnt`: MISSING from runtime
- Apple2026.sbs/.wps: current (manually synced timestamps matched)
- config.cfg font: correct (`20-SFProText-Regular.fnt`)

### Changed
- **`build-sim.sh`**:
  - `check_asset_freshness()` expanded from 5 to 18 fonts (full Apple2026 JOBS list + all size variants)
  - Added `sync_all_fonts()` — copies all `fonts/*.fnt` from repo to runtime after `make install`, filling the `wpsbuild.pl` gap; runs on every build and install-only path
  - Added `--install-only` / `ROCKPOD_INSTALL_ONLY=1` mode: skips configure/dep/compile; runs `make install` + `sync_all_fonts()` + `check_asset_freshness()` only (~30-40 sec vs 2+ min)
  - Added `.rockpod_install_stamp` touch after successful `make install` (used by verify script for accurate staleness reference)
  - Added `.rockpod_headers_stamp` to skip `prepare_core_generated_headers()` when lang/firmware/export sources haven't changed (saves ~15-25 sec per incremental build when headers are current)
- **`build-sim.ps1`** — added `-InstallOnly` switch (passes `ROCKPOD_INSTALL_ONLY=1` to `build-sim.sh`)
- **`rockpod.ps1`** — added `-InstallOnly` to `sim` command; updated help text
- **`tools/verify-sim-install.ps1`**:
  - Added `22-SFProText-Medium.fnt`, `18-SFProText-Regular.fnt`, `19-SFProText-Medium.fnt` to required checks
  - Staleness check now uses `.rockpod_install_stamp` as reference time (falls back to binary mtime when stamp absent)
  - Fixed PowerShell strict mode issue (`$staleFonts.Count` on non-array result)
- **`BUILD.md`** — updated env vars table (`ROCKPOD_INSTALL_ONLY`), fast iteration section, fast install-only section, font-regen workflow, common issues table; added install stamp explanation
- **`build-sim/simdisk/.rockbox/fonts/`** — manually synced 13 stale + 1 missing font from repo (13-SFCompactText-Regular, 15-SFProText-Medium/Semibold, 17-SFProText-Bold, 18-SFProText-Regular, 19-Inter-V-padded, 19-SFProText-Medium/Semibold, 22-SFCompactDisplay-Semibold, 22-SFProText-Medium [was missing], 22-SFProText-Regular, 25-SFProDisplay-Bold, 35-SFProDisplay-Bold)

### Verified
- `.\tools\verify-sim-install.ps1` → **Result: OK** — all 17 required assets [ok], all repo fonts current with last install stamp, Apple2026.sbs runtime current
- `.\build-sim.ps1 -InstallOnly` runs in ~37 sec (vs 125 sec full incremental build)

### Next
- After any C source changes: `.\build-sim.ps1 -Incremental -SkipDep`
- After font/WPS/icon-only changes: `.\build-sim.ps1 -InstallOnly` (fastest path)
- After running `tools/apple2026_rebuild_fonts_from_otf.py`: always follow with `.\build-sim.ps1 -InstallOnly`
- CRLF warning flood: still present (7,905 files have CRLF in working copy). Not blocking, but consider a `.gitattributes` normalization pass if it becomes unmanageable.

---

## 2026-04-08 (Picture Flow — album/artist + tracklist font sizes)

### Changed
- **`fonts/18-SFProText-Regular.fnt`** — from **`SF-Pro-Text-Regular.otf`** (`tools/otf_to_rb12_fnt.py`, track **0.40**); **`tools/apple2026_rebuild_fonts_from_otf.py`** JOBS updated.
- **`apps/plugins/pictureflow/pictureflow.c`** — tracklist **`18-SFProText-Regular.fnt`**; album/artist **`19-SFProText-Medium.fnt`** (slightly larger than tracks); **`pf_album_block_px`** trailing gap **+6 → +3** for less padding above the tracklist.

### Verified
- **`fonts/APPLE2026_FONT_ANALYSIS.md`** regenerated.

---

## 2026-04-09 (Apple2026 UX — Music Left: path match, not dirlevel)

### Changed
- **`apps/tree.c`** — Left at **Music** library root: **`paths_same_directory(tc.currdir, tc.browse->root)`** with **`browse->root`** under **`/Music`**, so **`GO_TO_ROOT`** runs before **`ft_exit()`**. **`ft_exit()`** strips **`/Music/`** to **`/`** (`filetree.c` when the last segment is removed). Prior **`dirlevel == 0`** never matched (**`set_current_file("/Music/")`** yields **`dirlevel == 1`**); **`dirlevel`** alone can also desync from **`currdir`**.
- **`DESIGN_SYSTEM.md`** — §**21A** Left behavior: path equality, not **`dirlevel`**.

### Next
- Sim: **Music** → list at **`/Music/`** → **Left** → main menu (no raw **`/`**).

---

## 2026-04-09 (Apple2026 — Music Left audit: library-only anchor)

### Audit
- **`ft_exit()`** (`filetree.c`): from **`/Music/`**, **`i==1`** sets **`currdir`** to **`/`** — confirms intercept-before-**`ft_exit()`** for **`/Music/`** only.
- **Regression fixed:** **`browse->root`** prefix **`/Music`** was too broad — **`/Music/Artist/`** (resume **`last_music_folder`**) must **not** **`GO_TO_ROOT`** on first Left; use **`path_is_curated_music_library_root()`** (exact **`/Music`** after trailing-slash trim).
- **`Icon_Audio` unsuitable:** enum value **0** matches default-zero **`browse_context.icon`** in many callers — use **`BROWSE_APPLE2026_MUSICLIB`** in **`tree.h`** / **`root_menu.c`** instead.

### Changed
- **`apps/tree.h`**, **`apps/root_menu.c`**, **`apps/tree.c`** — explicit browse flag for main-menu Music; Apple2026 Left guard uses **`path_is_curated_music_library_root()`** + **`paths_same_directory`**.
- **`DESIGN_SYSTEM.md`** — §**21A** resume vs default **`/Music/`** + flag vs Storage.

---

## 2026-04-09 (Music page first-open blank-top gap fix)

### Changed
- **`apps/gui/list.c`** — `list_display_title()` now returns `false` for `ROCKPOD_APPLE2026_IPOD`, so Apple2026 never reserves an in-list title row. SBS/large-title remains the only title system.

### Why
- First entry into Music could temporarily reserve a hidden title line (top blank area), showing 5 rows instead of 6 until navigation refreshed title handoff state.

### Next
- Sim verify: open Music from root first time now shows full row count immediately; back/forward no layout jump.

---

## 2026-04-08 (Wave3_V3 — Apple2026 shell density + Cover Flow typography/tracklist)

### Changed
- **`apps/gui/list.c`** — **`ROCKPOD_APPLE2026_IPOD`** row floor **35px** (was 52px target).
- **`wps/Apple2026.sbs`** — list **slot 5** **`20-SFProText-Regular.fnt`**; large-title band **52px**; **`main_lt` / `mainlarge_lt`** start **y=56**; volume strip viewports moved up (**`-32`/`-30`/`-35`**).
- **`wps/WPSLIST`**, **`apps/settings_list.c`** — default list font **`20-SFProText-Regular.fnt`**.
- **`fonts/20-SFProText-Regular.fnt`** — built via **`tools/apple2026_rebuild_fonts_from_otf.py`** / **`otf_to_rb12_fnt.py`** (same pipeline as 22pt Regular).
- **`apps/plugins/pictureflow/pictureflow.c`** — load **`18-SFProText-Regular`** (tracklist) + **`19-SFProText-Medium`** (album/artist); **`track_list_yh`** uses **`pf_album_block_px`**; **`show_track_list`** uses **`list_row_h`** + compact track font; **`draw_album_text`** uses title font + restore **`FONT_UI`**.
- **`DESIGN_SYSTEM.md`** — list font / row floor text synced.

### Verified
- Static: Picture Flow **`font_load`** paths match shipped **`fonts/*.fnt`** names; **`Apple2026.wps`** not edited (density pass = SBS + list + plugin).

### Next
- Sim (`BUILD.md`): Library ≥4 rows, submenu 5, volume position, CF titles smaller, tracklist ≥6 lines; screenshot **`build-sim/runtime_captures`**.

---

## 2026-04-09 (Apple2026 UX — CF idle → home; Music root back; no / junk)

### Changed
- **`apps/root_menu.c`** — **`pictureflow_scrn`:** **`PLUGIN_OK`** → **`GO_TO_ROOT`** (idle Cover Flow back always **main menu**, not **`GO_TO_PREVIOUS`** / WPS).
- **`apps/tree.c`** — **5 / 71:** **`ACTION_STD_CANCEL`** at **`/Music`** or **`/Music/`** → **`GO_TO_ROOT`** (curated library root; no fall-through to raw **`/`**).
- **`DESIGN_SYSTEM.md`** — §**21A** + Cover Flow stack note.

### Verified
- Static: **`PLUGIN_OK`** is **`0`**; matches **`case PLUGIN_OK`**. Music root path matches **`root_menu.c`** **`/Music/`** browse.

### Next
- Sim: CF from WPS → idle → back → **main menu**; Music → artists → back → back → **main menu** (no **/** junk).

---

## 2026-04-09 (Apple2026 — full-bleed list selector + dividers)

### Changed
- **`wps/Apple2026.sbs`** — **`%Vi(main*)`** viewports use **full LCD width** (`x=0`, default width) so the grey **selector bar** and **list separator** lines span edge-to-edge.
- **`apps/gui/bitmap/list.c`** — **`A26_LIST_CONTENT_INSET` (16px)** added to row `indent` on **`ROCKPOD_APPLE2026_IPOD`** so icons/labels stay aligned with the **Library** large-title margin; **disabled** icon-column separator masking so hairlines reach the **right edge**.
- **`apps/apple2026_shell.h`** — **`A26_LIST_CONTENT_INSET`**.
- **`wps/WPSLIST`** — comment updated.

### Verified
- Logic: `line.c` selector/`separator` use viewport `getwidth()`; full-width `%Vi` widens that region; chevrons still use `vp_w - 10` from the right.

### Next
- Sim: confirm scrollbar + inset still look correct.

---

## 2026-04-09 (Wave 4 — WPS chord contract: MENU short = browse back, long PLAY = quickscreen)

### Changed
- **`apps/gui/wps.c`** — `wps_handle_browse_parent()` shared by **`ACTION_WPS_BROWSE`**; removed long-**PLAY** → Picture Flow; **`ACTION_WPS_MENU`** = main menu only (repeat); party mode blocks **`ACTION_WPS_QUICKSCREEN`** like other overlays.
- **`apps/keymaps/keymap-ipod.c`** — **MENU short** → `ACTION_WPS_BROWSE`; **MENU repeat** → `ACTION_WPS_MENU` (home); **PLAY repeat** → `ACTION_WPS_QUICKSCREEN` (replaces `ACTION_WPS_STOP`); main + remote WPS tables and header comment.
- **`DESIGN_SYSTEM.md`** — §**21A** + WPS matrix row updated for Apple2026 WPS controls.

### Verified
- Static review; sim build per **`BUILD.md`** when convenient.

### Next
- Device/sim: MENU short vs MENU long; long PLAY opens quickscreen; wheel still volume; tree **PLAY repeat** still **`ACTION_TREE_STOP`** (non-WPS).

---

## 2026-04-08 (Wave 4 UX — context stack: WPS return + playlists + PF tracklist)

### Changed
- **`apps/settings.h`** — `PLAYBACK_SOURCE_PLAYLIST_VIEWER` for queue / playlist viewer starts.
- **`apps/tree.c`** — `SHOW_M3U` (playlist catalog) sets `PLAYBACK_SOURCE_PLAYLIST_BROWSER` when starting playback from the tree.
- **`apps/gui/wps.c`** — `ACTION_WPS_BROWSE`: full `playback_source` switch (PF, DB, Music, files, playlists, playlist viewer) before `wps_select_action`.
- **`apps/playlist_viewer.c`** — `playback_source_set(PLAYBACK_SOURCE_PLAYLIST_VIEWER)` on `playlist_start` from viewer / search.
- **`apps/plugins/pictureflow/pictureflow.c`** — resume from WPS with PF source + active audio opens **tracklist** via `skip_animation_to_show_tracks()`.
- **`DESIGN_SYSTEM.md`** — §21A updated for the Apple2026 context-stack contract.

### Verified
- Static review only in this session; full sim build per **`BUILD.md`** (MSYS) recommended.

### Next
- Simulator: DB → WPS → SELECT returns to database; Files outside `/Music` → SELECT returns to previous file tree; playlist catalog → Playlists; queue start → SELECT returns to playlist viewer; PF → WPS → SELECT lands on tracklist.

---

## 2026-04-08 (UI audit + polish — DESIGN_SYSTEM sync, shell chrome, Cover Flow selector)

### Changed
- **`DESIGN_SYSTEM.md`** — Reconciled §**27.1**, §**27.1.1**, §**54.1**, §**54.3**, §**54.4**, §**54.6**, §**54.7** with **live Figma `1:4008`** (via MCP) and current tree: **22-SFProText-Regular**, **52px** row floor, **30×30** tiles, **`ICON_PADDING 4`**, **16/56** large-title band, list **y=60**, selector **E5E5EA**, WPS coordinates from **`wps/Apple2026.wps`**.
- **`wps/Apple2026.sbs`** — Removed header **`player_status`** strip (playback state remains **mini-player** / bottom bar only; less stock-Rockbox header noise).
- **`apps/plugins/pictureflow/pictureflow.c`** — **`draw_gradient`** selection fill **D0D0D4 → E5E5EA** to match shell list selector.

### Verified
- **Figma MCP `get_design_context`** (`JyDHVsT9bqCPjbGEfyUD8d`, `1:4008`): **52px** rows, **16px** inset, **29px** icon cell, **22px Regular** labels, disclosure/chevron tertiary styling.
- Static: **`%xl(P,…)`** still used by **`player_status_bottom`** in SBS.

### Docs
- This entry; **`DESIGN_SYSTEM.md`** as contract update (no duplicate build steps).

### Next
- Simulator: confirm root Library (no double playback glyph), Cover Flow tracklist selection vs main menu selector, stopped vs playing bottom bar.

---

## 2026-04-08 (Library shell parity — verify + sync assets, no churn on good bits)

### Changed
- **`tools/apple2026_symbol_assets.py`** — ran **`python tools/apple2026_symbol_assets.py --supersample 2`** so **`icons/Apple2026Icons.bmp`** matches **30×30** tiles already in code (was stale vs earlier edit).
- **`apps/menus/theme_menu.c`** — **`reset_color()`** on iPod Video/Classic sets **lss/lse** to **E5E5EA** to match **WPSLIST** selector (was generic **`LCD_DEFAULT_LS`** / **`LCD_DEFAULT_BG`**).

### Verified
- **`fonts/22-SFProText-Regular.fnt`** present; list font + **16px** SBS **`%Vi`** inset + **52px** row floor left as-is.
- Font analysis refreshed via **`tools/analyze_rb12_fonts.py`**.

### Next
- Simulator: confirm Library list + icons after theme reload or **`Reset theme colors`** if testing reset path.

---

## 2026-04-08 (Font + symbol regeneration pipeline — analysis + OTF rebuild + supersampled icons)

### Changed
- **`tools/rb12_metrics.py`** — shared RB12 parser + ink/digit metrics for tooling.
- **`tools/analyze_rb12_fonts.py`** — scans `fonts/*.fnt`, writes **`fonts/APPLE2026_FONT_ANALYSIS.md`** (height, ascent, digit span, ink samples).
- **`tools/apple2026_rebuild_fonts_from_otf.py`** — rebuilds Apple2026 SF **`fonts/*.fnt`** from **`Apple Fonts/SF Pro`** and **`Apple Fonts/SF Compact`** via **`tools/otf_to_rb12_fnt.py`** with per-face **`--track`** / **`--space-extra`** (Inter donor fonts unchanged).
- **`tools/apple2026_symbol_assets.py`** — internal **`supersample` (default 2×)** raster before final resize for sharper glyphs; **`--supersample`**, **`--analyze`** CLI; regenerated **`icons/Apple2026Icons.bmp`** and **`wps/Apple2026/*.bmp`**.
- **`Apple Fonts/README.txt`** — documents rebuild + symbol commands.

### Verified
- `python tools/apple2026_rebuild_fonts_from_otf.py` — **14** SF `.fnt` rebuilt (OTFs present).
- `python tools/analyze_rb12_fonts.py` — report written.
- `python tools/apple2026_symbol_assets.py --supersample 2 --analyze` — exit **0**; per-tile ink stats printed.

### Next
- On-menu visual check after firmware picks up new **`22-SFProText-Medium.fnt`** (line height now **27** px font box vs prior build).

---

## 2026-04-08 (Wave3_Overhaul — aggressive Apple Music shell + WPS layout)

### Changed
- **Branch:** `Wave3_Overhaul`.
- **`apps/gui/list.c`** — Apple2026 row floor **`34 → 48`** px (closer to DESIGN_SYSTEM library rhythm on 320px-wide UI).
- **`apps/gui/bitmap/list.c`** — **`ICON_PADDING 12 → 15`**; chevron **`4×9 → 5×10`**, right inset **`8 → 10`**.
- **`tools/apple2026_symbol_assets.py`** — menu tiles **`34×30 → 38×34`**, glyph **`scale 1.0 → 1.06`**; regenerated **`icons/Apple2026Icons.bmp`** and **`wps/Apple2026/*.bmp`**.
- **`wps/Apple2026.sbs`** — large-title band **`y=14,h=46 → y=4,h=56`** (stronger top anchor; list still starts **`y=60`**).
- **`wps/Apple2026.wps`** — title **left** + playlist **right** on one row; **19pt** red artist, **16pt** album; progress/time nudged to **y=224/228**; font slots **10/11** for artist/album.
- **`wps/WPSLIST`** — comments aligned to new row/icon tokens.
- **`DESIGN_SYSTEM.md`** — §27 / §54.3 / §54.4 tokens (48px rows, 68px icon column, chevron, visible-row notes).

### Verified
- `python tools/apple2026_symbol_assets.py` exit **0**.
- **Not run:** `make` / simulator (run per **BUILD.md** on MSYS).

### Next
- Simulator: main menu density, icon crispness, Library title, WPS title/counter/artist hierarchy, album art with embedded covers.

---

## 2026-04-08 (Apple UI polish — list rhythm, SF Pro stack, tracking, hairlines)

### Changed
- **`apps/gui/list.c`** — Apple2026 row floor **`30 → 34`** px (Music-like vertical spacing).
- **`apps/gui/bitmap/list.c`** — **`ICON_PADDING 10 → 12`**; disclosure chevron inset **`6 → 8`** px from viewport right.
- **`apps/settings_list.c`** — default list font **`22-SFProText-Medium`**, default separator **`#C7C7CC`** (iOS hairline).
- **`apps/menus/theme_menu.c`** — reset path separator color aligned to **C7C7CC**.
- **`wps/WPSLIST`**, **`wps/Apple2026.sbs`**, **`wps/Apple2026.wps`** — list font + SBS/WPS slots use **SF Pro Text**; **`list separator height: 1`**, color **c7c7cc**.
- **`tools/otf_to_rb12_fnt.py`** — **`--track`** (global letter-spacing) and **`--space-extra`** (word spacing); regenerated key **`fonts/*.fnt`** with light **`--track`** (0.2 Display Bold, 0.25 Text).
- **`tools/apple2026_font_prep.py`** — SF Pro Text **`sizes`** extended through **22**.
- **`DESIGN_SYSTEM.md`** — §27 / §54.3 tokens (34px rows, 12px icon padding, 58px icon column, chevron inset).

### Notes
- TrueType **pair kerning** (GPOS) is still **not** in Rockbox; advance + bearing + optional tracking approximates polish.

### Verified
- `python tools/otf_to_rb12_fnt.py … --track …` exit 0 for regenerated fonts.

---

## 2026-04-08 (35px Library title font — OTF raster, not `.fnt` upscale)

### Changed
- **`tools/otf_to_rb12_fnt.py`** — FreeType-based RB12 `.fnt` generator from OTF/TTF (same packing idea as `convttf`; no bitmap scaling of an existing `.fnt`).
- **`fonts/35-SFProDisplay-Bold.fnt`** — regenerated from **`Apple Fonts/SF Pro/SF-Pro-Display-Bold.otf`** (`python tools/otf_to_rb12_fnt.py … -p 35`).
- **`tools/scale_rb12_fnt.py`** — removed (upscaling path deprecated).
- **`wps/Apple2026.sbs`**, **`DESIGN_SYSTEM.md`** — docs now reference `otf_to_rb12_fnt.py` / official SF Pro source.
- **`tools/apple2026_font_prep.py`** — SF Pro Display `sizes` extended to **35** for future `convttf` batch runs.
- **`Apple Fonts/README.txt`** — clarifies SF Pro is **vendored in-repo** under `Apple Fonts/SF Pro/`; Apple’s fonts page is reference for license/updates.

### Verified
- `python tools/otf_to_rb12_fnt.py …` exit 0; header reports `h=43`, `ascent=34`, `maxw=34`.

### Next
- Optional: run **`tools/apple2026_font_prep.py`** when MSYS `convttf` + full OTF set are available to align naming with the rest of the font matrix.

---

## 2026-04-08 (Apple2026 strict correction pass — shell + WPS + Music path + menu IA)

### Changed
- **`apps/root_menu.c`**
  - **Music → `/Music/`** (trailing slash): `set_current_file("/Music")` was parsed as file `Music` in `/` (root browse), not the library folder — fixed default + hotswap fallback to **`/Music/`**.
  - **Root menu (MODEL 5 / 71):** removed **Plugins** and **Shortcuts** entries from `menu_table` (still available under Settings / stock paths on other targets).
- **`wps/Apple2026.wps`**
  - Rebuilt toward Interpod-style stack: **album art `%Cd` on main layer** after documenting root cause (full-screen main `%Vb` was painting over backdrop art).
  - **Backdrop `%VB`:** flat `wpsBackdrop` only (no art in buffer).
  - **Metadata:** title, **centred playlist counter** under title, **artist `FF2D55`** (slot 10 `16-SFProText-Medium.fnt`), **album** grey on its own line; progress/time re-anchored to fit 240px height.
- **`wps/Apple2026.sbs`**
  - **Library** band: `y=18`, `h=40`; list `%Vi` `_lt` start **`y=58`**; slot 5 → **`22-SFCompactDisplay-Semibold.fnt`**; root battery text/icon nudged to match band.
- **`wps/WPSLIST`**, **`apps/settings_list.c`**
  - Default menu font **`22-SFCompactDisplay-Semibold.fnt`** (was `20-Inter-SemiBold`).
- **`apps/gui/list.c`**, **`apps/gui/bitmap/list.c`**
  - Row floor **`30px`**; **`ICON_PADDING 10`** (was 26 / 7).
- **`tools/apple2026_symbol_assets.py`** + regenerated outputs
  - Menu tiles **`34×30`**, scale **`1.0`**; **battery** strip redrawn (28×15 frames, slim capsule, green/orange/red fill levels).
- **`DESIGN_SYSTEM.md`**
  - §21A (`/Music/` + root menu IA), §27.1 / §27.1.1 (typography), §54.1 / §54.2 / §54.3 (geometry, battery, list/icon tokens).

### Verified
- `python tools/apple2026_symbol_assets.py` completed; `icons/Apple2026Icons.bmp` and `wps/Apple2026/*.bmp` regenerated.
- Static review: WPS draw order (shell fill → `%Cd` → frame); `tree.c` `set_current_file` path parsing for `/Music/` vs `/Music`.
- **Not run:** simulator / `make` (MSYS toolchain not invoked in this Windows session).

### Docs
- This entry; **`DESIGN_SYSTEM.md`** updates above.

### Next
- Simulator: main menu density, Library title position, **Music opens `/Music` folder**, WPS album art visible, battery glyph, metadata colours.
- Users with **`root_menu` cfg** listing `plugins` / `shortcuts`: tokens ignored until they reset menu or edit cfg.

---

## 2026-04-08 (Apple2026 focused correction/polish pass — WPS, menu shell, icons, typography)

### Changed
- **`wps/Apple2026.wps`**
  - Restored first-frame album-art draw path by removing `%VB` from the art viewport so real art renders directly in the main layer.
  - Tightened metadata stack and moved secondary metadata upward (`y=190`) with larger secondary font slot (`14-SFProText-Regular.fnt`).
  - Moved track number to right side on the same metadata line, same size and gray tone.
  - Replaced bitmap-backed progress rail with flat `noborder` rail (`E5E5EA` base / `6E6E73` fill) to remove right-edge artifact and match volume style.
  - Rebuilt WPS volume overlay rail to the same flat slider language and thickness family as progress.
- **`tools/apple2026_symbol_assets.py` + regenerated assets**
  - Upscaled menu icon tiles to `30x26`, increased glyph scale to `0.96`, reduced inner padding for sharper detail.
  - Kept symbol set coherent but strengthened Settings/System semantics (`gear`, `wrench.fill`) with larger render footprint.
  - Reduced album frame/placeholder radius from `10` to `6` for subtler corners.
  - Regenerated `icons/Apple2026Icons.bmp` and all `wps/Apple2026/*.bmp`.
- **`wps/Apple2026.sbs`**
  - Removed header/list divider line feel (no top hairline bars; no large-title separator line).
  - Removed list viewport cropping by expanding list viewports to full width and full vertical continuation under bottom shell.
  - Moved active playback indicator from top-left to bottom-right mini-player lane (`player_status_bottom`).
  - Added root-specific battery icon/text lanes aligned with `Library` large-title band.
  - Updated SBS volume rail to flat neutral style that visually matches WPS progress rail.
- **`wps/WPSLIST`**
  - Increased Apple2026 default list font to `20-Inter-SemiBold.fnt` for +1/+2px readability push.
  - Lightened selector color (`DADAE0`) and removed menu separators (`list separator height: 0`).
  - Updated Apple2026 icon strip documentation comment to `30x26`.
- **`apps/gui/bitmap/list.c`**
  - Increased icon padding `6 -> 7` to rebalance spacing around larger icons.
- **`apps/gui/list.c`**
  - Raised Apple2026 iPod list row minimum `25 -> 26` to preserve rhythm with larger icons/type.
- **`apps/settings_list.c`**
  - Changed iPod Video/Classic default font to `20-Inter-SemiBold`.

### Verified
- Lint diagnostics: clean on touched files.
- Asset generation: `python tools/apple2026_symbol_assets.py` completed successfully; all expected outputs regenerated.
- Static intent checks:
  - WPS now uses direct album-art drawing path (no `%VB` on art viewport).
  - Progress/volume rails now share flat neutral visual language.
  - Menu separators disabled from theme defaults.
  - Selector token lightened.
  - Playback indicator moved to bottom-right in active mini-player state.

### Docs
- This entry.

### Next
- Run simulator build/check after this full pass.
- Validate target screens:
  - WPS art visibility + progress right-edge artifact removal,
  - metadata/track-number alignment,
  - icon sharpness/scale (Settings/System),
  - full-width lighter selector,
  - no list/header separators,
  - bottom-right play/pause indicator in active lower tab shell.

---

## 2026-04-08 (Wave 4 — Navigation: playback source + Cover Flow back chain)

### Changed
- **`apps/settings.h`:** `enum playback_source` + `global_status.playback_source`; `playback_source_set()`.
- **`apps/root_menu.c`:** `GO_TO_PICTUREFLOW` updates `previous_browser` like other browsers; `playback_source_set()` implementation.
- **`apps/tree.c`:** On `ONPLAY_START_PLAY`, set source to **database** / **Music (`/Music`)** / **files** (iPod 5 / 71 path check).
- **`apps/gui/wps.c`:** WPS **SELECT** → **`GO_TO_PICTUREFLOW`** when `playback_source == PICTUREFLOW` (5 / 71 + tagcache).
- **`apps/plugins/pictureflow/pictureflow.c`:** Set `playback_source` to PictureFlow after `playlist_start` in `start_playback`; **idle/scrolling `PF_BACK`** returns **`PLUGIN_OK`** (exit to main menu) instead of **`PLUGIN_GOTO_WPS`**.

### Verified
- Lint: clean on touched files.
- Build: `build-sim` reported nothing to rebuild in this environment; run full incremental sim build per `BUILD.md` when MSYS `make` picks up changes.

### Docs
- `DESIGN_SYSTEM.md` §21A (this entry).

### Next
- Simulator: CF → track → WPS → SELECT → CF; CF idle Left → main menu; WPS MENU → home; Music → play → WPS → SELECT → `/Music` when source not PF.

---

## 2026-04-08 (Wave 5b — Remove Storage row + hide Picard track index in lists)

### Changed
- **`apps/root_menu.c`:** Removed **Storage** menu item and `storage_browser` (full-disk browse not shown at root).
- **`apps/tree.c` (iPod Video / Classic only):** In the file browser list, **display-only** stripping of leading **`N.`** prefixes on audio files (Picard-style `1. Title.m4a` → show as `Title` with extension rules unchanged). Real filenames unchanged for playback.

### Verified
- Lint: clean on touched files.

### Docs
- `MASTER.md` Wave 5 bullet; this entry.

---

## 2026-04-08 (Wave 5 — Music Library IA / menu simplification)

### Changed
- **`apps/root_menu.h` / `apps/root_menu.c`:** Added `GO_TO_MUSICLIB` — file browser rooted at **`/Music`** with separate `last_music_folder` state, `Icon_Audio` in `browse_context`. Main menu: **Music** first, then Now Playing, Playlists, Cover Flow, **Database**, **Storage** (full FS), Plugins, Shortcuts, Settings, System. Replaced generic **Files** row with **Storage** label + `Icon_file_view_menu`. **`browser_default`** for `MODEL_NUMBER` 5 / 71 returns `GO_TO_MUSICLIB` for “previous browser” / default files browser.
- **`apps/gui/wps.c`:** WPS **SELECT** with “files” option (`wps_select_action` == 3) returns **`GO_TO_MUSICLIB`** on iPod Video/Classic (5 / 71) instead of `GO_TO_FILEBROWSER`.
- **`apps/settings_list.c`:** Default **`wps_select_action`** for 5 / 71: **3** (files) so default matches Music library path above.
- **`DESIGN_SYSTEM.md` §21A:** Documented WPS SELECT → `/Music` browse.
- **`MASTER.md`:** Wave 5 rewritten to match this IA pass (no flat Albums/Songs engine in this wave).

### Verified
- Not run: `make` not on PATH in this Windows shell; run `./rockpod.ps1 sim -Incremental` or `build-sim` + `make` per `BUILD.md` / `AGENTS.md`.

### Docs
- This entry; `MASTER.md` Wave 5; `DESIGN_SYSTEM.md` §21A.

### Next
- Simulator: main menu order, Music → artist folders under `/Music`, Storage → `/`, WPS SELECT → `/Music`, Database still reachable.
- Users with saved `root_menu` config may need to reset menu order or add `music` key to see new defaults.

---

## 2026-04-08 (Wave 3f — Header/title stack tune + inset separators + §54 token sync)

### SBS: Large title band (`wps/Apple2026.sbs`)
- Changed `large_title` viewport background from `F8F8F8` → `FFFFFF`.
  Matches list content background — eliminates the subtle color seam between
  the "Library" title area and the first list row.
- Added `%dr(0,31,-,1,DCDCE0)` at the bottom of the `large_title` viewport.
  Draws a 1px `DCDCE0` separator at screen y=58 (boundary between title band
  and first list row) — clean Apple Music-style visual break.
- Visual stack at root (playing on, large title on):
  ```
  y=0..23   F8F8F8 header chrome
  y=24      DCDCE0 hairline
  y=25      F4F4F4 soft hairline
  y=26      FFFFFF (screen bg, 1px gap)
  y=27..51  FFFFFF + "Library" text (25px SFPro Display Bold)
  y=52..57  FFFFFF (breathing room below text)
  y=58      DCDCE0 separator (bottom of large_title band)
  y=59+     FFFFFF list content
  ```

### list.c: Inset dividers (`apps/gui/bitmap/list.c`)
- In `_default_listdraw_fn`, after `put_line` draws each row (including the
  bottom hairline separator), added a "cover" rectangle at the icon-column
  position: `fillrect(0, y+line_h-1, icon_width, 1)` using the viewport
  background color.
- Effect: the 1px divider line is covered in the icon column (x=0..39 within
  viewport = x=0..55 from screen edge) and remains visible starting at x=40
  (= screen x=56, aligned with the text column).
- This creates Apple Music-style inset separators that begin at the text
  column, not at the left edge. Only runs on color LCD depth≥16 when icons
  are shown and `list_separator_height > 0`.

### DESIGN_SYSTEM.md §54 full sync
- **§54.1:** Rewrote to document Interpod adaptive viewport system. Added all
  `large_title` geometry tokens. Replaced old fixed absolute height tokens
  with `mainlarge`/`main`/`_lt` variants and their negative-height offsets.
  Added architecture constraint note tying `-12`/`-37` to strip/bar heights.
- **§54.1.1:** Rewrote routing table and vertical stack diagram to show actual
  Interpod-based SBS structure (two stacks: playing+LT and stopped+no-LT).
- **§54.3:** Updated `icon_tile_h` from `32` → `25`. Updated `row_h_firmware`
  to `25`. Added `divider_implementation = inset`. Updated all visible row
  counts for 25px rows and new Interpod viewport heights.
- **§54.4:** Added `chevron_cy_formula` and `chevron_cx_formula` for
  derivation transparency. Added selector color tokens.
- **§54.5:** Rewrote to document the Interpod adaptive two-state bottom bar
  (`bottombar` 36px / `bottombarsmall` 11px) and the constraint tying the
  `%Vi` negative offsets to bar heights.

### Verification needed
- [ ] Simulator: root screen shows "Library" in white band with clean separator above first row
- [ ] Root: no visible color seam between title area and list
- [ ] All sub-menu screens: dividers start at text column (after icon), not at left edge
- [ ] Selected row: icon area at separator row shows viewport bg (white) — minor edge case acceptable
- [ ] Non-root screens: header title visible, no large title band
- [ ] Bottom bar: 36px when stopped (art + title), 11px strip when playing

---

## 2026-04-07 (Wave 4 — Apple2026 UX flow: queue shortcut + WPS browse default)

### Changed
- **`apps/keymaps/keymap-ipod.c`:** Mapped `ACTION_WPS_VIEW_PLAYLIST` on **SELECT+MENU** (main WPS; chord listed first so it overrides single-button actions). Remote: **RC_SELECT+RC_MENU** when `BUTTON_REMOTE` is enabled. Comment documents ordering vs browse/menu/hotkey/quickscreen.
- **`apps/settings_list.c`:** For **`MODEL_NUMBER` 5 / 71** (iPod Video / Classic), default **`wps_select_action`** = **1 (database / tag browser)** so SELECT from WPS returns into the Music library by default; other targets unchanged (0 = previous screen).
- **`apps/root_menu.c`:** Comment only: documents **`GO_TO_PREVIOUS_MUSIC`** / **`previous_music`** / WPS vs FM.

### Verified
- Static: keymap chord ordering places `ACTION_WPS_VIEW_PLAYLIST` before `ACTION_WPS_BROWSE` and `ACTION_WPS_MENU` in `button_context_wps`.
- Build: not run in this session (no `make` on host PATH outside MSYS); run `./rockpod.ps1 sim -Incremental` or `build-hw-*` + `make` per `BUILD.md` / `AGENTS.md`.

### Docs
- `DESIGN_SYSTEM.md` — new **§21A Navigation contract (Apple2026)** (terminology, rules, matrix).

### Next
- Simulator: WPS → SELECT+MENU opens current playlist; Play exits viewer to WPS; SELECT from WPS lands in database with new default.
- Optional: user may change **WPS select action** in settings if they prefer previous browser / Cover Flow / files.

---

## 2026-04-08 (Wave 3e — Cover Flow track-tap playback bug fix)

### Bug: tapping a track in the Cover Flow track list did nothing visible

**Root cause 1 — `auto_wps` default:**
`pf_cfg.auto_wps` defaulted to `0` (OFF). In this mode, selecting a track in the
track list calls `start_playback(false)` which starts playback but keeps Cover Flow
on screen — the display does not change, so the user sees no feedback and assumes
nothing happened. The correct Apple Music-like behaviour is mode `2` (VIA_TRACK_LIST):
select track → play + navigate to WPS (Now Playing screen).
- **Fix:** Changed `cfg->auto_wps = 0` → `cfg->auto_wps = 2` in `pictureflow.c` line 772.

**Root cause 2 — `warnon_erase_dynplaylist` default:**
When music was already playing, `warn_on_pl_erase()` in `start_playback()` triggered a
YES/NO confirmation dialog ("Erase the dynamic playlist?") before allowing playback to
start. The user pressing BACK or NO silently cancelled everything — `start_playback`
returned false and Cover Flow stayed on screen with no indication of failure. In Apple
Music tapping a song just plays it; there is no confirmation dialog for replacing
the queue. Changed the default from `true` to `false` so the dialog is opt-in.
- **Fix:** Changed default of `warnon_erase_dynplaylist` in `settings_list.c` from `true` → `false`.

### Files changed
- `apps/plugins/pictureflow/pictureflow.c` — line 772: `auto_wps = 0` → `auto_wps = 2`
- `apps/settings_list.c` — `warnon_erase_dynplaylist` default: `true` → `false`

### Verification needed
- [ ] Open Cover Flow, navigate to an album, open track list, tap a track
- [ ] Should immediately start playback AND navigate to WPS (Now Playing screen)
- [ ] Should work whether or not music is already playing (no confirmation dialog)

---

## 2026-04-08 (Wave 3d — Full Interpod-first merge: SBS + WPS rebuilt from Interpod base)

### Strategy change
User confirmed: start from Interpod's PROVEN, POLISHED code and layer Apple2026 on top — do not
greenfield or diverge. Interpod was "almost perfect." The goal is Interpod PLUS Apple2026 visual
language, not a fresh build that mimics it imperfectly.

### Apple2026.sbs — rebuilt from Interpod Rev.17

Key structural adoption from Interpod:
- **Adaptive bottom bar** (most important): when playing/paused → thin 11px hairline strip at
  bottom (content area expands to 201px); when stopped → full 36px bar showing last-played art +
  track title (content area shrinks to 176px). This mirrors Interpod exactly.
- **Interpod VI routing pattern**: `%?mp<%VI(mainlarge)|%VI(main)>` with negative-height
  viewports (`-12` = 11px strip space, `-37` = 36px bar space). Robust to any screen size.
- **Volume bar in bottom bar zone**: Interpod's 3-part `volumebar` viewport system (speaker icon,
  `vb.bmp` textured fill bar, loud/clip indicator) copied verbatim. Color updated to F8F8F8.
- **Battery/busy/lock**: Interpod's exact positioned viewports kept unchanged.
- **`busyindicatorleft`/`busyindicator`**: Interpod's dual-position busy spinner (left when
  battery % shown, right otherwise) carried forward.

Apple2026 additions on top:
- `%Lo` large title routing: `%?mp<%?Lo<%VI(mainlarge_lt)|%VI(mainlarge)>|%?Lo<%VI(main_lt)|%VI(main)>>`
- Two extra `_lt` VI variants (start at y=59 instead of y=27 to make room for large title)
- `large_title` labeled viewport (y=27..58, 32px, slot 2: 25px SFPro Display Bold, "Library")
- Header title suppressed at root (`%?Lo<|...title logic...>`) — large title takes its place
- Colors: `ededed` → `F8F8F8` everywhere; hairlines `E3E3E3/F3F3F3` → `DCDCE0/F4F4F4`
- Left/right inset: Interpod's 2px → 16px (Apple-like list margins)
- Font slots: {3:15-Inter-Bold, 5:16-Inter-SemiBold} → {2:25-SFPro, 3:16-Inter-SemiBold, 5:19-Inter-V-padded}
- Mini-player track font: slot 5 (now 19px, too tall) → slot 3 (16px, fits 36px bar)
- `albumart` mini bg: `ebeeee` → `F0F0F0`
- `volumebar_small_clip` color: `ED230D` → `FF2D55` (Apple2026 red)

New assets copied from Interpod to `wps/Apple2026/`:
- `vb.bmp` + `vb_backdrop.bmp` — volume bar texture/backdrop
- `pb.bmp` + `pb_active.bmp` — progress bar fill (normal/seeking)
- `pb_backdrop.bmp` + `pb_active_backdrop.bmp` — progress bar backdrops

### Apple2026.wps — rebuilt from Interpod Rev.17

Key structural adoption from Interpod:
- **Art in VB layer** (already fixed in Wave 3b — kept): backdrop pattern for no black bars
- **Custom progress bar bitmaps**: `%pb(0,0,-,4,image,pb.bmp,backdrop,pbi)` + active seek bar
  with `pb_active.bmp` (9px, shown during rewind). Much more polished than `noborder`.
- **Volume bar**: Interpod's single-viewport `vb.bmp` textured fill pattern
- **Repeat/shuffle indicators**: Interpod's exact hotspot positions, right margin (x=238, y=143)
- **Playlist counter**: left margin (x=0, y=140, w=82), `%pp %Sx(of) %pe` localised format
- **`timeprogresslossless`** label re-used for elapsed/lossless/remaining (3 same-label viewports)
- **Artist/album cycling**: Interpod's `%t(5)` alternation between artist/album/composer

Apple2026 additions on top:
- Art size: Interpod's 130x130 → 156x156 (centred x=82 vs x=95)
- Art position kept at y=8 (top 18px under header — intentional Interpod depth effect)
- All metadata coordinates recalculated for 156px art:
  - Title: y=172 h=22 (8px gap from art bottom y=164)
  - Artist: y=196 h=14 (0px below title)
  - Progress: y=212 h=4 (2px below artist)
  - Seeking: y=210 h=9 (centred on progress position)
  - Time: y=218 h=14 (6px below progress)
- Artist color: Interpod's `f24e61` → Apple2026 `FF2D55`
- Font slots: {3:15-Inter-Bold, 8:20-Inter-SemiBold, 9:20-Inter-V} →
  {3:16-Inter-SemiBold, 8:20-Inter-SemiBold, 9:13-Inter-SemiBold}
- Artist text: slot 9 (20px Interpod) → 13px (tighter hierarchy, Apple Music style)
- Artist gets full width (removed side-by-side track position — uses left margin counter instead)
- Removed standalone `25-SFPro` WPS slot (not needed in WPS — only SBS needs it for large title)

### Files changed
- `wps/Apple2026.sbs` — full rewrite from Interpod base (see above)
- `wps/Apple2026.wps` — full rewrite from Interpod base (see above)
- `wps/Apple2026/vb.bmp` — copied from Interpod
- `wps/Apple2026/vb_backdrop.bmp` — copied from Interpod
- `wps/Apple2026/pb.bmp` — copied from Interpod
- `wps/Apple2026/pb_active.bmp` — copied from Interpod
- `wps/Apple2026/pb_backdrop.bmp` — copied from Interpod
- `wps/Apple2026/pb_active_backdrop.bmp` — copied from Interpod

### Verification needed
- [ ] Build sim, confirm no skin parse errors in debug output
- [ ] At root (stopped): large title "Library" + full 36px bottom bar (but no track info if fresh boot)
- [ ] At root (playing): large title + thin 11px strip, content area taller
- [ ] Non-root (playing): compact title in header, thin strip, full content
- [ ] WPS: 156x156 art centred, no black bars, red artist, repeat/shuffle in right margin
- [ ] WPS: progress bar uses pb.bmp texture (not flat color)
- [ ] WPS: volume adjustment shows vb.bmp textured bar
- [ ] Screenshot comparison vs Interpod + Figma Apple Music Library

---

## 2026-04-08 (Wave 3b — Interpod-baseline stabilisation pass)

### Root causes fixed
- **WPS black side bars:** Art was in main layer (no `%VB`). Areas left/right of art box had
  no explicit viewport fill — relied on backdrop BMP loading correctly on hardware. On device,
  backdrop sometimes failed to render, leaving black behind art. Fix: moved art viewport to
  `%VB` (backdrop layer) exactly like Interpod, so art + letterbox areas are baked into the
  backdrop buffer. Main layer then paints chrome on top. Added `%V(0,0,-,-,-)` + `%Vb(F8F8F8)`
  as explicit main-layer context, matching Interpod's proven pattern.
- **Large title showing "Rockbox":** `large_title` viewport used `%Lt` which returns the
  Rockbox string. Since the viewport is gated by `%?Lo` (only shown at root), hardcoded
  "Library" as the display text. No C language changes needed.
- **Rows too short (19px instead of 32px):** `list_init_item_height` was ignoring icon tile
  height on non-touchscreen targets. Added `#if ROCKPOD_APPLE2026_IPOD` minimum of 32px to
  match the icon strip tile height (`ICON_TILE_H = 32`). Viewports in §54 were already sized
  for 32px rows — this fix makes the list engine match them.
- **Settings icon black/wrong:** `root_menu.c` was using `Icon_Submenu_Entered` (chevron.down,
  gray) for the Settings root menu item. Changed to `Icon_General_settings_menu` (gear, red).
- **Several main-menu icons showing as black/gray instead of red:** `_main_icon_tint()` had a
  narrow accent set; only 11 of 32 icons were red. Replaced with an inverted model: all icons
  are red (`CLR_ACCENT`) except structural chrome glyphs (cursor, reverse-cursor, moving,
  submenu indicators) which remain tertiary gray. All assets regenerated.

### Files changed
- `wps/Apple2026.wps` — WPS rebuilt with Interpod VB pattern for art + stable viewport stack
- `wps/Apple2026.sbs` — large_title text hardcoded to "Library"
- `apps/gui/list.c` — include `apple2026_shell.h`; 32px row-height minimum for Apple2026 iPods
- `apps/root_menu.c` — Settings item: `Icon_Submenu_Entered` → `Icon_General_settings_menu`
- `tools/apple2026_symbol_assets.py` — new accent model (all-red except chrome glyphs); assets regenerated
- `icons/Apple2026Icons.bmp`, `wps/Apple2026/*.bmp` — regenerated

### Verified
- Asset script runs clean (0 errors, 19 files generated)
- No linter errors in changed C files
- WPS art VB structure matches Interpod Rev.17 reference

---

## 2026-04-08 (Wave 3c — Font quality fix: Inter replaces SF Pro for all sub-25px slots)

### Root cause of fuzzy text confirmed
Both Interpod (Inter) and our fonts use `depth=1` (4bpp antialiased). The difference is
**TrueType hinting**: Inter was built by Rasmus Andersson specifically for screen rendering at
12-24px — it has complete `cvt`/`fpgm`/`prep` hinting tables that tell FreeType to snap stems
to exact pixel boundaries at each size. SF Pro relies on Apple's proprietary Core Text
rendering engine (AAT hints). FreeType — used by `convttf` — ignores AAT hints entirely.
Result at 13-19px on the iPod's 132 DPI screen: Inter strokes are 1px wide and perfectly
aligned; SF Pro strokes land at fractional positions, producing gray fringe (reads as blur).
At 25px (large title), the effect is negligible — kept SF Pro Display Bold there.

### Changes
- `fonts/16-Inter-SemiBold.fnt` — copied from Interpod 1 (header chrome, slot 3)
- `fonts/19-Inter-V-padded.fnt` — copied from Interpod 1 (body, slot 5; cell height=25px)
- `fonts/20-Inter-SemiBold.fnt` — copied from Interpod 2 (WPS title, slot 8; height=22)
- `fonts/13-Inter-SemiBold.fnt` — copied from Interpod 2 (secondary/metadata, slot 9; height=14)
- `wps/Apple2026.sbs` — font slots 3/5/9 now Inter (slot 2 keeps SF Pro Display 25px)
- `wps/Apple2026.wps` — font slots 3/5/8/9 now Inter; WPS coordinates updated for h=22 title
  - Title: y=172 h=22 (was h=20), artist: y=196 (was y=192), progress: y=212 (was y=210), time: y=218 (was y=224)
- `wps/WPSLIST` — body font: `19-Inter-V-padded.fnt` (was `19-SFProText-Medium.fnt`)
- `tools/apple2026_symbol_assets.py` — `ICON_TILE_H` 32→25 (matches 25px Inter row height)
- `apps/gui/list.c` — row minimum 32→25 (Inter padded font naturally drives 25px rows)
- All icons regenerated at 28×25 tile size

### Verified
- Asset script clean (0 errors, 19 files)
- No linter errors

---

## 2026-04-07 (Wave 3 §54-COMPLIANT CORRECTION: every coordinate verified against design system)

### Changed
- **SBS completely rewritten to match §54 geometry token table exactly:**
  - Header: **26px** (y=0..25) — was wrongly 28px.
  - Large title: **y=27..50** (24px band) — was wrongly y=28, h=30.
  - Content viewports: **y=27/h=177, y=51/h=153, y=27/h=213, y=51/h=189** — all were wrong.
  - Mini-player: **y=204, h=36** — was wrongly y=198, h=42.
  - Mini art: **8,207,30,30** — was wrongly 6,201,36,36.
  - Mini title: **48,212** — was wrongly 48,204.
  - Mini meta: **48,226** — was wrongly 48,224.
  - Mini state: **284,214,20,16** — was wrongly 284,210,20,18.
  - All indicator positions restored to §54.2: lock (24,7,9,12), battery (288,4,27,16), busy (58,9,9,9), player (3,5,15,16).
- **WPS completely rewritten to match §54.6 geometry:**
  - Album art: **156x156 at (82,9)** — was wrongly 170x170 at (75,6).
  - Title: **y=172** — was wrongly y=178.
  - Artist: **y=192** — was wrongly y=200.
  - Progress: **y=210** — was wrongly y=220.
  - Time: **y=224** — was wrongly y=227.
  - All header indicators match SBS exactly.
- **Font slots corrected to §27.1 hierarchy:**
  - Slot 2: **22-SFCompactDisplay-Semibold** (was 25-SFProDisplay-Bold — too large for 24px band).
  - Slot 3: **16-SFProText-Medium** (was 15-SFProText-Semibold — didn't match §33.2 lock).
  - Slot 5: **19-SFProText-Medium** (user body-size override, documented in §27.1.1).
  - Slot 8 (WPS title): **16-SFProText-Medium** (was 17-SFProText-Bold — §33.2 locks WPS title at 16px).
  - Slot 9: **13-SFCompactText-Regular** (was 14-SFProText-Regular — §27.1 locks secondary at 13px).
- **Icon assets regenerated at §54-locked sizes:**
  - Main strip: **22x20** tiles (was wrongly 24x22).
  - playerStatus: **15x16** (was 17x18).
  - holdSlider: **9x12** (was 11x14).
  - batteryStatus: **27x16** (was 29x18).
  - busyIndicator: **9x9** (was 11x11).
  - speaker_loud/mute: **19x17** (was 21x19).
  - albumPlaceholder: **156x156** (was 170x170).
- **WPSLIST comment** updated to reflect 22x20 tiles.
- **DESIGN_SYSTEM.md** §27.1.1 added documenting user body-size override (19px).

### Verified (line-by-line against §54)
- Every SBS viewport coordinate checked against §54.1, §54.2, §54.5, §58.2, §58.5 — all match.
- Every WPS viewport coordinate checked against §54.6, §58.6 — all match.
- Every asset dimension checked against §54 locks — all match.
- All 4 required fonts verified present in `fonts/`.
- No Cabbie leakage: iPod models use Apple2026 WPS/SBS/font/icons/backdrop.
- `dynamic_colors` confirmed false for iPods.
- Icon source confirmed: `Sourced Icons/sf-symbols-master/glyphs` (1672 glyph PNGs available).

### Docs
- This entry; DESIGN_SYSTEM.md §27.1.1 user override.

### Next
- Simulator build + screenshot validation.

---

## 2026-04-07 (Wave 3 practical fit-and-polish pass: font-to-lane clipping, WPS title scale, dividers)

### Changed
- **SBS mini_title**: slot 5 (19px) → **slot 3 (16px)**. 19px font in 16px lane was 3px clipping — would have visibly butchered song titles with descenders.
- **WPS slot 8 (title)**: 16-SFProText-Medium → **19-SFProText-Semibold**. 16px for the WPS song title was too conservative; 19px semibold in 20px lane (1px pad) provides Apple-scale typography the user requires.
- **WPS playlist count**: slot 3 (16px) → **slot 9 (13px)**. 16px in 14px lane was 2px clipping; secondary text should be 13px per hierarchy.
- **WPS time labels** (progress_lane, progress_lane_right): slot 3 → **slot 9 (13px)**. Same 2px clipping issue; time is tertiary text.
- **WPSLIST**: `list separator height: 0` → **`1`**. §54.3/§13 require visible inset dividers (DCDCE0). Height 0 eliminated all dividers entirely.

- **SBS volume overlay (HIDDEN BUG)**: speaker_mute was at viewport-relative (37,211) in a 36px-tall overlay — drawn at y=415 absolute, completely invisible. Fixed to (37,9). Speaker_loud/speaker_too_loud were drawn via %xd inside a 3px-tall volume_bar viewport, clipping 17px icons to 3px. Moved to volume_overlay viewport. %xl positions updated to (270,9) and (300,9) — viewport-relative to volume_overlay.
- **WPS volume overlay**: speaker_mute at viewport-relative (37,4) in 18px overlay — 3px bottom clipping. Fixed to (37,1) matching speaker_loud/too_loud y-offset.

### Verified (font-to-lane audit)
- Every text viewport checked: font px vs lane height.
- No critical clipping after corrections.
- Only marginal: battery_text (16px in 15px, digits-only content — no practical descender risk).
- WPS metadata stack: art ends y=165, title y=172 (7px gap), artist y=192, progress y=210, time y=224 — no overlaps, all transitions clean.
- Mini-player: title (16px in 16px) and meta (13px in 14px) both clean. 2px overlap zone at y=226-227 is per §58.5 design — meta overwrites, acceptable for the compact 36px band.
- SBS volume overlay: speaker and bar now correctly positioned with viewport-relative coords. Speakers in 36px overlay viewport, bar in separate 3px viewport (for color separation).
- WPS volume overlay: all speakers fit within 18px viewport without clipping.
- All SBS/WPS header indicators (player_status 15x16, lock 9x12, sleep 9x12, battery 27x16, busy 9x9) verified to fit their respective viewport dimensions.
- Content viewport bottom edges abut mini-player band (y=204) correctly.

### Docs
- This entry.

### Next
- Rebuild.

---

## 2026-04-07 (Wave 3 FULL REBUILD: typography, icons, geometry, WPS)

### Changed
- **Typography system rebuilt from scratch:**
  - Default list font: **19-SFProText-Medium** (was 16px) — closer to Apple UI body scale.
  - Large title: **25-SFProDisplay-Bold** (was 22px SFCompactDisplay-Semibold) — proper iOS-class large title.
  - Header chrome: **15-SFProText-Semibold** (was 16px Medium) — distinct from body, proportional.
  - WPS title: **17-SFProText-Bold** (new slot 8) — readable track name, distinct from body.
  - WPS metadata: **14-SFProText-Regular** (slot 9) — calm secondary text, not tiny.
  - Installed 6 new `.fnt` files from `Apple Fonts/Generated FNT/Apple2026/` into `fonts/`.
- **Icon system rebuilt:**
  - `tools/apple2026_symbol_assets.py`: tiles **24×22** (was 22×20); padding 1 (was 2); scale 0.96 (was 0.91).
  - WPS symbols enlarged: `playerStatus` **17×18** (was 15×16), `holdSlider` **11×14**, `batteryStatus` **29×18**, `speaker` **21×19**, `busyIndicator` **11×11**, `shuffle/repeat` all +2px.
  - Regenerated all `icons/Apple2026Icons.bmp` and `wps/Apple2026/*.bmp`.
- **SBS geometry fully rebuilt (`Apple2026.sbs`):**
  - Header band: **28px** (was 26px) — fits larger header icons.
  - Large title band: **30px** at y=28 (was 32px at y=27) — 25px Display Bold.
  - Content viewports adjusted: `content_with_mini` at y=28/h=170, `content_with_mini_lt` at y=58/h=140, etc.
  - Mini-player: **42px** (was 36px) at y=198 — 36×36 art, 19px title, 14px artist.
  - Volume overlay: **42px** with **5px** rail (was 36px/3px).
- **WPS fully rebuilt (`Apple2026.wps`):**
  - Album art: **170×170** at x=75, y=6 (was 160×160 at x=80, y=8) — larger, centered.
  - Title: y=178, **17px bold** (was y=170, 16px medium) — much more readable.
  - Artist: y=200, **14px regular, accent `#FF2D55`** (was y=190, 13px) — properly visible.
  - Progress bar: **5px/6px** rail (was 4px/5px).
  - Volume overlay: **22px** tall with **5px** rail.
  - Removed all outdated divider lines, white box overlay, broken framing.
  - albumPlaceholder regenerated at **170×170** (was 160×160).
- **`settings_list.c`:** `DEFAULT_FONTNAME` → `"19-SFProText-Medium"` (was 16px).
- **`WPSLIST`:** Font → `19-SFProText-Medium.fnt`; icon comment updated to 24×22; `list separator height: 0` (cleaner Apple aesthetic).

### Verified
- `python tools/apple2026_symbol_assets.py` exit 0; all 19 assets regenerated with correct dimensions.
- All 10 `.fnt` files present in `fonts/` with correct names (no BDF conflicts).
- No Cabbie/tango leakage in `apps/gui/`, `apps/menus/`, or theme files.
- `dynamic_colors` remains `false` for iPod models.
- SBS viewport geometry: header(28) + large_title(30) + content(140..212) + mini(42) = 240px validated.
- WPS geometry: art(6..175) + title(178..199) + artist(200..217) + progress(220..225) + time(227..240) = clean stack.
- Icon tile math: `Apple2026Icons.bmp` = 24×704 (24px wide, 32 icons × 22px each).

### Docs
- This entry.

### Next
- Simulator build + screenshot validation.
- Visual comparison vs Figma and Apple references.
- Fine-tune if list row rhythm needs adjustment after seeing real font rendering.
- Verify mini-player art + text alignment at 42px height.

---

## 2026-04-07 (Wave 3 major rebuild: type band, icons, WPS, dynamic colors)

### Changed
- `tools/apple2026_symbol_assets.py`: main icon strip **`22×20`** tiles; **accent `#FF2D55`** / **black** / **`#3C3C43`** tints by `Icon_*` index; flat **`wpsBackdrop`** (no top/bottom hairlines); **`160×160`** placeholder + subtle `albumFramed`.
- Regenerated `icons/Apple2026Icons.bmp`, `wps/Apple2026/*.bmp` (script run).
- `wps/Apple2026.sbs`: large-title **`h=32`**, content **`content_*_lt`** at **`y=59`** (`h=145` / `181`); mini title/meta **`x=46`**, **`w=228`**; volume rail **3px**.
- `wps/Apple2026.wps`: **`160×160`** art at **`x=80,y=8`**; **removed** `AlbumFramed` overlay on playing art; **4px/5px** progress; artist line **`#FF2D55`**; **3px** volume overlay; flat shell backdrops.
- `apps/settings_list.c`: **`dynamic_colors` default `false`** for **`MODEL_NUMBER` 5/71** (album-art tinting off for Apple2026 shell).
- `DESIGN_SYSTEM.md`: §9, §27.1, §31, §33.2, §33.2.1 updated for Wave 3 rebuild locks.
- `wps/WPSLIST`: comment on icon strip geometry.

### Verified
- `python tools/apple2026_symbol_assets.py` exit 0; `Apple2026Icons.bmp` size **22×640** (32×20px rows).
- Static: `settings_list.c` conditional wraps `OFFON_SETTING` for `dynamic_colors`.

### Docs
- This entry; `DESIGN_SYSTEM.md` as above.

### Next (screenshot / sim matrix — not run in this session)
- Root menu with **`%Lo`**: large-title truncation, list + **22px** icons.
- Submenus: neutral icon tints; **Settings** list readability.
- **WPS**: no frame overlay; progress/volume thickness; artist/title contrast.
- If list feels **short** vs Figma **52px** rows: future **larger list `.fnt`** (requires new convttf source) or touchscreen-only padding paths.

## 2026-04-07 (Wave 3 micro-polish follow-up: time lanes + large-title inset)

### Changed
- `wps/Apple2026.wps`: `progress_lane` / `progress_lane_right` foreground **`6E6E73`** (was `6F6F73`) — tertiary parity with mini/meta/volume.
- `wps/Apple2026.sbs`: `large_title` viewport right inset **`-16`** (was `-32`) — symmetric 16px gutter, less aggressive truncation on long root titles.
- `DESIGN_SYSTEM.md` §33.2.1: note on large-title `-16`.

### Verified
- Static: no remaining `6F6F73` in `Apple2026.wps`.

### Docs
- `DESIGN_SYSTEM.md` **33.2.1**; this entry.

### Next
- Sim: long `LANG_ROCKBOX_TITLE` / root title; elapsed/remaining readability.

## 2026-04-07 (Wave 3 micro-polish: gutters, tertiary, rails, mini-player)

### Changed
- `wps/Apple2026.wps`: metadata/stack and scrubber **16px** inset (`-16` right); tertiary **`6E6E73`**; progress/volume rail **`E5E5EA`**; `lossless_lane` **118**; mute **x=37**; `%pv` **56,8,208** aligned with SBS.
- `wps/Apple2026.sbs`: mini title/meta **x=46**, width **228**; volume bar **56×208**, rail **E5E5EA**; mute **x=37**.
- `DESIGN_SYSTEM.md` §33.2 + **33.2.1** (micro-polish rules, 16px header notes, mini **x=46**).

### Verified
- Static: `37+19=56` (mute abuts volume bar); `8+30+8=46` (mini text vs art).

### Docs
- `DESIGN_SYSTEM.md` **33.2.1**; this entry.

### Next
- Sim: WPS edges, volume scrub, mini truncation.

## 2026-04-07 (Wave 3 continue: `%Lo` root-menu token + Apple2026.sbs i18n)

### Changed
- `lib/skin_parser/tag_table.h`, `tag_table.c`: new SBS token **`%Lo`** (`SKIN_TOKEN_LIST_TITLE_IS_ROOT`) — true when list title equals `LANG_ROCKBOX_TITLE` (localized main menu name).
- `apps/gui/skin_engine/skin_tokens.c`: evaluate `%Lo` via `sb_get_title` vs `P2STR(ID2P(LANG_ROCKBOX_TITLE))`.
- `apps/gui/skin_engine/skin_parser.c`: parse `%Lo` with same `sb_skin_has_title` hook as `%Lt`.
- `wps/Apple2026.sbs`: large-title / `content_*_lt` routing uses `%?Lo<…>` instead of `%?if(%Lt, =, Rockbox)`.
- `manual/appendix/wps_tags.tex`: document `%Lo`.
- `DESIGN_SYSTEM.md` §27.1: root-menu / `%Lo` rule.

### Verified
- Static: token table and parser cases aligned with `SKIN_TOKEN_LIST_TITLE_IS_ROOT`.

### Docs
- This entry; manual + DESIGN_SYSTEM as above.

### Next
- Build + sim: parse Apple2026.sbs; verify large-title in a non-English language.

## 2026-04-07 (Toolchain audit: exit-code bugfix + rockpod.ps1 dispatch)

### Changed
- `tools/rockpod-msys.ps1`: removed `| Out-Host` — it broke `$LASTEXITCODE` (pipeline ended with Out-Host exit 0, masking bash failures). Bare `& env.exe ... bash -lc` + `return $LASTEXITCODE` (ExtraEnv splat path verified). `BUILD.md` documents this.
- `rockpod.ps1`: `check` only dispatches; `sim`/`hw` use `$?` + explicit `exit` when child returns (check-env uses `exit $code` in-process).

### Verified
- `& env ... bash -lc 'exit 42'` → `$LASTEXITCODE=42`; ExtraEnv branch → `exit 7` return value.

### Docs
- `BUILD.md` “Toolchain script notes”; this entry.

## 2026-04-07 (Wave 3 Apple2026 parity + polish pass — typography, rails, progress, scrollbar)

### Changed
- `wps/Apple2026.sbs` / `wps/Apple2026.wps`: font slot 3 → `16-SFProText-Medium.fnt` (aligned with WPSLIST + default settings); header hairline `E3E3E6` → `DCDCE0` (A26 shell rail).
- `wps/Apple2026.wps`: WPS progress scrubber foreground `9E9EA6` → `3C3C43` (tertiary / `A26_PROGRESS_FILL` parity); volume overlay background `DADADD` → `EDEDED` (mini-player surface continuity).
- `apps/settings_list.c`: default `scrollbar_width` **2** on iPod Video/Classic (5/71) vs 6 elsewhere (matches Apple2026 theme + DESIGN subtle bar).
- `apps/plugins/pictureflow/pictureflow.c`: loading progress active segment `158,158,166` → `60,60,67` (shell tertiary fill parity).
- `tools/apple2026_symbol_assets.py`: `wpsBackdrop` / `usbBackdrop` hairlines and USB frame outline use `220,220,224` rail family; regenerated `wps/Apple2026/wpsBackdrop.bmp`, `usbBackdrop.bmp`.

### Verified
- Static review: `settings_list.c` preprocessor for `scrollbar_width` default; skin `%Fl` slot 3 matches `16-SFProText-Medium.fnt` on disk.

### Docs
- `DESIGN_SYSTEM.md` §27.1 note on slot 3 / 16px alignment; this entry.

### Next
- Simulator: list/header type rhythm, scrollbar width on fresh config, WPS progress + volume overlay, Picture Flow cache bar.

## 2026-04-07 (Toolchain polish: env audit + unified CLI + MSYS helper)

### Changed
- `tools/rockpod-msys.ps1`: shared MSYS2 root resolution (`-MsysRoot` / `ROCKPOD_MSYS_ROOT`), `Invoke-RockpodMsysBash` (later: removed `Out-Host` pipeline — see “Toolchain audit” entry — to preserve correct bash exit codes).
- `tools/rockpod-check-env.sh`: prerequisite audit (make/gcc/perl/python3, `IPC::Open2`, SDL2, `arm-none-eabi-*`, optional zip/ccache); `--sim-only` / `--hw-only`.
- `rockpod-check-env.ps1`, `rockpod.ps1`: Windows entry points (`check` | `sim` | `hw` | `help`).
- `build-sim.ps1` / `build-hw.ps1`: dot-source `rockpod-msys.ps1`; added `-MsysRoot`.
- `build-sim.sh`: optional `ROCKPOD_STRICT_INSTALL=1` fails the script when `make install` fails.
- `BUILD.md`, `AGENTS.md`: audit command, `rockpod.ps1`, MSYS override, strict install.

### Verified
- `sh -n tools/rockpod-check-env.sh`; `rockpod.ps1 help`; `rockpod-check-env.ps1` prints full audit (`Result: OK` when deps present).

### Docs
- This entry; `BUILD.md` / `AGENTS.md` updates.

### Next
- Optional: `ccache` wiring per-target if a configure hook is added later.

## 2026-04-07 (Typography: §27.1 large title + 16px list default)

### Changed
- `wps/Apple2026.sbs`: `%Fl(2,22-SFCompactDisplay-Semibold.fnt)`; `%Vl(large_title,…,2)` (gutter x=16); `%?if(%Lt, =, Rockbox)<%Vd(large_title)|>`; `%VI(content_with_mini_lt)` / `%VI(content_full_lt)` when root title is English `"Rockbox"`; compact header unchanged (slot 3, 15px).
- `wps/Apple2026.wps`: `%Fl(2,22-SFCompactDisplay-Semibold.fnt)` for parity.
- `wps/WPSLIST` Apple2026: default font `16-SFProText-Medium.fnt`.
- `apps/settings_list.c`: iPod 5/71 `DEFAULT_FONTNAME` `16-SFProText-Medium.fnt`.

### Verified
- `fonts/22-SFCompactDisplay-Semibold.fnt` and `fonts/16-SFProText-Medium.fnt` exist in tree.

### Docs
- This entry.

### Next
- Sim: root menu layout + list truncation; locale-safe match for large-title if not `"Rockbox"`; optional `14-SFCompactText-Regular.fnt` when OTF available.

## 2026-04-07 (Build-system investigation: incremental workflow + BUILD.md)

### Changed
- `build-sim.sh` / `build-hw.sh`: optional incremental mode (`-i` / `--incremental` or `ROCKPOD_INCREMENTAL=1`) reuses existing build dir and skips configure when `.rockpod_configure_stamp` matches; optional `ROCKPOD_SKIP_DEP=1` skips `make dep` if `make.dep` exists.
- `build-sim.ps1` / `build-hw.ps1`: `-Incremental` switch passes `ROCKPOD_INCREMENTAL`.
- New `BUILD.md`: end-to-end build flow, dependencies, outputs, slowness/fragility notes, symlinkinstall hint, Apple2026 Python tools called out as manual-only.
- `AGENTS.md`, `CLAUDE.md`: pointer to `BUILD.md`.

### Verified
- `sh -n build-sim.sh build-hw.sh` under MSYS2 bash (syntax OK).

### Docs
- This entry; `BUILD.md` is the primary build reference for this pass.

### Next
- Optional: theme-dev profile trimming `WPSLIST` / packaging scope is a larger product decision; optional `ccache` note already in `BUILD.md`.

## 2026-04-07 (Rockbox-leakage pass: splash chrome + theme separator reset)

### Changed
- `apps/apple2026_shell.h`: added `A26_SPLASH_BROKEN_FILL` for splash “broken theme” panel background.
- `apps/gui/splash.c`: iPod Video/Classic — splash panel border uses `A26_SHELL_RAIL` hairline instead of harsh theme-FG box; broken-theme fill/text use Apple2026 neutrals (`A26_SPLASH_BROKEN_FILL`, `A26_PROGRESS_FILL`) instead of stock light gray/black.
- `apps/menus/theme_menu.c`: `reset_color()` sets `list_separator_color` to `LCD_RGBPACK(220,220,224)` on `MODEL_NUMBER` 5/71 (exact DCDCE0); other targets keep `LCD_LIGHTGRAY`.

### Verified
- Static: preprocessor keeps unconditional `draw_border_viewport()` for non-iPod builds; iPod uses rail border when `depth > 1`.

### Docs
- This entry.

### Next
- Optional: `english.lang` tone pass for empty/error strings; grep `apps/gui` + `apps/menus` for remaining hardcoded `LCD_*` on dialog paths; plugin load strings.

## 2026-04-07 (Wave 3 shell token pass — splash + classic statusbar)

### Changed
- Added `apps/apple2026_shell.h`: shared `LCD_RGBPACK` tokens for iPod Video (`MODEL_NUMBER` 5) / Classic (`MODEL_NUMBER` 71) — rail `DCDCE0`, progress fill tertiary gray, battery remainder neutral.
- `apps/gui/splash.c`: splash progress bar uses Apple2026 rail/fill on RockPod iPod targets (replaces generic light-gray rail + black fill).
- `apps/gui/statusbar.c`: classic statusbar battery “remainder” segment uses `A26_BATTERY_REMAIN` instead of `LCD_DARKGRAY` on those targets.

### Verified
- Static: `ROCKPOD_APPLE2026_IPOD` gates non-iPod builds to prior Rockbox colors.

### Docs
- This entry; `DESIGN_SYSTEM.md` unchanged (tokens already specified there).

### Next
- Grep remaining `LCD_DARKGRAY` / `LCD_LIGHTGRAY` in `apps/` dialogs; optional yesno border alignment to same token header; rebuild sim.

## 2026-04-07 (Wave 3 runtime default theme wiring — Apple2026 active by default)

### Changed
- `apps/settings_list.c`: for `MODEL_NUMBER` 5 (iPod Video) and 71 (iPod Classic 6G/7G), default `wps`/`sbs` are now `Apple2026`, default font `15-SFProText-Medium`, iconsets `Apple2026Icons` + `viewers.6x8x16`, light-shell list colors and flat gray selector aligned with `wps/WPSLIST`, backdrop default `-` (no cabbie backdrop).

### Verified
- Static: `MODEL_NUMBER` 5/71 match `firmware/export/config/ipodvideo.h` and `ipod6g.h`.
- Note: existing `simdisk`/`config.cfg` can still override until reset or delete; use fresh config to see new defaults.

### Docs
- This entry only.

### Next
- Rebuild sim (`./build-sim.ps1`), clear or reset theme config if testing defaults, capture new screenshots.

## 2026-04-07 (Wave 3 icon-system audit + correction pass)

### Changed
- Completed dedicated Apple2026 icon-system correction cycle focused on fringing/safe-box/family coherence.
- Updated `tools/apple2026_symbol_assets.py`:
  - added `_rgba_to_keyed_rgb()` alpha-cut conversion to magenta-keyed BMP export to suppress pink fringe halos.
  - switched symbol resize path to RGBA-first + bicubic/tinted compositing before keyed export.
  - changed strip writer to RGBA composition then keyed export for cleaner edges.
  - tightened `busyIndicator.bmp` generation to reduce rotational clipping risk (`5x5` rays in centered `9x9` frame).
  - restored explicit `losslessIndicator.bmp` generation.
  - removed legacy progress/volume bar bitmap generation (`pb*`, `vb*`) from generator output.
- Regenerated Apple2026 icon assets with updated generator:
  - `icons/Apple2026Icons.bmp`
  - `wps/Apple2026/*.bmp` symbol/state set (player/hold/sleep/repeat/shuffle/speakers/busy/battery/lossless/album/backshell).
- Removed legacy donor-risk leftovers from active Apple2026 runtime icon folder:
  - `pb.bmp`, `pb_backdrop.bmp`, `pb_active.bmp`, `pb_active_backdrop.bmp`,
  - `vb.bmp`, `vb_backdrop.bmp`.

### Verified
- Current active Apple2026 runtime icon folder now contains only Apple2026 symbol-family assets (no `pb*`/`vb*` legacy leftovers).
- Generator run completed successfully and produced expected Apple2026 asset set including `losslessIndicator.bmp`.
- Existing Apple2026 shell files remain wired to symbol-family assets and no donor string regressions were reintroduced.

### Docs
- Logged icon-audit correction cycle in `WORK_LOG.md` as required.

### Next
- Continue icon-state semantic pass (pink/gray/black/white mapping per surface) and run final runtime-icon inventory proof against active WPS/SBS/WPSLIST bindings.

## 2026-04-07 (Wave 3 restart execution block: Apple2026 shell baseline rewrite)

### Changed
- Rewrote `wps/Apple2026.sbs` from scratch to a clean Apple2026 shell baseline using locked geometry/tokens:
  - header band (`0..26`), title lanes (`74/56`), battery/lock/busy anchors, light-shell top bar.
  - explicit content viewport split for with-mini (`y=27,h=177`) and full (`y=27,h=213`) states.
  - rebuilt mini-player shell (`y=204..239`, `36h`) with `30x30` art, title/meta lanes, right play/pause lane.
  - rebuilt ephemeral volume overlay with thin neutral `2px` rail and Apple2026 neutral palette.
- Rewrote `wps/Apple2026.wps` from scratch to a clean Apple2026 Now Playing baseline:
  - retained `156x156` centered art at `x=82,y=9`.
  - rebuilt metadata hierarchy lanes (`title y=172`, `artist y=192`) with SF-first fonts.
  - rebuilt progress/time lanes (`y=210/224`) with thin neutral rail and active-thickness state.
  - preserved lock/battery/status anchors and lossless/state overlays with Apple2026 tone.

### Verified
- Apple2026 runtime files now contain no Interpod donor header/comments or legacy scaffold blocks.
- SF-first font bindings remain active in both rewritten files:
  - `15-SFProText-Medium.fnt`
  - `16-SFProText-Medium.fnt`
  - `13-SFCompactText-Regular.fnt`
- WPS/SBS geometry now follows Wave 3 token locks directly rather than inherited partial structure.

### Docs
- `WORK_LOG.md` updated for this restart execution block.
- No new `DESIGN_SYSTEM.md` lock additions were required in this block (existing tokens were sufficient).

### Next
- Continue Wave 3 restart completion on remaining shell-wide surfaces:
  - loading/fallback/empty consistency pass in core/plugin surfaces,
  - Cover Flow UI alignment-only parity pass,
  - whole-shell consistency sweep and static drift checks against Apple2026 locks.

## 2026-04-07 (Wave 3 completion review snapshot: doc/source parity + readiness)

### 1) WHAT IS FULLY IMPLEMENTED
- Apple2026 runtime shell path is active (`wps/WPSLIST` with Apple2026 WPS/SBS + Apple2026 iconset).
- Apple2026 SF-first font bindings are wired in active Apple2026 surfaces:
  - `15-SFProText-Medium.fnt`, `16-SFProText-Medium.fnt`, `13-SFCompactText-Regular.fnt`.
- Apple2026 symbol/icon replacement pipeline is active:
  - `icons/Apple2026Icons.bmp` + generated `wps/Apple2026/*.bmp` symbol/runtime assets.
- WPS/SBS core composition lock is applied for current Apple2026 surface:
  - large-art WPS coordinates, thin neutral progress language, mini-player lanes, header anchors.
- Secondary/fallback no-build UI replacements applied:
  - splash loading grammar (`apps/gui/splash.c`), yes/no neutralization (`apps/gui/yesno.c`), USB centered takeover composition (`apps/gui/usb_screen.c`), theme separator default neutralization (`apps/menus/theme_menu.c`).
- Boot asset presence is ready in source tree:
  - `apps/bitmaps/native/rockboxlogo.320x98x16.bmp`.

### 2) WHAT IS PARTIALLY IMPLEMENTED
- Whole-shell parity across all listed user-facing surfaces is partially implemented:
  - major Apple2026 shell assets and core WPS/SBS language are in place,
  - but several secondary flows are only covered by prior audit/lock documentation rather than explicit per-surface implementation deltas in this slice.
- Cover Flow UI alignment is partially completed:
  - loading + tracklist selection/progress language updated,
  - broader header/label parity still requires final runtime visual confirmation.
- Design-lock closure is completed in docs, but full visual completion gate is partial without fresh simulator screenshot set against local SVG references.

### 3) WHAT IS STILL MISSING
- Full runtime completion evidence pass (no-build mode was honored):
  - simulator screenshot checkpoint set for all Wave 3 surfaces is still pending in this continuation.
- Explicit final parity pass remains for some secondary user-facing surfaces called out by section 67 audit matrix (playlist/queue/search/radio/shortcuts/deeper settings/filesystem states) to confirm they visually match Apple2026 shell language end-to-end.
- Final stress-matrix (`S1..S7`) screenshot evidence across all required surfaces is still missing in this pass.

### 4) WHERE INHERITED ASSETS STILL REMAIN
- Global platform defaults still reference legacy baseline in core settings code (`apps/settings_list.c` default font macro uses Adobe baseline), outside Apple2026 theme override path.
- In Apple2026 runtime path specifically (`wps/Apple2026*`), current sweeps show no remaining donor references for:
  - `Interpod`, `amusicpod`, `Adobe-Helvetica`, `vb_backdrop/pb_backdrop` legacy dependency strings.

### 5) BIGGEST REMAINING VISUAL OR UX GAPS
- Final whole-product consistency proof is blocked by missing fresh visual evidence run:
  - direct side-by-side confirmation vs local SVG exports for all required surfaces is not yet logged in this no-build slice.
- Secondary-surface UX continuity still needs final implementation-confirmation sweep (not just audit state), especially around playlist/radio/search/context transitions.

### 6) WHETHER THE SHELL IS READY FOR BUILD AND TEST
- **Yes, ready for build and test checkpoint.**
- Source-side Apple2026 font/icon/boot/WPS dependencies required for packaging are present.
- Final “fully complete and native-coherent” sign-off still depends on post-build simulator/device screenshot verification across all required surfaces.

## 2026-04-07 (Wave 3 continuation: lock-resolution + dependency readiness pass)

### Scope
- Continued Wave 3 from current implementation state (no reset).
- Resolved remaining in-doc numeric lock ambiguity and aligned Apple2026 runtime defaults/dependency checks.

### Changes applied
- `DESIGN_SYSTEM.md`
  - resolved section `33.3` checklist to explicit completed state tied to section `33.2`,
  - normalized scrollbar numeric lock wording to match token table (`2px` width, subtle inset behavior).
- `wps/WPSLIST`
  - Apple2026 main default font updated from `16-SFProText-Medium.fnt` to `15-SFProText-Medium.fnt` to match current lock hierarchy for list/header baseline.
- Static dependency-readiness verification executed:
  - confirmed Apple2026 iconset path exists: `icons/Apple2026Icons.bmp`,
  - confirmed Apple2026 required fonts exist: `15/16-SFProText-Medium.fnt`, `13-SFCompactText-Regular.fnt`, `22-SFCompactDisplay-Semibold.fnt`,
  - confirmed boot logo asset exists: `apps/bitmaps/native/rockboxlogo.320x98x16.bmp`,
  - confirmed WPS/SBS referenced Apple2026 bitmap/font dependencies are present (scripted check result: `ALL_APPLE2026_WPS_DEPENDENCIES_PRESENT`).
- Local SVG reference set presence verified for completion comparison:
  - `Sourced Icons/Figma File as SVG.svg`
  - `Sourced Icons/iOS Stocks 07  —◇ Light 🍏.svg`

### Verified (static)
- Apple2026 theme registration remains active and SF-first in `wps/WPSLIST`.
- Apple2026 icon/symbol and font runtime paths required by current WPS/SBS are present.
- No build run in this slice (per no-build instruction), but packaging-critical dependency files are present in source tree.

### Build/runtime status
- Skipped intentionally in this cycle (user-requested no-build mode).

### Files touched
- `DESIGN_SYSTEM.md`
- `wps/WPSLIST`
- `WORK_LOG.md`

### Next action
- Continue Wave 3 completion pass with explicit whole-shell completion review against:
  - `DESIGN_SYSTEM.md`,
  - `MASTER.md`/`AGENTS.md`,
  - local SVG exports,
  - active source hierarchy surfaces (Home/Music/Artists/Albums/Songs/Playlists/Settings/Files/mini-player/WPS/loading/fallback/boot/Cover Flow UI + tracklist).

## 2026-04-07 (Wave 3 continuation: theme default separator neutralization)

### Scope
- Continued Wave 3 secondary/fallback consistency pass.
- Normalized theme reset default separator color to Apple2026 neutral baseline.

### Changes applied
- `apps/menus/theme_menu.c`
  - updated `reset_color()` list separator default:
    - from `LCD_DARKGRAY`
    - to `LCD_LIGHTGRAY`
  - rationale: aligns reset/default behavior with Apple2026 subtle separator direction used in shell/list language.

### Verified (static)
- Change is isolated to theme color reset default and does not alter menu wiring or browse flow.
- IDE includePath squiggle (`3ds/types.h`) is environment-indexing noise, unrelated to this edit.

### Build/runtime status
- Skipped intentionally in this cycle (user-requested no-build mode).

### Files touched
- `apps/menus/theme_menu.c`
- `WORK_LOG.md`

### Next action
- Continue Wave 3 secondary-surface completion checks on remaining user-facing non-plugin paths and lock final parity set.

## 2026-04-07 (Wave 3 continuation: USB takeover surface Apple2026 alignment)

### Scope
- Continued Wave 3 no-build secondary-surface replacement pass.
- Applied Apple2026 shell continuity to USB takeover surface.

### Changes applied
- `apps/gui/usb_screen.c`
  - centered USB logo horizontally within themed parent viewport (replacing right-edge anchoring),
  - keeps existing behavior and HID/title flow unchanged while aligning composition to Apple2026 centered-shell language.

### Verified (static)
- File-level targeted sweep and prior active-surface sweeps remain clean for legacy/donor visual references in modified Apple2026 surfaces.

### Build/runtime status
- Skipped intentionally in this cycle (user-requested no-build mode).

### Files touched
- `apps/gui/usb_screen.c`
- `WORK_LOG.md`

### Next action
- Continue remaining Wave 3 secondary parity checks (playlist/queue/search/radio wording/layout consistency) under Apple2026 token/style lock.

## 2026-04-07 (Wave 3 continuation: fallback/dialog loading-grammar alignment)

### Scope
- Continued Wave 3 no-build implementation for core fallback/dialog surfaces.
- Removed remaining non-Apple2026 visual language in splash/yesno flows and validated active-surface legacy sweep.

### Changes applied
- `apps/gui/splash.c`
  - shifted splash panel spacing to calmer Apple2026 geometry (`RECT_SPACING` from `3` -> `8`),
  - replaced scrollbar-based progress meter with Apple2026 thin neutral rail (`2px`) and darker active fill,
  - replaced vararg talk macros in this unit with explicit `talk_idarray` calls for compatibility-safe voice prompts.
- `apps/gui/yesno.c`
  - replaced saturated touchscreen yes/no frame colors (green/red) with Apple2026 neutral grays:
    - yes frame: `LCD_RGBPACK(208,208,212)`
    - no frame: `LCD_RGBPACK(158,158,166)`

### Verified (static)
- Targeted legacy/style sweep returned no matches in active modified surfaces:
  - query included legacy asset refs (`vb_backdrop`, `pb_backdrop`, donor/font strings) and saturated yes/no frame colors.
  - scope: `apps/gui/splash.c`, `apps/gui/yesno.c`, `apps/plugins/pictureflow/pictureflow.c`
  - result: `0` matches.

### Build/runtime status
- Skipped intentionally in this cycle (user-requested no-build mode).

### Files touched
- `apps/gui/splash.c`
- `apps/gui/yesno.c`
- `WORK_LOG.md`

### Next action
- Continue Wave 3 completion on remaining secondary surface parity (playlist/queue/search/radio/usb/fallback wording consistency) under Apple2026 token/style lock.

## 2026-04-07 (Wave 3 continuation: Apple2026 replacement-first shell enforcement)

### Scope
- Continued Wave 3 implementation in no-build mode.
- Enforced Apple2026-first replacement behavior for remaining inherited visual language in active shell surfaces.

### Changes applied
- Regenerated Apple2026 symbol/runtime assets from SF-symbol sources after generator update:
  - command: `python tools/apple2026_symbol_assets.py`
  - includes new generated runtime glyph strip:
    - `wps/Apple2026/losslessIndicator.bmp`
- WPS/SBS replacement-first cleanup:
  - `wps/Apple2026.wps`
    - removed bitmap-backed volume rail dependencies (`vb_backdrop.bmp`, `pb_backdrop.bmp`, `pb_active_backdrop.bmp` usage path),
    - switched volume lane to Apple2026 thin neutral token language (`2px` rail, neutral inactive + darker active behavior).
  - `wps/Apple2026.sbs`
    - removed `vb_backdrop.bmp` dependency from preload path,
    - switched volume rail to thin neutral token-driven bar rendering.
- Cover Flow UI-only alignment (`apps/plugins/pictureflow/pictureflow.c`):
  - tracklist selected row now uses flat neutral-gray fill (no gradient),
  - tracklist loading state now adds centered thin neutral indicator under wait label,
  - progress bar rendering tightened to Apple2026 thin neutral rail language (`2px`, neutral base + darker active fill).

### Verified (static)
- Apple2026 SF-first font bindings remain active in theme surfaces:
  - `15-SFProText-Medium.fnt`
  - `16-SFProText-Medium.fnt`
  - `13-SFCompactText-Regular.fnt`
- Legacy reference sweep across active Apple2026 shell files returned no matches for inherited paths:
  - search target: `vb_backdrop|pb_backdrop|pb_active_backdrop|vb.bmp|pb.bmp|Adobe-Helvetica|Inter-|Ubuntu-|amusicpod|Interpod`
  - scope: `wps/Apple2026*`
  - result: `0` matches.

### Build/runtime status
- Skipped intentionally in this cycle (user-requested no-build mode).

### Files touched
- `tools/apple2026_symbol_assets.py`
- `icons/Apple2026Icons.bmp`
- `wps/Apple2026/*.bmp` (regenerated, including `losslessIndicator.bmp`)
- `wps/Apple2026.wps`
- `wps/Apple2026.sbs`
- `apps/plugins/pictureflow/pictureflow.c`
- `WORK_LOG.md`

### Next action
- Continue Wave 3 surface completion pass across remaining must-fix secondary/fallback surfaces (playlist/queue/loading/dialog consistency) using Apple2026 token locks, with no fallback to inherited assets unless hard-fail.

## 2026-04-07 (Wave 3 direct replacement pass: Apple2026 symbol-first assets)

### Scope
- Continued Wave 3 implementation with no build run (per current no-build instruction).
- Replaced inherited icon/symbol assets in active Apple2026 path with generated SF-symbol-derived assets.

### Changes applied
- Executed Apple2026 symbol generator:
  - `tools/apple2026_symbol_assets.py`
- Regenerated primary iconset directly from `Sourced Icons/sf-symbols-master/glyphs`:
  - `icons/Apple2026Icons.bmp`
- Regenerated WPS/SBS runtime symbol assets from SF symbols:
  - `wps/Apple2026/playerStatus.bmp`
  - `wps/Apple2026/holdSlider.bmp`
  - `wps/Apple2026/sleep.bmp`
  - `wps/Apple2026/repeat.bmp`
  - `wps/Apple2026/repeatOne.bmp`
  - `wps/Apple2026/repeatShuffle.bmp`
  - `wps/Apple2026/shuffle.bmp`
  - `wps/Apple2026/speaker_loud.bmp`
  - `wps/Apple2026/speaker_mute.bmp`
  - `wps/Apple2026/speaker_too_loud.bmp`
  - `wps/Apple2026/albumFramed.bmp`
  - `wps/Apple2026/albumPlaceholder.bmp`
  - `wps/Apple2026/wpsBackdrop.bmp`
  - `wps/Apple2026/usbBackdrop.bmp`
- Extended generator coverage so inherited runtime fallback sprites are now generated Apple2026-first:
  - `wps/Apple2026/repeatAB.bmp`
  - `wps/Apple2026/busyIndicator.bmp`
  - `wps/Apple2026/batteryStatus.bmp`

### Verified (static)
- Apple2026 theme files already remain on SF-first font bindings:
  - `15-SFProText-Medium.fnt`
  - `16-SFProText-Medium.fnt`
  - `13-SFCompactText-Regular.fnt`
- Regeneration command completed successfully with explicit output list for all replaced assets.

### Build/runtime status
- Skipped intentionally in this cycle (user-requested no-build mode).

### Files touched
- `tools/apple2026_symbol_assets.py`
- `icons/Apple2026Icons.bmp`
- `wps/Apple2026/*.bmp` (regenerated symbol/runtime assets listed above)
- `WORK_LOG.md`

### Next action
- Continue Wave 3 shell pass by replacing remaining inherited visual assets/surfaces where Apple2026 mapping is already locked (no audit-only pause).

## 2026-04-07 (Wave 3 grouped static continuation: core shell + WPS + boot integration)

### Scope
- Continued Wave 3 implementation with **builds intentionally skipped** per user instruction.
- Focused on Sub-wave 3.2/3.4/3.5/3.6 static edits only.

### Changes applied
- Core shell / SBS refinement (`wps/Apple2026.sbs`):
  - added mini-player metadata lane and shifted track/meta layout to preserve right control lane,
  - stabilized mini-player play/pause box behavior,
  - tightened header visual treatment to calmer light-shell top bar,
  - preserved locked anchor behavior for title, lock, battery text/icon, and busy indicator.
- WPS refinement (`wps/Apple2026.wps`):
  - moved to large-art-first layout (`156x156` frame at `x=82,y=9`),
  - aligned title/artist/progress/time lanes to locked Apple2026 Wave 3 coordinates,
  - switched progress rails to thin neutral bars with active state thickness behavior,
  - kept Apple2026 accent token (`#FF2D55`) for artist/accent emphasis.
- Theme cfg-generation support (`wps/wpsbuild.pl`):
  - added parser/output support for `scrollbar` and `scrollbar width` so these tokens are preserved in generated theme cfgs.
- Theme token wiring (`wps/WPSLIST`):
  - Apple2026 now explicitly binds `scrollbar: right`, `scrollbar width: 2`, and subtle separators.
- Cover Flow UI alignment (`apps/plugins/pictureflow/pictureflow.c`):
  - updated loading progress bar styling to Apple2026-neutral language,
  - removed bright green fill,
  - reduced bar thickness and framing to a calm thin indicator,
  - centered width constrained to Apple2026 loading max-width behavior.

### Boot/startup integration (Sub-wave 3.6 static)
- Added iPod S5L boot visual helper in `bootloader/ipod-s5l87xx.c`:
  - centered logo draw on black background,
  - removed default startup banner text from normal startup path,
  - retained fatal-error text paths unchanged.
- Added deterministic boot asset generator:
  - `tools/apple2026_boot_asset.py`
  - source: `Sourced Icons/iPhone-Rebooting.png`
  - output: `apps/bitmaps/native/rockboxlogo.320x98x16.bmp`
- Generated preview artifact for visual inspection:
  - `screenshots/apple2026-boot-logo-320x98-preview.png`

### Design-system contract update
- Resolved WPS geometry ambiguity by updating `DESIGN_SYSTEM.md` section `54.6` to match section `70` refinement:
  - `wps_art_box_default = 156x156`
  - `wps_art_x = 82`
  - `wps_art_y = 9`
  - `wps_title_y = 172`
  - `wps_artist_y = 192`
  - progress/time lanes remain `210/224`.

### Verified (static)
- No reported syntax errors in edited text files via diagnostics checks.
- Boot logo preview confirms centered Apple mark on black for the generated 320x98 boot bitmap strip.

### Build/runtime status
- Skipped intentionally in this cycle (user-requested no-build mode).

### Files touched
- `wps/Apple2026.sbs`
- `wps/Apple2026.wps`
- `wps/WPSLIST`
- `wps/wpsbuild.pl`
- `bootloader/ipod-s5l87xx.c`
- `tools/apple2026_boot_asset.py`
- `apps/bitmaps/native/rockboxlogo.320x98x16.bmp`
- `screenshots/apple2026-boot-logo-320x98-preview.png`
- `DESIGN_SYSTEM.md`
- `apps/plugins/pictureflow/pictureflow.c`
- `WORK_LOG.md`

### Next action
- Continue static Sub-wave 3.3 + remaining 3.2/3.4 lane polish (icons/chevrons/selector/readability safety) before re-enabling Build 2.

## 2026-04-07 (Wave 3 Sub-wave 3.2/3.4 grouped static pass: core-shell anchors)

### Scope
- Post-Build-1 grouped static implementation pass for core shell anchors.
- No build executed in this entry (reserved for Build 2 bundle checkpoint).

### Changes applied
- Extended `wps/wpsbuild.pl` to support generated cfg output for:
  - `scrollbar`
  - `scrollbar width`
- Updated `Apple2026` block in `wps/WPSLIST`:
  - locked scrollbar direction/width (`right`, `2`)
  - subtle separator activation (`list separator height: 1`, neutral separator color)
- Updated `wps/Apple2026.sbs` core-shell lanes:
  - preserved locked header/title/battery text anchors,
  - adjusted battery icon box to fixed token anchor (`288,4,27,16`),
  - reduced mini-player top separator to a single hairline,
  - reserved right lane in mini-player track text to avoid control collision,
  - added explicit mini-player play/pause indicator lane (`x=284,w=20`) tied to playback state,
  - retained Apple2026 accent token in active micro-progress segment.

### Verified (static)
- No parse/lint errors reported for:
  - `wps/wpsbuild.pl`
  - `wps/WPSLIST`
  - `wps/Apple2026.sbs`

### Files touched
- `wps/wpsbuild.pl`
- `wps/WPSLIST`
- `wps/Apple2026.sbs`
- `WORK_LOG.md`

### Build/runtime status
- Not run in this entry.
- Next action: continue grouped shell bundle (remaining Sub-wave 3.2/3.3 elements) before **Build 2**.

## 2026-04-07 (Wave 3 BUILD 1 checkpoint: Apple2026 theme-path proof)

### Scope
- Build 1 only: validate Apple2026 runtime wiring after Sub-wave 3.1 scaffolding.
- No broad shell-polish pass in this checkpoint.

### Commands run (verified)
- `./build-sim.ps1` (initial Build 1 run)
- `./build-sim.ps1` (single rerun after font-reference stability adjustment)

### Build result
- Simulator wrapper reached install marker successfully:
  - `Installing your build in your 'simdisk' dir`
- No terminating `make: ***` fatal error in Build 1 flow.
- CRLF/git warning flood observed; treated as non-fatal noise per project rule.

### Runtime path proof (verified)
- `build-sim/simdisk/.rockbox/themes/Apple2026.cfg` exists
- `build-sim/simdisk/.rockbox/wps/Apple2026.wps` exists
- `build-sim/simdisk/.rockbox/wps/Apple2026.sbs` exists
- `build-sim/simdisk/.rockbox/icons/Apple2026Icons.bmp` exists
- Generated cfg wiring verified:
  - `wps: /.rockbox/wps/Apple2026.wps`
  - `sbs: /.rockbox/wps/Apple2026.sbs`
  - `iconset: /.rockbox/icons/Apple2026Icons.bmp`
  - flat selector + light shell tokens present.

### Simulator launch smoke-check (verified)
- `build-sim/rockboxui.exe` launched successfully (`pid` observed).

### Notes
- Prebuilt Wave 2 SF `.fnt` files are not auto-packaged by this current WPSLIST-driven path.
- For Build 1 stability, Apple2026 SBS/WPS currently use packaged baseline fonts (`15/16-Adobe-Helvetica`) while preserving Apple2026 geometry/state scaffolding.
- Dedicated custom-font packaging integration is queued for later grouped implementation work.

### Screenshots
- Not captured in this checkpoint (theme-path proof + launch smoke-check only).
- Screenshot capture is planned in grouped visual checkpoints (Build 2 and Build 3).

### Files touched
- `wps/WPSLIST`
- `wps/Apple2026.sbs`
- `wps/Apple2026.wps`
- `wps/Apple2026/*`
- `icons/Apple2026Icons.bmp`
- `WORK_LOG.md`

### Next action
- Proceed to Sub-wave 3.2/3.3/3.4 grouped core-shell implementation before Build 2.

## 2026-04-07 (Apple2026 icon-system audit + correction pass)

### Verified
- Runtime main icon strip: `icons/Apple2026Icons.bmp` (32× `Icon_Audio`…`Icon_Rockbox`); WPS/SBS assets under `wps/Apple2026/`; defaults in `apps/settings_list.c` use `viewers.6x8x16` (packaged viewer strip, not SF-normalized in-tree).
- DESIGN_SYSTEM §33.2 battery box `27×16` conflicted with prior `batteryStatus.bmp` frames `27×19` (vertical clip in `%Vl(battery_icon,…,27,16,…)`).

### Changed
- `tools/apple2026_symbol_assets.py`: LANCZOS downscale for glyphs; battery redrawn to `27×16`×10 frames; repeat shuffle/AB use distinct SF names with file-based fallback; `speaker_mute` normalized to `19×17` to match loud tile; busy `rays` resize uses LANCZOS; rotate keeps BICUBIC (Pillow constraint).
- `wps/Apple2026.sbs`, `wps/Apple2026.wps`: `%xl(B,…)` vertical offset `0,4` → `0,0` so the 16px-tall strip is not pushed down inside the header viewport.
- Regenerated: `icons/Apple2026Icons.bmp`, `wps/Apple2026/*.bmp` from the script.

### Next
- Simulator pass: header battery, repeat overlays, volume row alignment; optional future `Apple2026`-styled `viewers` strip + `.icons` to replace legacy `viewers.6x8x16`.

## 2026-04-07 (Wave 3 Sub-wave 3.1 scaffold implementation: theme path wiring)

### Scope
- Implemented Apple2026 theme-path scaffolding only (no broad shell polish yet).
- Objective: make runtime theme loading path valid for Build 1 proof.

### Changes applied
- Registered a new `Apple2026` theme block in `wps/WPSLIST` with explicit `320x240` bindings:
  - `wps.320x240x(16|24|32): Apple2026.wps`
  - `sbs.320x240x(16|24|32): Apple2026.sbs`
  - iconset binding to `icons/Apple2026Icons.bmp`
  - flat selector color model and light-shell base tokens.
- Added new scaffold files:
  - `wps/Apple2026.sbs` (Interpod-derived structural baseline)
  - `wps/Apple2026.wps` (Interpod-derived WPS baseline)
- Wired prepared Wave 2 SF fonts into scaffold skin files:
  - `15-SFProText-Medium.fnt`
  - `16-SFProText-Medium.fnt`
  - `13-SFCompactText-Regular.fnt`
- Updated scaffold accent usage to Apple2026 token (`#FF2D55`) in active SBS/WPS color spots.

### Asset staging completed
- Created `wps/Apple2026/` and staged bitmap bundle from Interpod donor assets.
- Added Apple2026 iconset source to root icons path:
  - `icons/Apple2026Icons.bmp`
- Staged prepared SF `.fnt` assets into `fonts/` for runtime availability.

### Files touched
- `wps/WPSLIST`
- `wps/Apple2026.sbs`
- `wps/Apple2026.wps`
- `wps/Apple2026/*` (staged scaffold bitmaps)
- `icons/Apple2026Icons.bmp`
- `fonts/15-SFProText-Medium.fnt`
- `fonts/16-SFProText-Medium.fnt`
- `fonts/13-SFCompactText-Regular.fnt`
- `fonts/22-SFCompactDisplay-Semibold.fnt`
- `WORK_LOG.md`

### Build/runtime status
- Not run yet in this entry.
- Next action: execute **Build 1** for theme-path proof.

## 2026-04-07 (Wave 3 Sub-wave 3.0 preflight + strict execution plan lock)

### Scope
- Wave 3 preflight only, before broad shell implementation edits.
- Confirmed branch, required-read order, asset/path readiness, and execution sequence.

### Required-read order (verified)
1. `MASTER.md`
2. `AGENTS.md`
3. `DESIGN_SYSTEM.md` (full file)
4. `WORK_LOG.md`

### Branch state (verified)
- Active branch: `Wave3`

### Preflight checks (verified)
- Theme registration source file exists: `wps/WPSLIST`
- Runtime target folders exist:
  - `build-sim/simdisk/.rockbox/themes`
  - `build-sim/simdisk/.rockbox/wps`
  - `build-sim/simdisk/.rockbox/icons`
  - `build-sim/simdisk/.rockbox/fonts`
- Required Wave 3 source assets exist:
  - `Sourced Icons/iPhone-Rebooting.png`
  - `Imported Reference Themes/Interpod/.rockbox/icons/InterNightPod.bmp`
  - `Imported Reference Themes/Interpod 2/.rockbox/wps/Interpod 2.sbs`
  - `Imported Reference Themes/Interpod 2/.rockbox/wps/Interpod 2.wps`
- Prepared Apple2026 font outputs confirmed:
  - `Apple Fonts/Generated FNT/Apple2026/SFCompactDisplay/Semibold/22-SFCompactDisplay-Semibold.fnt`
  - `Apple Fonts/Generated FNT/Apple2026/SFProText/Medium/16-SFProText-Medium.fnt`
  - `Apple Fonts/Generated FNT/Apple2026/SFProText/Medium/15-SFProText-Medium.fnt`
  - `Apple Fonts/Generated FNT/Apple2026/SFCompactText/Regular/13-SFCompactText-Regular.fnt`

### Wave 3 execution plan locked for this run
1. Sub-wave 3.1
   - Register `Apple2026` in `wps/WPSLIST`
   - Add Apple2026 scaffolding files in `wps/` (`.cfg` via WPSLIST generation path, `.sbs`, `.wps`)
   - Wire minimum fonts/icons/assets for runtime loading
2. **BUILD 1** (theme path proof only)
3. Sub-wave 3.2 + 3.3 + 3.4 grouped implementation
   - Header, list geometry, lanes, selector, chevrons, scrollbar, icon integration, mini-player
4. **BUILD 2** (core shell checkpoint)
5. Sub-wave 3.5 + 3.6 + 3.7 grouped implementation
   - WPS, boot image path, loading/fallback shelling, Cover Flow UI alignment only
6. Sub-wave 3.8 whole-shell consistency pass
7. **BUILD 3** (full visual checkpoint)

### Build cadence lock (strict)
- No incremental rebuild churn between grouped bundles.
- Only Build 1 / Build 2 / Build 3 unless a true blocker requires emergency validation.

### Files touched
- `WORK_LOG.md`

### Build/runtime status
- Not run in this entry (preflight/documentation lock only).

## 2026-04-06 (Post-fix confidence rerun: canonical wrappers remain green)

### Scope
- Fast confidence rerun only (no new build-system code changes).
- Canonical path re-validated on Native Windows + MSYS2 UCRT64 wrappers.

### Commands run (verified)
- `./build-sim.ps1` -> exit code `0`
- `./build-hw.ps1 -Target ipod6g` -> exit code `0`

### Artifact checks (verified)
- `build-sim/simdisk/.rockbox/rockbox-info.txt` exists, size `879`, mtime `2026-04-06 22:13:18`
- `build-sim/rockboxui.exe` exists, size `9995481`, mtime `2026-04-06 22:12:22`
- `build-hw-ipod6g/rockbox.zip` exists, size `7855897`, mtime `2026-04-06 22:21:00`
- `build-hw-ipod6g/rockbox.ipod` exists, size `764788`, mtime `2026-04-06 22:19:20`

### Notes
- CRLF/git warning flood remains present in logs and was again non-fatal in successful wrapper runs.
- No canonical-path regression detected in this confidence pass.

### Files touched
- `WORK_LOG.md`

## 2026-04-06 (Top-level MCP Figma URL lock in design docs)

### Scope
- Documentation-only update to ensure the requested Figma reference URL is explicitly saved at top level in design docs.

### Update applied
- Added a new top-level entry in `DESIGN_SYSTEM.md`:
  - `## 3A) Top-level MCP Figma reference`
  - `mcp_com_figma_mcp_use_figma`: `https://www.figma.com/design/JyDHVsT9bqCPjbGEfyUD8d/Apple-Music-UI?node-id=35-9972&t=rOOHiOTV8SE8HN2c-4`
  - locked node mapping: `35:9972`

### Notes
- Direct browser fetch of this Figma URL is WebGL-gated in-session; URL/value was still preserved exactly in docs.

### Files touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build/runtime status
- Not run in this pass (documentation-only change).

## 2026-04-06 (Wave 2 focused Now Playing reference-ingestion pass: iOS current-player)

### Scope
- Wave 2 design-system pass only (no broad implementation).
- Added one critical Apple Music Now Playing reference to improve WPS/mini-player/control language.

### Reference inputs
- Figma URL (node `35:9972`) was provided but not directly renderable via browser scrape in-session.
- User-supplied exported SVG used as authoritative geometry source:
  - `Sourced Icons/iOS Stocks 07  —◇ Light 🍏.svg`
- Interpod lineage re-checked for adaptation grounding:
  - `Imported Reference Themes/Interpod/.rockbox/wps/Interpod.wps`
  - `Imported Reference Themes/Interpod 2/.rockbox/wps/Interpod 2.wps`
  - `Imported Reference Themes/amusicpodds/.rockbox/wps/amusicpodds.wps`
  - corresponding `.sbs` files for lower-shell/volume behavior.

### What was extracted from the new reference
- Portrait iPhone composition (`390x844`) with large near-full-width art card.
- Strong title/artist hierarchy below art.
- Triad transport controls with center play/pause optical emphasis.
- Thin progress rail + compact thumb language.
- Secondary volume slider language and lower action row semantics.

### Design-system lock update
- Added `## 70) Current iOS Now Playing ingestion lock (Figma node 35:9972 via SVG reference)` to `DESIGN_SYSTEM.md`.
- New section includes:
  - preserve/adapt/reject rules,
  - iPod-form-factor compression strategy,
  - Interpod vs Interpod2 vs amusicpodds adaptation mapping,
  - playback control and slider implementation-facing token rules,
  - explicit inherit/reject matrix for Wave 3 execution.

### Outcome
- Apple2026 Now Playing direction is now explicitly informed by current Apple Music language,
  but translated to iPod constraints and Interpod strengths instead of literal phone-copy layout.

### Files touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build/runtime status
- Not run in this pass (design-system lock update only).

## 2026-04-06 (Wave 2 full-glyph review completion + environment-step freeze workaround)

### Scope
- Complete the requested all-glyph review for prepared Apple2026 fonts.
- Keep scope within Wave 2 typography/font readiness only.

### Execution issue observed
- The Python environment handoff step repeatedly appeared frozen/blocked in-session
  (configure action cancelled/stalled), preventing forward progress when used as the sole path.

### Workaround and completion path
- Executed the generator/audit directly with the known working interpreter and MSYS runtime PATH:
  - `C:/Python314/python.exe tools/apple2026_font_prep.py`
- Regeneration completed successfully (`144/144`) and produced all inventory/preview/glyph artifacts.

### Glyph-audit bug fixes applied
- Updated `tools/apple2026_font_prep.py` glyph integrity logic to avoid false rejects from
  non-printing codepoints in the scanned range.
- Added printable-glyph gate for integrity checks and excluded expected blank codepoints
  (`U+0020`, `U+00A0`, `U+00AD`, control range handling).

### Final all-glyph result (verified)
- `APPLE2026_GLYPH_AUDIT.md` summary after rerun:
  - Primary: `144`
  - Fallback: `0`
  - Reject: `0`
- Per-font full glyph sheets present under:
  - `Apple Fonts/Generated FNT/Apple2026/glyph_sheets/`

### Files touched
- `tools/apple2026_font_prep.py`
- `DESIGN_SYSTEM.md` (added section 69 full-glyph audit lock)
- `WORK_LOG.md`

### Build/runtime status
- Font-generation/audit runtime completed successfully in this pass.
- No broad simulator/hardware shell changes executed in this pass.

## 2026-04-06 (Wave 1 strict build-environment recovery pass)

### Scope
- Strict build-readiness recovery only (no UI/theme/performance work).
- Canonical path locked to **Native Windows + MSYS2 UCRT64**.
- Target focus locked to **ipod6g** (+ simulator) per current project need.

### Failure isolation results (verified)
- Reproduced build in direct MSYS2 bash (no wrapper abstraction) and captured full logs.
- Observed `make: Circular ... dependency dropped` and CRLF/git warning flood; verified these are noisy and non-fatal in successful runs.
- Identified real wrapper-level issue:
  - `build-hw.ps1` could stop early on benign stderr (`NativeCommandError`) before exit-code evaluation.

### Corrective action applied (smallest fix)
- Updated wrappers to evaluate success by process exit code instead of treating stderr text as fatal:
  - `build-hw.ps1`
  - `build-sim.ps1`
- Kept explicit non-zero exit enforcement (`throw` on non-zero) to preserve fail-fast behavior on true errors.

### Validation (verified)
- Direct canonical shell build succeeded:
  - command: `./build-hw.sh ipod6g` (inside MSYS2 UCRT64 bash)
  - package marker present: `ZIP rockbox.zip`
- Wrapper parity build succeeded after fix:
  - command: `./build-hw.ps1 -Target ipod6g`
  - exit code: `0`
- Simulator wrapper build succeeded after fix:
  - command: `./build-sim.ps1`
  - install marker present: `Installing your build in your 'simdisk' dir`

### Artifact state (verified)
- `build-hw-ipod6g/rockbox.zip` exists
- `build-hw-ipod6g/rockbox.ipod` exists
- `build-hw-ipod6g/rockbox-info.txt` exists
- `build-sim/rockboxui.exe` exists
- `build-sim/simdisk/.rockbox/rockbox-info.txt` exists

### Documentation alignment updates
- Updated `Pasted markdown (3).md` (`PROJECT RESET HANDOFF MASTER`) to match actual repo reality:
  - removed stale `tools/windows-shims/*` references,
  - switched to root wrappers (`build-hw.ps1`, `build-sim.ps1`),
  - corrected expected artifacts,
  - added warning-noise vs fatal-error guidance.

### Current canonical status
- **Canonical path is green:** Native Windows + MSYS2 UCRT64.
- Do not pivot to WSL unless canonical path regresses with true repeatable fatal errors.

## 2026-04-06 (Wave 2 font-asset preparation and readiness pass)

### Scope
- Strict Wave 2 pass focused only on typography asset readiness before broad Wave 3 shell work.
- No broad shell implementation started.

### Actions completed
- Re-audited typography lock in `DESIGN_SYSTEM.md` (sections 27, 28, 29, 65, 66).
- Re-inventoried approved source families under `Apple Fonts/`:
  - SF Pro Display/Text
  - SF Compact Display/Text
  - New York (reviewed, not used for primary generation set)
- Built and validated converter path (`tools/convttf.exe`) in MSYS2 UCRT runtime.
- Added automated generation/analysis script:
  - `tools/apple2026_font_prep.py`
- Generated broad curated Apple2026 `.fnt` library into:
  - `Apple Fonts/Generated FNT/Apple2026/`

### Output summary
- Total generated candidates: `144`
- Quality status:
  - `140` primary
  - `3` fallback
  - `1` reject
- Family coverage:
  - SF Pro Display (`19-25`, Regular/Medium/Semibold/Bold)
  - SF Compact Display (`19-25`, Regular/Medium/Semibold/Bold)
  - SF Pro Text (`10-19`, Regular/Medium/Semibold/Bold)
  - SF Compact Text (`8-19`, Regular/Medium/Semibold/Bold)

### Quality/readability validation
- Applied automated readability filters (density, punctuation clarity, digit width floors,
  micro-size heavy-weight risk).
- Generated role preview sheets for all required typography roles:
  - `Apple Fonts/Generated FNT/Apple2026/previews/role_*.png`
- Explicit reject in this pass:
  - `08-SFCompactText-Bold.fnt` (`micro_bold_muddy_risk`)

### Documentation outputs
- Added Wave 2 font readiness lock section to `DESIGN_SYSTEM.md`:
  - `## 68) Wave 2 font-asset preparation lock (font-system readiness)`
- Generated inventory artifacts:
  - `Apple Fonts/Generated FNT/Apple2026/APPLE2026_FONT_INVENTORY.md`
  - `Apple Fonts/Generated FNT/Apple2026/apple2026_font_inventory.csv`
  - `Apple Fonts/Generated FNT/Apple2026/apple2026_font_inventory.json`

### Next task queue
1. Use this prepared set as default source for Wave 3 surface-by-surface font picks.
2. Avoid ad hoc regeneration unless a hard requirement falls outside prepared ranges.
3. Keep fallback Inter/Ubuntu `.fnt` files as contingency only.

## 2026-04-06 (Wave 2 full user-facing runtime audit pass)

### Scope
- Performed one more major whole-product Wave 2 audit focused on all user-facing
  surfaces reachable while running Rockbox.
- Audit includes core shell, plugin, dialog, fallback, USB, and boot/startup-adjacent
  user-visible paths.

### Evidence basis (code-level)
- Core runtime/navigation: `apps/root_menu.c`, `apps/tree.c`, `apps/menus/main_menu.c`,
  `apps/menus/settings_menu.c`
- Playback/music paths: `apps/gui/wps.c`, `apps/onplay.c`, `apps/tagtree.c`,
  `apps/playlist_viewer.c`, `apps/playlist_catalog.c`
- Modal/fallback: `apps/gui/splash.c`, `apps/gui/yesno.c`, `apps/lang/english.lang`
- Secondary surfaces: `apps/radio/radio.c`, `apps/radio/radio_skin.c`,
  `apps/shortcuts.c`, `apps/gui/usb_screen.c`
- Cover Flow UI/loading references: `apps/plugins/pictureflow/pictureflow.c`
- Boot/startup references: `bootloader/show_logo.c`, `bootloader/ipod-s5l87xx.c`

### DESIGN_SYSTEM update
- Added `## 67) Full user-facing runtime audit lock (Wave 2 whole-product pass)`
  to `DESIGN_SYSTEM.md`.
- Added required 8-part structure:
  1) full inventory,
  2) primary flagship surfaces,
  3) secondary surfaces,
  4) fallback/dialog/empty surfaces,
  5) biggest inconsistencies,
  6) Wave 3 must-fix surfaces,
  7) deferred surfaces,
  8) documentation update plan.

### Outcome
- Wave 2 now has explicit whole-product user-facing surface coverage, not only
  primary list/WPS shells.
- Must-fix and deferred boundaries are now documented for Wave 3 execution discipline.

### Build / runtime status
- Not run in this pass (audit/spec lock documentation pass only).

### Next task queue
1. Enforce section `67` as a mandatory coverage checklist during every Wave 3 validation step.
2. Resolve listed must-fix inconsistencies before claiming any whole-product UI pass complete.
3. Keep deferred items blocked per branch boundaries.

## 2026-04-06 (Wave 2 high-risk string stress-matrix lock pass)

### Scope
- Continued Wave 2 planning only.
- Added a strict pre-implementation stress-matrix layer for long-string readability,
  truncation behavior, and lane collision prevention.

### What was added
- Added `## 66) High-risk string stress matrix (pre-implementation lock)` to `DESIGN_SYSTEM.md`.
- Defined canonical stress IDs (`S1..S7`) spanning long title/artist/album/playlist/file labels,
  punctuation edge cases, and missing-metadata fallback labels.
- Mapped stress-string application across key surfaces:
  - Home/Artists/Albums/Songs/Playlists,
  - Settings/Files,
  - album page tracklist,
  - WPS/mini-player,
  - Cover Flow tracklist,
  - loading/fallback states.
- Added explicit stress pass/fail gates and screenshot evidence requirements tied to stress IDs.

### Intent
- Ensure Wave 3 starts with predefined clipping/overlap stress tests,
  reducing implementation-time guesswork and regressions.

### Files touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build / runtime status
- Not run in this pass (planning/spec lock only).

### Next task queue
1. Enforce section 66 stress checks at every Wave 3 validation step.
2. Record stress IDs (`S1..S7`) in screenshot notes for pass/fail evidence.
3. Block progression for any surface failing stress checks until corrected.

## 2026-04-06 (Wave 2 readability/overlap prevention guardrails pass)

### Scope
- Extended Wave 2 planning with explicit UI/UX best-practice implementation guardrails.
- No broad runtime/theme implementation changes in this pass.

### What was added
- Added a dedicated safety-lock section to `DESIGN_SYSTEM.md`:
  - `## 65) UI/UX best-practice safety lock (readability + overlap prevention)`
- New mandatory rule groups include:
  - readability floors by surface,
  - overlap-prevention hard-fail criteria,
  - truncation/marquee priority logic,
  - spacing minimums and lane clearances,
  - hierarchy safety for small-screen adaptation,
  - simulator validation best practices,
  - device-realism safety bias rules.

### Intent
- Reduce Wave 3 guesswork around readability and layout safety.
- Prevent polished-shell regressions (crowding, clipping, lane collisions, muddy text) before implementation scaling.

### Files touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build / runtime status
- Not run in this pass (planning/spec lock only).

### Next task queue
1. Enforce section 65 gates as part of Wave 3 step validation.
2. Treat any overlap/clipping event as fail until corrected.
3. Keep screen-by-screen evidence logging tied to sections 58-65.

## 2026-04-06 (Wave 2 implementation-grid expansion pass)

### Scope
- Continued strict Wave 2 planning without broad implementation edits.
- Expanded pre-implementation planning from token list to full execution blueprint.

### Planning expansions completed
- Converted isolated geometry tokens into a full implementation grid system:
  - global shell grid,
  - header grid,
  - list row grid,
  - icon/text/chevron/scrollbar lane grid,
  - mini-player grid,
  - WPS grid,
  - Cover Flow label/loading grid,
  - boot/loading grid.
- Added screen-by-screen token application map for:
  - Home/Music/Artists/Albums/Songs/Playlists,
  - Settings/Files,
  - album list/album page,
  - WPS/mini-player,
  - Cover Flow browse/loading/tracklist,
  - fallback/loading/boot surfaces.
- Added exact file-by-file Wave 3 execution order with dependency gates and deferred-file policy.
- Added per-step validation checklist with capture requirements, pass/fail criteria, and common mistake checks.
- Added stricter boot/loading implementation workflow boundaries (theme-driven vs code-driven).
- Added full user-flow coherence rules and hard drift-prevention rule set.

### DESIGN_SYSTEM additions
- Added sections `58` through `64` to `DESIGN_SYSTEM.md`.

### Files touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build / runtime status
- Not run in this pass (planning/spec lock only).

### Next task queue
1. Use sections `58-64` as mandatory execution checklist when Wave 3 scaffolding starts.
2. Block any surface implementation lacking explicit lane/token mapping.
3. Validate each Wave 3 step with required screenshot set before continuing.

## 2026-04-06 (Wave 2 strict pre-implementation exactness pass)

### Scope
- Pre-implementation planning only (no broad Wave 3 theme implementation edits).
- Goal: remove coordinate/layout ambiguity before Wave 3 begins.

### What was audited
- Rockbox theme construction model:
  - cfg/wps/sbs/fms responsibilities,
  - viewport and conditional layout patterns,
  - limits of theme control vs core rendering.
- Sourced implementation patterns:
  - Interpod, Interpod 2, amusicpodds construction methods,
  - PictureFlow plugin draw paths (`apps/plugins/pictureflow/pictureflow.c`),
  - core splash/loading behavior (`apps/gui/splash.c`),
  - target bootloader behavior (`bootloader/ipod-s5l87xx.c`, `bootloader/show_logo.c`).

### Exactness layer added to design lock
- Added strict pre-implementation sections `53` to `57` in `DESIGN_SYSTEM.md` covering:
  - construction-control mapping,
  - exact geometry tokens for `320x240`,
  - surface-by-surface prep constraints,
  - cropping/spacing/polish risk safeguards,
  - exact Wave 3 prep sequence and validation order.

### Boot image planning (required new asset)
- Asset: `Sourced Icons/iPhone-Rebooting.png`.
- Verified dimensions: `640x1136`.
- Locked workflow:
  - compose to black `320x240` canvas,
  - centered Apple logo,
  - no startup text,
  - avoid naive full-frame non-uniform scaling.

### Key no-drift contract
- If a coordinate/token/lane is not explicitly locked in `DESIGN_SYSTEM.md`,
  implementation for that surface is blocked.

### Files touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build / runtime status
- Not run in this pass (planning/spec lock only).

### Next task queue
1. Execute only Wave 3 scaffolding steps that map to locked sections 53-57.
2. Keep per-surface screenshot validation tied to token table before accepting implementation changes.
3. Block any layout improvisation not backed by explicit tokens.

## 2026-04-07 (Wave 0 packaging/install stabilization)

### Scope
- Continued native Windows/MSYS2 stabilization after compile-path recovery.
- Targeted the remaining packaging/install failure (`rockpod/-`, `fonts/-`) without introducing Wave 3 UI changes.

### Root Cause (Verified)
- `wps/wpsbuild.pl` had fragile value handling in theme asset copy paths:
  - icon path extraction depended on a regex capture (`$1`) that could be stale/empty,
  - theme values containing whitespace around `-` were not normalized before copy decisions.
- This allowed invalid source/destination path fragments to propagate into package/install copy operations.

### Fixes Applied
- Hardened `wps/wpsbuild.pl`:
  - added `trim_value()` and normalized incoming theme values,
  - guarded backdrop/font/icon copy helpers against empty/`-` values,
  - replaced fragile icon-path capture usage with explicit `icons/<file>` extraction and validation,
  - ensured copy helpers no-op safely on invalid placeholders.

### Validation Results (Verified)
- Simulator wrapper (`./build-sim.ps1`):
  - reaches `Creating rockbox-info.txt`,
  - reaches `Installing your build in your 'simdisk' dir`,
  - no recurrence of `cp ... /rockpod/-` or `fonts/-` errors.
- Hardware wrapper (`./build-hw.ps1 -Target ipod6g`):
  - reaches `Creating rockbox-info.txt`,
  - reaches `ZIP rockbox.zip`,
  - no recurrence of packaging `-` path errors.
- Simulator runtime smoke launch:
  - `build-sim/rockboxui.exe` starts successfully (process creation verified).

### Current State
- Wave 0 build reliability blocker has moved from **active packaging failure** to **green native sim/hw packaging path** for validated targets.
- `ipodvideo` full hardware pass remains pending (not yet run in this stabilization slice).

### Files Touched
- `wps/wpsbuild.pl`
- `WORK_LOG.md`

## 2026-04-06 (Wave 2 full-product interface coverage pass)

### Scope
- Expanded Wave 2 from core shell/list rules to full-product interface coverage.
- This pass remains design-system/UX-spec only (no performance and no Cover Flow architecture work).

### New coverage areas locked
- Boot/startup design-system rules (including intended Apple-like reboot presentation).
- Loading/wait-state system across core splash and PictureFlow loading states.
- Full-surface contracts for:
  - home/root,
  - music lists,
  - album list,
  - album page,
  - now playing / WPS,
  - mini-player,
  - Cover Flow UI,
  - Cover Flow tracklist,
  - settings,
  - filesystem lists,
  - fallback/empty states.
- Complete end-to-end user-flow continuity lock.

### Boot image review (new asset)
- Reviewed `Sourced Icons/iPhone-Rebooting.png`.
- Verified header dimensions from file bytes: `640x1136`.
- Locked adaptation direction for iPod startup:
  - black `320x240` canvas,
  - centered Apple mark,
  - no extra startup text in flagship visual mode.

### Selector/scrollbar normalization update
- Added explicit iOS-like guidance into design lock:
  - default active row selection is solid neutral gray (non-gradient),
  - thin right-edge gray scrollbar baseline,
  - pink accent (`#FF2D55`) reserved for music-emphasis states, not default list selection.

### DESIGN_SYSTEM.md expansion result
- Added sections `34` through `52` for full-product Wave 2 coverage and completion gate.
- Added implementation-facing references for startup/loading/PictureFlow surfaces.

### Files touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build / runtime status
- Not run in this pass (documentation and planning lock only).

### Next task queue
1. Snap remaining ambiguous numeric anchors (header/chevron/scrollbar/battery/mini-player/loading).
2. Begin Wave 3 scaffolding with the expanded full-surface lock.
3. Validate each surface step with simulator screenshots once build path is green.

## 2026-04-06 (Wave 2 -> Wave 3 readiness planning pass)

### Scope
- Implementation-readiness audit only (no broad Wave 3 UI implementation yet).
- Goal: verify whether Wave 2 is specific/stable enough to execute Wave 3 without drift.

### Readiness Findings
- Fully locked and safe now:
  - source hierarchy + donor priority,
  - list geometry tokens (`52/56/48`),
  - inset/divider tokens (`16/76/50`, `0.5px`),
  - flat selector direction and Apple2026 accent token (`#FF2D55`),
  - scrollbar-on/right/subtle direction,
  - glyph-primary icon strategy and icon-state color logic,
  - Interpod base DNA + Interpod2/amusicpodds donor boundaries.
- Still ambiguous (must lock before full shell completion):
  - numeric header alignment anchors,
  - chevron exact placement,
  - final scrollbar width/inset token,
  - battery icon scale and battery-text relation,
  - mini-player exact geometry,
  - loading-state viewport geometry,
  - per-surface final font-size snaps where ranges still exist.

### Wave 3 Implementation Surface Map (locked)
- Theme registration/config source of truth:
  - `wps/WPSLIST`
- New Apple2026 shell files (to be added via WPSLIST flow):
  - `.cfg`, `.sbs`, `.wps`
- Runtime deployment/validation path:
  - `build-sim/simdisk/.rockbox/themes/`
  - `build-sim/simdisk/.rockbox/wps/`
  - `build-sim/simdisk/.rockbox/icons/`
  - `build-sim/simdisk/.rockbox/fonts/`
- Donor reference baselines (read-only extraction sources):
  - `Imported Reference Themes/Interpod/...`
  - `Imported Reference Themes/Interpod 2/...`
  - `Imported Reference Themes/amusicpodds/...`

### Ordered Wave 3 Execution Plan
1. theme scaffolding + token wiring
2. header system
3. list geometry/insets
4. chevron/divider/icon integration
5. selector + scrollbar behavior
6. mini-player continuity
7. WPS alignment
8. loading/empty-state shell
9. whole-shell consistency audit

### Validation Plan
- Rebuild simulator after each major implementation step.
- Capture screenshot sets per step (header, list, icons/chevrons/dividers,
  selector+scrollbar, mini-player, WPS, loading state).
- Compare every set against:
  1) Figma nodes `1:4008`, `1:475`, `1:5998`
  2) Apple references in `MASTER.md`
  3) Interpod baseline
- Mark each step pass/fail with explicit mismatch notes.

### Drift Prevention Lock
- No visual implementation without a corresponding `DESIGN_SYSTEM.md` rule.
- Missing numeric rule = block implementation and lock doc first.
- `WORK_LOG.md` must record screenshots, pass/fail, unresolved deltas, next action.

### Go / No-Go
- **GO** for Wave 3 foundation scaffolding and already-locked token implementation.
- **NO-GO** for full Wave 3 completion until all ambiguous numeric locks are added.

### Files Touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

## 2026-04-06 (Wave 2 symbol-source determination)

### Scope
- Finalized explicit decision for symbol source strategy:
  - SF-family glyph symbols vs imported icon libraries.

### Determination
- Primary winner: **SF-family glyph symbols** for main UI iconography.
- Imported libraries remain secondary/fallback for tiny status/system edge cases and compatibility.

### Reasoning (implementation-facing)
- Glyph symbols align best with typography lock, preserving Apple-like optical consistency.
- Glyph route scales and recolors more coherently across state sets.
- Imported packs still valuable for validated tiny runtime status assets.

### Source Priority Lock
1. SF glyph symbols (primary)
2. `sf-symbols-master` candidate bitmaps (secondary masters)
3. Interpod/Interpod2 imported icon packs (status/system fallback)
4. amusicpodds icons (targeted-only donor)

### Files Touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

## 2026-04-06 (amusicpodds comparative deep pass)

### Scope
- Deep comparative audit: `Interpod` vs `Interpod 2` vs `amusicpodds`
- Required focus categories completed:
  1) list geometry
  2) album list behavior
  3) mini-player/lower shell continuity
  4) top navigation/header behavior
  5) icon style/symbol choice
  6) separator treatment
  7) active/inactive color logic
  8) album page composition
  9) scrollbar/selector behavior
  10) overall Apple Music shell alignment

### Inputs Re-verified
- Re-read Figma source nodes:
  - `1:4008` (library shell)
  - `1:475` (album list)
  - `1:5998` (album page)
- Re-read theme files:
  - `Interpod.cfg`, `Interpod.sbs`, `Interpod.wps`
  - `Interpod 2.cfg`, `Interpod 2.sbs`, `Interpod 2.wps`
  - `amusicpodds.cfg`, `amusicpodds.sbs`, `amusicpodds.wps`

### Key Determinations (explicit)
- `Interpod` remains strongest overall base (light shell + full-package cohesion).
- `Interpod 2` is strongest polish/state donor (flat selector behavior, cleaner state handling).
- `amusicpodds` is strongest targeted donor (typography/icon direction) but not base-shell donor.

### Where amusicpodds is closer to Apple Music
- SF Compact-driven typography voice and compact readability experiments.
- Music-first icon semantics and some playback info spacing choices.
- Album-art prominence in WPS composition.

### Where amusicpodds is not closer
- Dark global shell inversion conflicts with Apple2026 light-shell/Figma baseline.
- `scrollbar: off` conflicts with subtle-scrollbar requirement.
- `statusbar: off` baseline conflicts with current Apple2026 shell strategy.
- Accent token drift (`f24e36`) conflicts with Apple2026 accent (`#FF2D55`).

### Spec Updates Applied
- Added explicit three-way comparison section to `DESIGN_SYSTEM.md`:
  - Interpod 1 vs Interpod 2 vs amusicpodds
  - strengths by category
  - inherit/reject map per category
  - final inheritance package and explicit rejects
- Added implementation gate section with per-surface acceptance criteria:
  - library root gate
  - album list gate
  - album page gate
  - lower-shell/mini-player gate
  - donor-compliance gate
- Added token-level donor mapping section:
  - core shell token defaults
  - selection/scroll token defaults
  - typography/icon/lower-shell donor mapping
  - hard rejection token list

### Inheritance Lock (for implementation waves)
- Keep pure Interpod DNA:
  - light-shell base
  - shell coherence
  - lower-shell continuity model
- Pull from Interpod 2:
  - flat selector model
  - state/polish refinements
- Pull from amusicpodds (limited):
  - SF Compact legibility studies
  - select symbol candidates from `amusicpod.bmp`
  - compact playback spacing cues
- Reject from amusicpodds for flagship default:
  - dark-shell base
  - scrollbar-off
  - statusbar-off
  - legacy accent red token

### Build / Runtime Status
- Not run in this pass (documentation/specification-only).

### Next Task Queue
1. Translate inheritance lock into concrete Apple2026 token/state implementation plan (without crossing into Cover Flow/perf branches).
2. Define per-surface acceptance criteria for library, album list, and album page based on this comparison lock.
3. Continue simulator-validated UI implementation once build path is green.

## 2026-04-07

### Scope
- Wave 1 / Wave 2 design documentation pass only
- No implementation edits outside documentation
- No Cover Flow/performance/caching work

### Verified Inputs Re-reviewed
- `MASTER.md` hierarchy and branch boundaries
- User Figma target nodes: `1:4008`, `1:475`, `1:5998` (previously extracted geometry/state data reused)
- Apple references (HIG + support pages) including: Typography, Icons, SF Symbols, Color, Layout, Toolbars, Tab bars, Scroll views, Disclosure controls, Lists and tables, Search fields, Sidebars
- Imported theme set: `Interpod`, `Interpod 2`, `amusicpodds`, `adwaitapod`, `adwaitapod_simplified`, `iClassic_v1.1`, `iPodOS`, `iPone`, `iRB_Modern`

### Documentation Changes
- Rewrote `DESIGN_SYSTEM.md` into a strict, implementation-facing design lock document.
- Added explicit numeric geometry rules for:
  - library rows (`52px`),
  - album rows (`56px`),
  - track rows (`48px`),
  - key insets (`16px`, `76px`, `50px`),
  - hero art (`270px`),
  - separator thickness (`0.5px`).
- Added formal sections for:
  - source hierarchy,
  - Apple principles,
  - Interpod preservation,
  - imported-theme extraction,
  - typography/color/icons/selector/scrollbar/divider/chevron rules,
  - mini-player/tab-bar composition,
  - loading/empty states,
  - anti-patterns,
  - future-agent checklist and validation protocol.

### Verified vs Assumed
- Verified:
  - core geometry tokens and composition rhythm from prior direct Figma extraction,
  - imported theme config/layout direction from prior file audit,
  - Apple guideline principles from fetched pages listed above.
- Assumed:
  - none newly introduced in this pass; this was a synthesis/lock pass using verified prior evidence.

### Files Touched
- `DESIGN_SYSTEM.md`
- `WORK_LOG.md`

### Build / Simulator Status
- Not run in this pass (documentation-only session).

### Next Task Queue
1. Use this expanded `DESIGN_SYSTEM.md` as the single UI lock for subsequent shell implementation waves.
2. During implementation, validate each major surface against Figma/Apple references and log deltas.
3. Keep branch boundaries strict: defer Cover Flow optimization/performance architecture.

## 2026-04-06

### Scope
- Wave 0 and Wave 1 only
- Environment/build validation first
- Simulator validation second
- Design/source audit baseline third
- No Cover Flow optimization work

### Confirmed Decisions
- `MASTER.md` remains the primary source of truth
- `Apple2026` is the flagship classification
- Original `Interpod` remains the strongest base theme
- Figma nodes `1:4008`, `1:475`, and `1:5998` are the strongest visual authority
- Local asset roots are:
  - `Apple Fonts/`
  - `Sourced Icons/`
  - `Imported Reference Themes/`
  - `Sample Music Folder with Library Structure/`

### Build Workflow Progress
- Added Windows wrappers:
  - `build-sim.ps1`
  - `build-hw.ps1`
- Standardized wrappers on MSYS2 `UCRT64` launch instead of plain `MSYS_NT`
- Forced a combined PATH so the hosted SDL tools and `arm-none-eabi-*` toolchain can both be found
- Updated shell scripts to use portable job detection with `JOBS` override
- Patched `tools/configure` so ARM selection accepts `arm-none-eabi-*`
- Patched `build-hw.sh` to default `CROSS_COMPILE=arm-none-eabi-`

### Current Build State
- Hardware toolchain detection is working:
  - `ipod6g` configure now resolves `arm-none-eabi-gcc 13.3.0`
- Environment-level blocker is resolved:
  - previous `arm-elf-eabi-*` failure is no longer the primary issue
- Dependency generation/build order has been hardened:
  - `build-sim.sh` and `build-hw.sh` now run `make dep` explicitly
  - both scripts pre-generate key headers before parallel compile (`lang_enum.h`, `lang/lang.h`, `sysfont.h`, `bitmaps/rockboxlogo.h`, `bitmaps/default_icons.h`, etc.)
- `make.dep` parser instability (`multiple target patterns`) is no longer the active blocker in current runs
- Generated-header races (`bitmaps/rockboxlogo.h`, `lang_enum.h`) are resolved for main compile flow
- Simulator compile now reaches completion of core and plugin compilation and produces `build-sim/rockboxui.exe`
- Simulator executable launch smoke test succeeded on Windows (`Start-Process` returned a valid PID)
- Hardware `ipod6g` build now reaches zip phase (`ZIP rockbox.zip`)
- `ipodvideo` smoke validation was not run because `ipod6g` is not yet green

### Active Packaging / Install Blocker
- Both simulator `make install` and hardware packaging currently report:
  - `cp: cannot stat '/c/.../rockpod/-': No such file or directory`
  - `ERROR: Error opening file: /c/.../rockpod/fonts/-`
- This appears in buildzip/install packaging stage, after successful compilation.
- Build artifact status is therefore:
  - compile artifacts: **verified present** (`rockboxui.exe`, hardware objects/zip stage)
  - packaging/install stage: **still unstable**

### WSL Fallback Status (Verified)
- WSL fallback is now accepted as secondary canonical build path if native Windows remains unstable.
- Current machine status check:
  - `wsl --status` returns `WSL_E_WSL_OPTIONAL_COMPONENT_REQUIRED`
  - message indicates WSL optional component install is pending and system reboot may be required
- Current blocker to WSL continuation:
  - reboot required before Ubuntu provisioning and WSL-based toolchain steps can proceed

### Build-System Files ToucheOpenRouterd
- `tools/configure`
- `tools/root.make`
- `firmware/firmware.make`
- `apps/apps.make`
- `apps/lang/lang.make`
- `apps/bitmaps/bitmaps.make`
- `apps/plugins/bitmaps/pluginbitmaps.make`
- `build-sim.sh`
- `build-hw.sh`
- `build-sim.ps1`
- `build-hw.ps1`

### Design Audit Baseline
- Figma findings locked into `DESIGN_SYSTEM.md`
- Imported theme set inventoried:
  - `Interpod`
  - `Interpod 2`
  - `amusicpodds`
  - `adwaitapod`
  - `adwaitapod_simplified`
  - `iClassic_v1.1`
  - `iPodOS`
  - `iPone`
  - `iRB_Modern`
- `Apple Fonts/SF Pro` confirmed with full Display/Text/Rounded families
- `Sourced Icons/Figma File as SVG.svg` confirmed
- `Sourced Icons/sf-symbols-master` confirmed as the main symbol inventory
- Sample music library confirmed with album-artist -> album -> track structure and album covers

### Simulator / Screenshot Status
- Simulator launch not completed
- No screenshots captured in this pass
- Screenshot comparison remains pending until generated-asset builds are stable

### Next Task Queue
1. Reboot Windows so WSL optional component can finish activation
2. Re-check `wsl --status` and `wsl -l -v`; if healthy, provision Ubuntu path
3. If native Windows is still unstable, switch canonical build flow to WSL Ubuntu
4. Isolate and fix the buildzip/install `fonts/-` and `rockpod/-` packaging-source issue
5. Re-run simulator validation and capture baseline screenshots once packaging/install is stable

---

### Wave 3 — ecosystem adaptation note + progress-rail parity (2026-04-07)
- **Design intent:** Documented in `DESIGN_SYSTEM.md` §33.7.1 — Figma/iOS references define *language*
  (hierarchy, separators, polish); **§54** iPod tokens are authoritative for shipped geometry; goal is
  same ecosystem, not literal phone clone.
- **Code:** `A26_PROGRESS_TRACK` (`E5E5EA`) added in `apps/apple2026_shell.h`; splash loading bar rail
  uses it (`apps/gui/splash.c`). Picture Flow indexing/loading rails aligned to same token
  (`apps/plugins/pictureflow/pictureflow.c` — `draw_progressbar`, `show_track_list_loading`).
- **Verified:** Logic-only / token alignment; simulator screenshot pass still pending packaging stability.
- **Next:** Continue Wave 3 per §33.5 (remaining shell surfaces); build when `make install`/zip path is green.

### Wave 3 — shell stack documentation + mini-player token alignment (2026-04-07)
- **`DESIGN_SYSTEM.md` §54.1 / new §54.1.1:** Replaced ambiguous `content_h_without_footer` with SBS-accurate
  viewport heights; documented `%?mp` / `%?Lo` routing and vertical stack for `wps/Apple2026.sbs`.
- **`wps/Apple2026.sbs`:** Mini-player title/meta text lanes `x` **46→48**, width **228→226** to match §54.5
  `mini_player_title_x = 48` (right edge unchanged at 274).
- **Verified:** Arithmetic `27+177=204`, `51+153=204`, `51+189=240` (stopped + large title) consistent with routing.
- **Next:** Screenshot pass for list+mini and root large-title when build is green; optional viewers icon strip parity.

### Wave 3 — cross-reference verification pass vs reference themes + geometry fixes (2026-04-07)

**Audit scope:** Compared Apple2026 `.sbs`/`.wps` coordinates against both DESIGN_SYSTEM.md
(§54/§27.1) and the available reference theme files (`adwaitapod_dark_simplified`,
`Themify_2`). No Interpod/Interpod2 theme files exist in-tree; these two themes served
as the only structural reference for viewport patterns.

**Reference theme observations (structural, not stylistic):**
- Both refs use 24-25px headers; Apple2026's 26px is intentional and documented.
- Neither ref uses a bottom mini-player bar — both use sidebar widgets. Apple2026's
  bottom mini-player is an Apple Music-inspired design choice, documented in §54.5.
- Both refs place metadata BESIDE left-aligned art (horizontal layout). Apple2026 places
  metadata BELOW centered art (Apple Music vertical stack). Intentional per §70.5.2.
- Both refs place playlist counter on the RIGHT side, not overlapping art.
- Themify_2 uses 30px row height; adwaitapod uses 20-22px rows.

**Issues found and corrected:**

1. **Large title band: 24px→32px** — §27.1 explicitly states the Wave 3 rebuild uses a
   32px-tall large-title band (y=27..58) with list content starting at y=59. The previous
   SBS used the old §54.1 values (24px band, content at y=51). Fixed:
   - `%Vl(large_title,16,27,-16,24,2)` → `%Vl(large_title,16,27,-16,32,2)`
   - `%Vi(content_with_mini_lt,0,51,-,153,-)` → `%Vi(content_with_mini_lt,0,59,-,145,-)`
   - `%Vi(content_full_lt,0,51,-,189,-)` → `%Vi(content_full_lt,0,59,-,181,-)`
   - 22px font in 32px band gives 5px padding per side — much more Apple-like.
   - Updated §54.1 in DESIGN_SYSTEM.md: `content_y_large_title = 59`,
     `list_viewport_h_with_mini_lt = 145`, `list_viewport_h_no_mini_lt = 181`.

2. **Playlist counter: moved from art zone to artist line** — user confirmed the old
   viewport at `%V(16,-100,64,14,9)` (absolute y=140) was creating a visible white
   rectangle in the art zone. Relocated to right-aligned on the artist line:
   - Removed old `%V(16,-100,64,14,9)` playlist counter.
   - Narrowed artist viewport from `-16` (288px) to `224px` to make room.
   - Added `%V(248,192,56,18,9)` — right-aligned "%pp/%pe" in tertiary gray (6E6E73),
     same y-line as artist text. Clean Apple-like format.

**Coordinates confirmed correct (all other areas):**
- Header: (0,0,-,26) with hairline, title at (74,5), battery at (288,4) — all match §54.2.
- Content viewports (non-LT): y=27, h=177/213 — match §54.1.
- Mini-player: y=204, h=36, art (8,207), title (48,212), meta (48,226), playpause (284,214) — all match §54.5.
- WPS: art 156x156 at (82,9), title y=172, progress y=210, time y=224 — all match §54.6.
- Volume overlay: both SBS and WPS speaker icons at correct viewport-relative positions.

**Verified:** Full line-by-line SBS and WPS coordinate audit against §54, §27.1, and reference theme structural patterns.

### Wave 3 — Apple Music visual-parity pass (2026-04-07)

**Goal:** Direct image comparison between Apple Music Library reference screenshot and
current iPod build photo. Correct the visual deltas to bring the main menu much closer
to the Apple Music design ecosystem.

**Key findings from direct image comparison:**
- Apple Music shows 5-6 tall rows; iPod showed 9 cramped rows.
- Apple Music has 16px left inset; iPod icons were flush to screen edge.
- Apple Music has generous icon-to-text gap; iPod had 2px gap.
- Apple Music large title dominates; iPod had competing header + large title.
- Apple Music icons are all red/pink; iPod mixed red/black/gray.

**Changes made:**

1. **Row height 20→32px** — changed icon tile height in `tools/apple2026_symbol_assets.py`
   from `ICON_TILE_H=20` to `ICON_TILE_H=32`. Since Rockbox row height =
   `max(font_height, icon_height)`, 32px tiles drive 32px rows. Visible items drop from
   9 to 5-6, matching Apple Music density.

2. **Icon padding 1→6px** — changed `ICON_PADDING` in `apps/gui/bitmap/list.c` from 1 to 6.
   Adds 6px before and 6px after each icon in the list, creating icon column width of
   22+12=34px. Proper icon-to-text gap now exists.

3. **List viewport inset** — all four `%Vi` content viewports in `Apple2026.sbs` changed
   from `x=0, w=full` to `x=16, w=-16` (i.e., `%Vi(...,16,...,-16,...)`). This creates
   16px left and right margins, matching Apple Music's inset. Dividers naturally span only
   288px (viewport width), creating inset dividers without code changes.

4. **Large title font 22→25px Bold** — SBS slot 2 changed from `22-SFCompactDisplay-Semibold`
   to `25-SFProDisplay-Bold`. The large title "Rockbox" now dominates the screen like Apple
   Music's "Library" dominates its view.

5. **Header title hidden on root** — when `%Lo` (root screen), the SBS header title text
   viewport is not drawn. This removes the redundant double "Rockbox" effect (header +
   large title both showing "Rockbox"). The header still shows battery, play status, and
   busy indicators. On non-root screens, the header title resumes as before.

6. **Icon tint: black→gray** — removed the `CLR_ON_LIGHT` (black) icon category entirely.
   Icons that were black (Folder, Cursor, Submenu) are now tertiary gray (`3C3C43`).
   Expanded accent set to include Folder (1), Plugin (9), System menu (24), Playback
   menu (25) for stronger red/pink emphasis at top-level.

7. **Icon regeneration** — ran `apple2026_symbol_assets.py` with updated tile dimensions
   and tinting. All 32 icon tiles regenerated at 22x32 with adjusted padding.

**Documentation updated:**
- `DESIGN_SYSTEM.md` §54.1: added `list_viewport_x=16`, `list_viewport_w=288`, `right_inset=16`.
- `DESIGN_SYSTEM.md` §54.1.1: documented header-title-hidden-on-root behavior.
- `DESIGN_SYSTEM.md` §54.3: replaced abstract Figma-derived geometry with actual Rockbox
  implementation values: `icon_tile_h=32`, `icon_padding=6`, `icon_column_w=34`,
  `text_start_x=50`, visible row counts per viewport mode.
- `DESIGN_SYSTEM.md` §27.1: updated large title from 22px Compact to 25px Pro Display Bold.
- `wps/WPSLIST`: updated comment to reflect 22x32 tile dimensions.

**Files changed:**
- `apps/gui/bitmap/list.c` — ICON_PADDING 1→6
- `tools/apple2026_symbol_assets.py` — tile height, tinting, padding
- `wps/Apple2026.sbs` — font slot 2, viewport insets, header routing
- `wps/WPSLIST` — comment update
- `icons/Apple2026Icons.bmp` — regenerated
- `wps/Apple2026/*` — all WPS assets regenerated
- `DESIGN_SYSTEM.md` — geometry tokens updated
- `WORK_LOG.md` — this entry

**Expected visual result:**
- Root menu shows bold "Rockbox" title dominating top, no competing header text.
- Menu rows are 32px tall with ~5-6 visible items (was 9).
- Icons are inset 22px from screen edge (16px viewport + 6px padding).
- 12px gap between icon right edge and text start.
- Dividers span viewport width (288px), naturally inset 16px from each screen edge.
- Red/pink accent on music-first icons; gray on secondary/settings icons.
- No black icons anywhere.

**Next:** Hardware build and on-device verification.

### Wave 4 — Major correction pass (menu + WPS + Cover Flow) (2026-04-07)

**Goal:** Comprehensive correction pass addressing user's 14-point list of outstanding
issues across main menu, WPS, and Cover Flow. Direct visual comparison against Apple
Music reference screenshot and iPod screen photo.

**Changes made:**

1. **DESIGN_SYSTEM.md icon_tile_w 22→28** — previous pass changed the icon tile width
   in the Python script but failed to update the design token table. §54.3 now correctly
   documents `icon_tile_w=28`, `icon_column_w=40`, `text_start_x=56`.

2. **WPSLIST comment 22x32→28x32** — updated icon strip dimensions comment to match
   actual 28x32 tile size from the generation script.

3. **WPS first-play rendering bug FIXED** — root cause identified: the album art viewport
   in `Apple2026.wps` had `%VB` (output-to-backdrop-buffer) flag, causing art to render to
   the backdrop framebuffer instead of the main display. On first WPS entry, the backdrop
   buffer was empty, so the art area showed blank/black. On re-entry, the cached backdrop
   had art from the previous render. Fix: removed `%VB` from the art viewport. Art now
   renders directly to the main display, visible on first frame. Also added explicit
   `%Vf(000000)%Vb(F8F8F8)` to the backdrop viewport as defensive fallback.

4. **Right-side chevrons IMPLEMENTED** — added C-level chevron drawing to
   `apps/gui/bitmap/list.c` in `_default_listdraw_fn()`. After each `put_line()` call,
   a 4x9 pixel ">" chevron is drawn at the right edge of the list text viewport using
   `display->drawline()`. Color: `C7C7CC` (light gray, Apple-subtle). Positioned at
   `vp_width - 10` horizontally, vertically centered in the row. Only drawn on color
   displays (`HAVE_LCD_COLOR && depth >= 16`) and non-title items. This was previously
   identified as impossible with Rockbox's bar selector style — solved by drawing the
   chevron after the standard line rendering.

5. **Cover Flow: fullscreen by default** — changed `show_statusbar` default from `true`
   to `false` in `apps/plugins/pictureflow/pictureflow.c`. Cover Flow now launches
   without the SBS header, using the full 320x240 screen. Eliminates the white bar
   under the header that covered album cover tops.

6. **Cover Flow: "Cover Flow" title removed** — all 4 `sb_set_persistent_title("Cover Flow",...)`
   calls changed to empty string. With statusbar disabled, this is redundant, but also
   ensures no title shows if user manually re-enables statusbar.

7. **Icon regeneration** — ran `apple2026_symbol_assets.py` with the corrected
   `ICON_TILE_W=28, ICON_TILE_H=32, padding=2, scale=0.92`. All icons regenerated
   at 28x32 with ~23px artwork centered in each tile.

**Documentation updated:**
- `DESIGN_SYSTEM.md` §54.3: `icon_tile_w` 22→28, derived values updated.
- `DESIGN_SYSTEM.md` §54.4: chevron tokens updated to match C implementation
  (4x9px drawline, `C7C7CC`, inset 6).
- `wps/WPSLIST`: comment updated to 28x32.
- `WORK_LOG.md`: this entry.

**Files changed:**
- `apps/gui/bitmap/list.c` — right-side chevron drawing code
- `apps/plugins/pictureflow/pictureflow.c` — show_statusbar=false, title text removed
- `wps/Apple2026.wps` — removed `%VB` from art viewport, added explicit colors to backdrop
- `wps/WPSLIST` — comment update
- `icons/Apple2026Icons.bmp` — regenerated
- `wps/Apple2026/*` — all WPS assets regenerated
- `DESIGN_SYSTEM.md` — §54.3 icon_tile_w, §54.4 chevron tokens
- `WORK_LOG.md` — this entry

**Known limitations documented (not fixable in theme layer):**
- **Font kerning/tracking:** Rockbox `.fnt` format (BDF-derived) does not support
  kerning pairs. The `convttf` tool that converts TTF→BDF→FNT drops kerning tables.
  Letter pairs like ab, li, lu, am, ar will appear too tight. Fixing this requires
  modifying the font renderer or `convttf` tool, which is beyond UI/UX scope.
- **22px body font:** No 22px SF Pro Text TTF source was found in `Apple Fonts/`
  for generation. Current body font remains 19px. If a TTF source becomes available,
  `convttf` can generate the 22px variant.

**Expected visual results:**
- Right-side light gray chevrons on all list items.
- Cover Flow launches fullscreen without header bar or white gap.
- WPS renders correctly on first song play (no black bars / missing art).
- Menu icons at 28x32 tiles with ~23px artwork.

**Remaining items for next pass:**
- On-device visual verification of all changes.
- Potential 22px body font if TTF source is located.
- WPS layout refinement (Interpod-grounded improvements).

### WPS Master Rebuild — Apple2026 WPS stop-regression pass (2026-04-08)

**Goal:** Full stop-regression rebuild of `wps/Apple2026.wps` from Interpod's proven
structural baseline. Prior WPS kept regressing (blank art on first open, cropped times,
font-missing fallback, duplicate image IDs, non-ASCII encoding issues, misaligned metadata).

**Root causes identified (full history audit):**
1. `%VB` removed from art viewport in a prior pass ("fix" for first-open blank art) — this was
   the WRONG fix. Removing `%VB` from the art viewport breaks Rockbox's album art compositor.
   The correct fix is to keep `%VB` on the art viewport (Interpod's proven pattern) and ensure
   `%Cl`/`%Cd` live inside that viewport.
2. `16-SFProText-Regular.fnt` referenced — this file does not exist in `fonts/`. Font-missing
   causes the skin parser to fail and fall back to the default WPS.
3. `albumShadow.bmp` preloaded with `%xl(AlbumShadow,...)` AND drawn with `%x(AlbumShadow,...)`
   — duplicate image ID causes parse failure and WPS fallback.
4. Non-ASCII characters (em-dashes U+2014) in comments — some Rockbox parser versions
   fail on non-ASCII bytes in WPS files even in comments.
5. Over-engineering: extra viewport layers, experimental shadow assets, 5 font slots — each
   additional complexity added another failure surface.

**Rebuild approach:**
- `wps/Apple2026.wps` completely rewritten from Interpod's source as strict structural baseline
- Applied only the specific requested changes on top of Interpod's proven structure
- Verified every change against audit script before accepting

**Changes from Interpod baseline:**
- Art: 130x130 -> 150x150 (+15%), centered at x=85 (was x=95 for 130px art)
- Font slot 3: `14-SFProText-Regular.fnt` (was `15-Inter-Bold.fnt`)
- Font slot 8: `20-SFProText-Regular.fnt` (was `20-Inter-SemiBold.fnt`)
- Font slot 9: `16-SFProText-Medium.fnt` (was `20-Inter-V.fnt`)
- Artist/album split into two separate viewports (Interpod combined them in one)
- Artist color: `FF2D55` Apple red (Interpod: `F24E61`)
- Album: separate gray line below artist
- Playlist counter: right-aligned on title line (Interpod: separate top row `%V(18,-100,...)`)
- `albumFramed.bmp` NOT used (it is 130x130 and would misrender at 150x150)

**Anti-regression audit results (python tools/audit_apple2026_wps.py):**
- A. Non-ASCII: 0 chars. PASS
- B. Image IDs: no duplicates; all %xd refs resolve. PASS
- C. Fonts: all 3 referenced .fnt files exist on disk. PASS
- D. BMP assets: all 19 referenced .bmp files exist on disk. PASS
- E. %VB art viewport: %Cl confirmed inside %VB viewport. PASS

**Files changed:**
- `wps/Apple2026.wps` — complete rebuild from Interpod baseline
- `tools/audit_apple2026_wps.py` — improved audit script (multi-frame %xl aware)
- `DESIGN_SYSTEM.md` §54.6 — updated with locked geometry, font slots, and 10 anti-regression rules

**WPS anti-regression contract (do not violate):**
1. %VB must stay on the art viewport
2. %Cl and %Cd must be inside the %VB viewport
3. Font slots 3/8/9 only; no reference to non-existent .fnt files
4. No duplicate image IDs
5. No non-ASCII characters (including comments)
6. albumShadow.bmp: if preloaded, must be drawn via %xd only (not %x)
7. albumFramed.bmp: only valid for 130x130 art
8. Run audit script before committing any future WPS change

**Next:** On-device or simulator verification of WPS layout.

### Wave 4 — Bounded-stack navigation (loop prevention) (2026-04-09)

**Goal:** Prevent the WPS back-path from re-entering a stale content chain when
the user accesses "Now Playing" from Main Menu after already backing out of that
chain. Eliminates the Main Menu -> WPS -> Cover Flow -> Main Menu infinite loop.

**Root cause identified:**
`playback_source` is a session-global tag stamped when playback starts. It
persists until new playback starts from a different surface. When a user backed
all the way out of a content chain (e.g. Cover Flow -> tracklist -> WPS -> Cover
Flow -> Main Menu) and then re-entered WPS from "Now Playing" on Main Menu, the
stale `playback_source` (still `PICTUREFLOW`) would route them back into Cover
Flow -- a chain they had already closed. This created a circular navigation loop.

**Design principle:** WPS back-path must depend on **how** WPS was entered this
time, not just on where playback was started. Main Menu is a stable ceiling.
Re-entering WPS from Main Menu is a "flat entry" -- back returns to Main Menu.

**Implementation:**
1. Added `bool wps_entered_from_root` global in `apps/root_menu.c`, declared
   `extern` in `apps/root_menu.h`.
2. Set `wps_entered_from_root = (last_screen == GO_TO_ROOT)` in the root menu
   dispatch loop before `GO_TO_WPS` is loaded. Covers both `CONFIG_TUNER` and
   non-tuner paths.
3. Added gatekeeper at top of `wps_handle_browse_parent()` in `apps/gui/wps.c`:
   if flag is `true`, returns `GO_TO_ROOT` without consulting `playback_source`.
   All existing source-aware routing preserved for content-chain entries.

**Entry-path verification (all correct):**
- Main Menu -> "Now Playing" -> WPS: `last_screen == GO_TO_ROOT` -> flag=true -> back=Main Menu.
- Cover Flow -> play -> WPS: `last_screen == GO_TO_PICTUREFLOW` -> flag=false -> back=Cover Flow.
- Music -> play -> WPS: `last_screen == GO_TO_MUSICLIB` -> flag=false -> back=Music.
- Database -> play -> WPS: `last_screen == GO_TO_DBBROWSER` -> flag=false -> back=Database.
- Playlist catalog -> play -> WPS: `last_screen == GO_TO_PLAYLISTS_SCREEN` -> flag=false -> back=Playlists.
- Playlist viewer -> play -> WPS: `last_screen == GO_TO_PLAYLIST_VIEWER` -> flag=false -> back=Playlist viewer.
- Boot auto-resume -> WPS: `last_screen == GO_TO_ROOT` -> flag=true -> back=Main Menu.
- MENU long (Home) from WPS, then "Now Playing": flag=true -> back=Main Menu.

**Files changed:**
- `apps/root_menu.h` -- added `extern bool wps_entered_from_root` declaration
- `apps/root_menu.c` -- added flag definition + setter in `GO_TO_WPS` dispatch
- `apps/gui/wps.c` -- added gatekeeper in `wps_handle_browse_parent()`
- `DESIGN_SYSTEM.md` -- added bounded-stack navigation subsection to section 21A
- `WORK_LOG.md` -- this entry

**Next:** Build verification (simulator and/or hardware).

---

### WPS Correction Pass (fresh-eyes photo review) (2026-04-09)

**Trigger:** User photo of on-device WPS revealed multiple visual bugs.

**Bugs identified from photo:**

1. **Thick black border around art** - caused by `wpsArtCorners.bmp` corner mask
   viewport (`%V(82,5,156,156,-)`) with white background (`%Vb(FFFFFF)`) that was 3px
   larger on each side than the 150x150 art viewport. The magenta-keyed corner mask
   was not rendering correctly, leaving a visible white/dark frame artifact.
2. **Song title way too big** - font slot 8 (20px Medium) dominated the whole bottom
   zone and made the composition feel unbalanced.
3. **Album title cropping into progress bar** - three separate text lines
   (title 20px + artist 16px + album 14px = 50px) plus progress bar and time row
   all competed in 82px below the art. Album line was running into the progress bar.
4. **Bottom zone too cramped** - no breathing room between any elements.
5. **Album art missing (placeholder)** - expected on real hardware when tracks have
   no embedded art; placeholder was visible but functional.

**Fixes applied:**

- `wpsArtCorners.bmp` corner overlay viewport **removed entirely** - clean square art,
  no border artifact. Simplest reliable fix; rounded corners can be revisited later.
- Art stays **150x150 at x=85, y=8** (perfectly centered on 320px screen) - user
  confirmed they like this size.
- Title font: **slot 8 (20px) -> slot 9 (16px Medium)** - reduced to stop dominating.
- Artist + Album **collapsed to one alternating line** (Interpod pattern): two named
  viewports (`artist_line` red / `album_line` gray) at same y position with inverse
  `%t(5)` subline timing. Saves 14-16px vertical height.
- Bottom zone **rebalanced** with gaps between every element:
  - Title y=162 (gap 4px from art bottom)
  - Artist/Album y=183 (gap 4px from title)
  - Progress bar y=203 (gap 4px from artist/album)
  - Time row y=211 (gap 4px from progress bar)
  - 15px bottom padding remaining
- Volume bar coordinates updated to match new bottom zone.

**Audit result:** ALL CHECKS PASSED (non-ASCII, image IDs, fonts, BMPs, %VB).

**Files changed:**
- `wps/Apple2026.wps` -- WPS correction pass

**Anti-regression note:** `wpsArtCorners.bmp` remains on disk as an asset but is
no longer referenced in the WPS. Do not re-add unless art viewport dimensions are
verified to match the corner mask bitmap exactly (it was sized for 150x150 but the
overlay was 156x156 causing misalignment).

**Next:** Simulator build + screenshot verification of new layout.

---

### Major Apple2026 Polish Pass — System-Level Rebuild (2026-04-09)

**Goal:** Comprehensive system-level polish pass addressing 14 user-reported issues.
This pass went beyond theme-layer patches into core Rockbox code where needed.

**Root causes identified and fixed:**

1. **System-wide text clipping** — SBS viewport heights were sized to nominal font
   point sizes (e.g., "16pt" = 16px viewport) instead of actual RB12 bitmap heights
   (16pt font = 20px bitmap). Every viewport using font slot 3 was clipping 3-5px;
   the large title viewport clipped 1px of "Library."

2. **WPS progress bar regression** — Bitmap pill bars were removed in a prior pass
   (due to a right-edge artifact) and replaced with code-drawn flat rectangles.
   Rockbox has no native rounded-rect primitive, so bitmap-based bars are required
   for the pill shape.

3. **Music library back-navigation regression** — `path_is_curated_music_library_root()`
   was checking `browse->root` (which could be a deep resumed path like
   `/Music/Artist/Album/`) instead of `tc.currdir`. When `browse->root` was deep,
   the intercept at `/Music/` never fired, and users fell through to filesystem
   root `/` seeing Calendar/Contacts/Notes.

**Changes made:**

1. **SBS viewport height corrections** (Apple2026.sbs):
   - `large_title`: h=30 -> h=32 (font slot 2, RB12 h=31)
   - `title` / `titlewide`: h=17 -> h=20 (font slot 3, RB12 h=20)
   - `batterytext` / `batterytext_root`: h=15 -> h=20
   - `mp_track` / `mp_track_noart`: h=16 -> h=20
   - Header background: 28px -> 30px
   - Content viewport starts adjusted: y=29/40 -> y=30/38
   - Battery icon positions moved up 2-4px (user requirement #4)
   - All icon/indicator y-positions adjusted to match new header geometry

2. **WPS complete rebuild** (Apple2026.wps):
   - All viewport heights corrected for actual RB12 font heights
   - Title VP: h=17 -> h=20 (font slot 9, RB12 h=20)
   - Artist/Album VP: h=16 -> h=18 (font slot 3 = 14pt, RB12 h=18)
   - Battery text VP: h=15 -> h=18
   - Playlist counter VP: h=14 -> h=18
   - Elapsed/remaining time VPs: height corrected to 18
   - Progress bars: restored bitmap pill bars (pb.bmp, pb_backdrop.bmp,
     pb_active.bmp, pb_active_backdrop.bmp) via %xl preloads + image/backdrop params
   - Volume bar: restored bitmap pill bars (vb.bmp, vb_backdrop.bmp)
   - Bottom-zone layout recalculated for corrected font heights

3. **Music library back-navigation fix** (apps/tree.c):
   - Changed intercept at `ACTION_STD_CANCEL` to check `tc.currdir` against
     `/Music/` instead of `browse->root`. Now catches the `/Music/` boundary
     regardless of the resumed depth, preventing fall-through to filesystem root.

4. **Icon color model rework** (tools/apple2026_symbol_assets.py):
   - Changed from mixed red/grey to: all icons red EXCEPT structural nav chrome
     (cursor, submenu chevrons, moving indicator). Main menu now reads as a
     strong red Apple Music-style icon surface.
   - Icon scale reduced from 1.0 to 0.88, padding increased from 1 to 3,
     giving better proportion with both 18pt and 16pt text tiers.

5. **Semantic music icons** (apps/gui/icon.h, apps/tree.c, apple2026_symbol_assets.py):
   - Added `Icon_Artist` (person.fill) and `Icon_Album` (square.stack.fill) to
     the themable_icons enum (iPod targets only, guarded by MODEL_NUMBER).
   - Icon strip extended from 32 to 34 tiles.
   - `tree_get_fileicon()` now returns `Icon_Artist` for directories at music
     library depth 0 and `Icon_Album` at depth 1+, when `BROWSE_APPLE2026_MUSICLIB`
     flag is set.

6. **Menu density improvement** (apps/gui/list.c):
   - Normal tier row floor: 32px -> 30px (gains ~1 visible row with mini-player)
   - Dense tier row floor: 30px -> 28px (gains ~1 visible track row)
   - With 30px rows + 38px large title + 44px mini-player:
     root+playing shows 5 items (was 4). Root+stopped shows 6 (was 5).

7. **Icon asset regeneration** (icons/Apple2026Icons.bmp + all wps/Apple2026/*.bmp):
   - Regenerated via apple2026_symbol_assets.py with updated color model + scale.
   - 34-tile strip verified (all tiles have ink content).

**Files changed:**
- `wps/Apple2026.sbs` — viewport heights, header geometry, battery position
- `wps/Apple2026.wps` — complete rebuild with corrected heights + pill bars
- `wps/WPSLIST` — updated geometry comments
- `apps/tree.c` — back-navigation fix + semantic music icons
- `apps/gui/list.c` — row floor reduction (32->30 normal, 30->28 dense)
- `apps/gui/icon.h` — added Icon_Artist, Icon_Album
- `tools/apple2026_symbol_assets.py` — icon color model + scale + new icons
- `icons/Apple2026Icons.bmp` — regenerated (34 tiles)
- `wps/Apple2026/*` — all WPS assets regenerated

**WPS audit result:** ALL CHECKS PASSED (non-ASCII, image IDs, fonts, BMPs, %VB).

**What required core Rockbox changes vs theme-level:**
- CORE: `apps/gui/icon.h` (new icon enum entries), `apps/gui/list.c` (row floors),
  `apps/tree.c` (navigation intercept + icon assignment)
- THEME: `Apple2026.sbs`, `Apple2026.wps`, `WPSLIST`, icon/asset generation

**What still remains:**
- On-device or simulator visual verification of all changes
- Verify pill bar BMPs render without right-edge artifact at new dimensions
- Verify `Icon_Artist` and `Icon_Album` render correctly in music browser
- Fine-tuning of artist/album subline timing if still unstable
- Settings menu icon grey distinction (currently all red except nav chrome;
  runtime context-aware tinting would require icon rendering code changes)

**Next:** Build verification (simulator and/or hardware).

---

## 2026-04-10 — Music-library back-navigation deep audit and fix

**User report:** "The flow from music to albums and then back still returns me to the file menu sometimes" — intermittent regression.

**Deep audit findings (4 bugs identified):**

**Bug 1 — `browse_current` poisoned the resume path for `GO_TO_MUSICLIB`.**
When `Settings → General Settings → File Browser → Follow Playlist` is ON and the user returns from WPS, the old code set `folder = current_track_path` (e.g. `/Music/Artist/Album/Song.mp3`). This opened the music session at the album folder depth, bypassing the curated-root logic. Fixed by removing the `browse_current` / `last_screen == GO_TO_WPS` branch for `GO_TO_MUSICLIB` entirely — the music library always resumes at the last-visited music folder (validated under `/Music/`) or the library root.

**Bug 2 — `last_music_folder` was not validated to stay inside `/Music/`.**
The post-browse save only reset to `/` when there was no second separator. A weird non-Music path with a separator would persist. Fixed by checking `strncmp(last_music_folder, "/Music", 6)` on both entry and exit.

**Bug 3 — `GO_TO_PREVIOUS` from the music library browse could open `GO_TO_FILEBROWSER`.**
`dirbrowse()` returns `GO_TO_PREVIOUS` when `ft_load` fails on a stale path. In `load_screen()`, `GO_TO_PREVIOUS` restores `last_screen` to whatever was before Music. If that was `GO_TO_FILEBROWSER = 0`, root_menu routes to the file browser. Fixed by intercepting `GO_TO_PREVIOUS` from `GO_TO_MUSICLIB` and mapping it to `GO_TO_ROOT`.

**Bug 4 — Apple2026 tree.c guard lacked `dirlevel <= 1` safety.**
Added `tc.dirlevel <= 1` to the guard to ensure it only fires at the music root level, not from a deep subfolder with a miscounted dirlevel.

**Files changed:**
- `apps/root_menu.c` — removed `browse_current` path for MUSICLIB, strengthened `last_music_folder` validation, added `GO_TO_PREVIOUS → GO_TO_ROOT` intercept
- `apps/tree.c` — added `dirlevel <= 1` guard condition

**Expected behavior after fix:**
- Music → Artists view → back: goes to main menu (1 press from library root)
- Music → Artist → Albums → back: Artists (1), then back → main menu (1)
- Never exposes the raw filesystem browser

---

## 2026-04-10 - Apple2026 architecture audit document

Goal: complete a source-level Apple2026 audit of Rockbox UI architecture,
renderer assumptions, shell continuity, and plugin boundaries, then capture
the findings in a dedicated document instead of scattering them through the
work log.

Changed:
- `Apple2026ArchitectureAudit.md`
  - Added a dedicated architecture audit covering:
    - list renderer, row model, and menu framework
    - skin engine, WPS/SBS runtime, and viewport model
    - typography, font rendering, text layout, clipping, and baseline control
    - icon rendering, bitmap/image pipeline, and asset constraints
    - dialogs, quickscreen, context menus, and utility surfaces
    - navigation, action/keymap flow, activity stack, and screen return behavior
    - plugin architecture and PictureFlow integration
    - playback-shell coordination, music-state continuity, redraw, buffering,
      responsiveness, and code duplication
  - Classified each major limitation as theme-impossible, theme-awkward,
    source-fixable, architecture-required, and/or branch-justified.
  - Added branch-level recommendations for a new Apple2026-focused Rockbox UI
    foundation.

Verified:
- Audit findings were tied back to current source behavior in the inspected
  Rockbox subsystems, not just theme symptoms.
- The document explicitly separates:
  - what theming cannot solve cleanly
  - what can be improved with contained source work
  - what requires deeper architecture changes
  - what should define a dedicated Apple2026 branch

Docs:
- `Apple2026ArchitectureAudit.md` is now the dedicated home for this pass.
- `WORK_LOG.md` keeps only this pointer entry rather than duplicating the full
  findings.

Next:
- Use `Apple2026ArchitectureAudit.md` to define the first implementation wave
  for a new Apple2026 UI-core branch:
  - unified shell routing/state
  - reusable row/cell renderer
  - transient surface framework
  - text role/layout abstraction
  - plugin and PictureFlow shell bridge

---

## 2026-04-10 - Apple2026 detailed rewrite-planning extension

Goal: push the architecture audit beyond subsystem findings into a concrete
rewrite plan with proposed modules, interfaces, migration order, and
acceptance gates for the new Apple2026 UI-core branch.

Changed:
- `Apple2026ArchitectureAudit.md`
  - Added a detailed rewrite architecture section covering:
    - proposed `apps/ui/*` module layout
    - route model, shell state, and navigation-intent abstraction
    - surface lifecycle contract
    - row model / row renderer API direction
    - text-role / text-layout abstraction
    - semantic asset registry direction
    - plugin bridge contract for shell-integrated plugins
  - Added a staged migration plan from legacy root/menu/tree/WPS/plugin code
    into the new UI core.
  - Added file-by-file rewrite strategy, non-goals for the first foundation
    wave, and explicit acceptance gates for routing, rows, typography,
    transient surfaces, and plugin integration.

Verified:
- The rewrite plan now names concrete source modules and migration targets
  instead of only describing architecture direction at a high level.
- The plan is aligned to the existing Rockbox seams currently exposed through:
  - `GO_TO_*` screen routing
  - `browse_context` / `tree_context`
  - `action` / keymap context routing
  - `current_activity`
  - `playback_source`

Docs:
- `Apple2026ArchitectureAudit.md` is now the primary architecture-planning
  document for the next branch-definition step.

Next:
- Convert the new detailed plan into the first implementation checklist:
  1. shell router/state scaffolding
  2. playback-anchor replacement for WPS back behavior
  3. row renderer migration for root + music library

---

## 2026-04-11 - Apple2026 SBS/WPS header-shell forensic load-failure audit

Goal: determine whether the Apple2026 header/shell regression was first a true
skin load failure or a later layout/routing bug, then prove the root cause with
runtime evidence before making any visual changes.

Changed:
- `wps/Apple2026.sbs`
  - Reduced the fatal right-aligned sleep viewport width from `53` to `52`:
    - `%Vl(home_lock_sleep,-52,10,53,18,8)` -> `%Vl(home_lock_sleep,-52,10,52,18,8)`
- `wps/Apple2026.wps`
  - Reduced the matching fatal right-aligned sleep viewport width from `53` to
    `52`:
    - `%Vl(lock_sleep,-52,10,53,18,3)` -> `%Vl(lock_sleep,-52,10,52,18,3)`
- `tools/apple2026_skin_audit.py`
  - Added a repeatable Apple2026 skin audit for:
    - viewport bounds
    - duplicate viewport labels
    - approximate token counts
    - `%xl()` preload decode budgets
    - optional package drift checks against runtime roots and zip artifacts
- `Apple2026SBSFailureAudit.md`
  - Added the dedicated forensic record for this regression.

Verified:
- The failing state was a real parser/load failure, not a first-pass routing
  issue.
- `debugwps.out` captured the actual fatal parse points:
  - `Error on line 279.`
  - `%Vl(home_lock_sleep,-52,10,53,18,8)`
  - `Error on line 276.`
  - `%Vl(lock_sleep,-52,10,53,18,3)`
- `apps/gui/skin_engine/skin_parser.c` rejects a viewport when:
  - `skin_vp->vp.width + skin_vp->vp.x > display->lcdwidth`
- On a 320px display, `x=-52` resolves to `268`; `268 + 53 = 321`, so both
  directives overflow by one pixel and abort parsing.
- Post-fix runtime logs in `postfix_debugwps.err` show:
  - `Apple2026.sbs` loaded with `loaded=1 fallback=0 failsafe=0`
  - `Apple2026.wps` loaded with `loaded=1 fallback=0 failsafe=0`
  - `sb_skin_get_info_vp ... found=1`
- `python tools/apple2026_skin_audit.py` now returns `OK`.
- The active simulator config still points to:
  - `wps: /.rockbox/wps/Apple2026.wps`
  - `sbs: /.rockbox/wps/Apple2026.sbs`
- Hypotheses ruled out as the primary current cause:
  - token overflow
  - wrong active config paths
  - duplicate viewport labels
  - CRLF corruption
  - missing current simulator fonts
  - skin-buffer exhaustion as the first failing condition
- Important packaging trap verified:
  - stale extracted hardware `.rockbox` directories did not match source
  - rebuilt `build-hw-ipodvideo/rockbox.zip` and `build-hw-ipod6g/rockbox.zip`
    do match source hashes for `Apple2026.sbs` and `Apple2026.wps`

Docs:
- `Apple2026SBSFailureAudit.md` is the dedicated forensic record for:
  - exact symptom set
  - tested hypothesis tree
  - proven load-state determination
  - actual root cause
  - ruled-out branches
  - fix strategy and anti-regression checks

Next:
- Treat SBS/WPS load-state verification as the first gate in the next build.
- If Apple2026 regresses again after a confirmed successful skin load, then
  investigate viewport routing, state logic, and refresh behavior as secondary
  layers rather than first causes.

---

## 2026-04-11 - Apple2026 album-art geometry regression fix

Goal: fix the regression where WPS hero album art rendered tiny and the SBS
bottom mini-player album art rendered oversized/cropped, without destabilizing
 skin loading.

Changed:
- `wps/Apple2026.wps`
  - Removed the secondary lockscreen notification `%Cl/%Cd` album-art load.
  - Kept the primary WPS hero album-art definition as the only `%Cl` in the
    file (`150x150` hero art block).
- `wps/Apple2026.sbs`
  - Removed the secondary home-lock notification `%Cl/%Cd` album-art load.
  - Kept the primary mini-player album-art definition as the only `%Cl` in the
    file (`32x32` mini-player thumbnail block).
- `tools/apple2026_skin_audit.py`
  - Added a guard that fails the audit if a skin file contains multiple `%Cl`
    album-art loads.

Verified:
- Root cause proved from engine behavior:
  - `struct wps_data` carries a single `albumart` pointer and a single
    `playback_aa_slot` per skin.
  - `parse_albumart_load()` overwrites `wps_data->albumart` each time `%Cl`
    appears.
  - This means the later 44x44 lockscreen `%Cl` in each file was overriding the
    primary art geometry:
    - WPS hero art was being forced to use the later small lockscreen art role.
    - SBS mini-player art was being forced to use the later larger lockscreen
      art role and got cropped in the 32x32 lane.
- Static verification:
  - `rg "%Cl\\(" wps/Apple2026.wps wps/Apple2026.sbs` now reports exactly one
    `%Cl` per file.
- Packaging/runtime verification:
  - `python tools/apple2026_skin_audit.py --zip-artifact build-hw-ipod6g\\rockbox.zip`
    returns `OK`.
  - `./tools/verify-sim-install.ps1` returns `OK`.
  - `./build-sim.ps1 -InstallOnly -StrictInstall` completed successfully.
  - `./build-hw.ps1 -Target ipod6g -Incremental` completed successfully.

Docs:
- `WORK_LOG.md` now records the actual regression cause and the album-art
  single-definition safeguard.

Next:
- If album art regresses again, check for any newly added secondary `%Cl`
  blocks before touching geometry or viewport coordinates.

---

## 2026-04-11 - Source-isolated WPS navigation model

Goal: replace the coarse/stale WPS return model with an explicit playback
return context so WPS Up/back restores the source the user actually started
playback from instead of reconstructing it from `playback_source`,
`previous_browser`, `current_track_path`, or a forced cross-surface return.

Changed:
- `apps/settings.h`
  - Added a RAM-only `enum playback_context` and explicit playback return
    fields on `global_status`:
    - `ROOT`
    - `PICTUREFLOW_TRACKLIST`
    - `FILESYSTEM`
    - `DATABASE`
    - `PLAYLIST`
    - `QUEUE`
- `apps/root_menu.c`
  - Added shared playback-context setter helpers.
  - Root-entered WPS (`Now Playing` / resume from Main Menu) now stamps
    `PLAYBACK_CONTEXT_ROOT`.
  - File/music/database browser re-entry from WPS now prefers the explicit
    playback context over `previous_browser`/`browse_current`/stale browser
    state.
- `apps/tree.c`
  - Playback start from filesystem/music now captures the exact selected path
    and target browser screen (`GO_TO_MUSICLIB` vs `GO_TO_FILEBROWSER`).
  - Playback start from database now captures explicit `dirlevel` and
    `selected_item`.
  - Playback start from playlist-browser (`SHOW_M3U`) now stamps playlist
    context instead of relying on generic browser history.
- `apps/playlist_viewer.c`
  - Split saved-playlist playback from queue/current-playlist playback:
    - queue/current playlist -> `PLAYBACK_CONTEXT_QUEUE`
    - saved playlist viewer -> `PLAYBACK_CONTEXT_PLAYLIST`
- `apps/gui/wps.c`
  - WPS Up/back now routes from `global_status.playback_context` first.
  - Legacy `playback_source` handling remains only as fallback.
- `apps/plugins/pictureflow/pictureflow.c`
  - PictureFlow playback now stamps `PLAYBACK_CONTEXT_PICTUREFLOW_TRACKLIST`.
  - Reopened Cover Flow tracklists now reselect the playing track within the
    album when possible instead of only reopening the plugin surface.

Verified:
- Code compiles successfully:
  - `./build-sim.ps1 -Incremental -SkipDep`
- Installed simulator tree is current:
  - `./tools/verify-sim-install.ps1` -> `Result: OK`
- Static behavior now matches the intended routing model:
  - Cover Flow playback returns through explicit Cover Flow tracklist context.
  - Filesystem/Music playback returns through explicit filesystem context with
    the captured path, not `GO_TO_PREVIOUS_BROWSER`.
  - Database playback returns through explicit database browse state.
  - Saved playlist playback and queue playback no longer share the same WPS
    return bucket.

Assumed / not yet interactively verified:
- Live simulator button-path validation for all target flows:
  - Cover Flow -> tracklist -> WPS -> back
  - Music/filesystem album -> WPS -> back
  - Database album -> WPS -> back
  - Playlists -> saved playlist viewer -> WPS -> back
  - Queue/current playlist -> WPS -> back
- Shuffle-play edge cases remain intentionally out of scope unless they block
  the primary source-isolated paths.

Next:
- Run live simulator navigation checks for the five source-return flows above.
- If any mismatch remains, tighten the context payload at the specific source
  rather than adding more fallback reconstruction logic in WPS.

---

## 2026-04-11 - Lockscreen redraw triage for post-reboot regression

Goal: isolate the lockscreen regression where hold could show duplicate battery
chrome, missing notification art, and black boxes after reboot.

Changed:
- `apps/gui/skin_engine/skin_engine.c`
  - `skin_do_full_update()` now forces a full redraw when the main-screen hold
    state changes, so WPS/SBS overlays do not rely on a partial non-static
    refresh when switching into or out of lockscreen mode.
- `wps/Apple2026.wps`
  - `lock_notif_art` now explicitly fills its 44x44 lane with the card
    background color instead of leaving the viewport empty after the single
    `%Cl` safeguard removed the secondary album-art load.
- `wps/Apple2026.sbs`
  - `home_lock_notif_art` now does the same explicit fill.

Verified vs assumed:
- Verified from source: each skin still has only one `%Cl`, so the lockscreen
  notification art lane cannot display a second independent album-art size in
  the current engine model.
- Assumed until runtime check: the duplicate battery symptom is stale base-layer
  content surviving a partial refresh on hold transition; the new full-redraw
  gate is intended to prove or disprove that quickly.

---

## 2026-04-11 - Apple2026 Filesystem Typography Hierarchy Fix

Goal: stabilize the Apple2026 filesystem two-tier font selection, removing a state regression where visiting a tracklist (dense font) could force the root menus (normal font) into the dense size when backing out.

Changed:
- pps/tree.c: explicitly defaulted ockpod_list_font_tier to ROCKPOD_LIST_FONT_NORMAL at the start of directory drawing, overriding any leaked state from previous screens.
- pps/menu.c: added ockpod_list_font_tier = ROCKPOD_LIST_FONT_NORMAL; explicitly on menu startup, guaranteeing that native screens never incorrectly inherit tracklist density.
- pps/root_menu.c: cleaned up obsolete font resets.

Verified:
- Incremental compile completes and simulator runtime passes static checks.
- Static audit shows all entry points (menu drawing, directory loading) proactively establish explicit font state before lists are initialized.

Docs:
- Apple2026ListHierarchyAudit.md / implementation_plan.md created, defining the default behavior as NORMAL for menus and directories and DENSE for tracklists.

Next:
- Check visual transitions when navigating backwards from Music -> Artist -> Album -> Tracklist or Playlist Viewer -> Main Menu.

---

## 2026-04-12 - Opus audit synthesis and priority extraction pass

Goal: turn the three external Opus audits into a current-repo, implementation-ready Apple2026 priority plan without treating the remote audit as source of truth.

Changed:
- `OpusAuditPriorityPlan.md`
  - Added a ranked Apple2026 implementation handoff based on:
    - `OpusAudit1`
    - `OpusAudit2`
    - `OpusAudit3`
    - current repo source verification
    - current branch docs/worklogs/build notes

Verified:
- Confirmed in current source:
  - PictureFlow cleanup still does not clear `skin_render_inhibit_flush(false)`.
  - `BUILD.md` release packaging still points to the wrong GitHub repo.
  - SF Pro font-policy docs are contradictory with the actual checked-in font files.
  - `%Lo` root detection is too broad for `ACTIVITY_MAINMENU`-backed submenus.
  - Apple2026 list dynamic fill still causes row-height relayout.
  - WPSLIST still does not reassert `dynamic colors: off` or `backlight on button hold: normal`.
  - `apple2026_skin_audit.py` still misses theme-config/font-contract checks.
  - `rockpod_list_font_tier` is still a global mutable.
  - chevrons still draw on leaf rows.
  - lockscreen notification art is still a placeholder rectangle.
  - track-number stripping in `tree.c` is still too aggressive.
  - `playback_source` is still a live routing fallback and PictureFlow still depends on it for one return path.
- Rejected or downgraded after source review:
  - file-level `playback_context_path` handling as a current Music bug
  - `rootmenu_track_changed_callback` poisoning Music state
  - PowerShell wrapper audit-coverage gap
  - quickscreen rewrite/delete recommendation as an immediate code action
  - dynamic-colors and hold-backlight issues as current compiled-default regressions
  - immediate SBS rewrite for double-battery symptom without first runtime-validating the new hold-refresh fix

Assumed / not yet runtime-verified:
- whether the recent hold-transition full-redraw fix fully eliminates the lockscreen duplicate-battery/black-box symptom on live simulator runs
- whether the `%pS(0.2)` lockscreen refresh hack is still user-visible enough to justify source-side replacement now
- whether the current deterministic-parent quickscreen path has any remaining live regression despite matching the branch's own stabilization docs

Docs:
- `OpusAuditPriorityPlan.md` is now the implementation-facing priority document for the next Apple2026 coding pass.

Next:
- Execute the ranked immediate set first:
  1. PictureFlow flush guard cleanup
  2. release/build doc and policy correction
  3. root/header regression fix
  4. list-row stability and theme self-heal defaults
  5. audit-contract expansion before the next beta build

---

## 2026-04-12 - Opus audit continuation: regression-first reprioritization

Goal: tighten the Apple2026 priority plan so bug fixes and regressions stay above documentation and architecture work, and so partially mitigated regressions are not treated as solved without current runtime proof.

Changed:
- `OpusAuditPriorityPlan.md`
  - Added a regression-first override to the methodology.
  - Added an explicit regression-first execution order.
  - Added three new unclosed regression-gate items:
    - quickscreen live revalidation
    - lockscreen hold-transition live revalidation
    - Music vs Files / WPS return-matrix live revalidation
  - Reframed partially mitigated quickscreen and lockscreen findings as
    "not closed until current runtime validation".

Verified:
- Current source still contains enough historical regression risk that these
  surfaces should not be considered closed from static review alone:
  - quickscreen
  - lockscreen hold transitions
  - WPS return routing across Music / Files / DB / playlists / Cover Flow
- The current Apple2026 audit tool also has a trust gap:
  - `tools/apple2026_skin_audit.py` returns success when an explicitly
    requested `--package-root` does not exist, which can mask a missing
    simulator install tree unless another check catches it.

Docs:
- `OpusAuditPriorityPlan.md` is now explicitly regression-first instead of
  mixed-priority by default.

Next:
- Treat the next coding pass as:
  1. regression closure and runtime proof
  2. only then release/docs trust cleanup
  3. only then architecture/backlog cleanup

---

## 2026-04-12 - Opus audit continuation: unresolved bug/trust gaps still open

Goal: keep extending the Opus synthesis without assuming the last build or later docs fully closed the audited bugs.

Changed:
- `OpusAuditPriorityPlan.md`
  - Tightened quickscreen from "runtime-only concern" to "partially confirmed in live source":
    `quickscreen_set_parent_apple2026()` still exists and the Apple2026 path still
    passes `NULL` to `viewportmanager_theme_enable()`.
  - Expanded the audit-tooling item to include a concrete false-positive bug:
    explicit missing `--package-root` currently returns `OK`.
  - Added three more grounded items:
    - Apple2026 version-stamp / stale-config self-heal
    - README capability/beta-stance correction
    - PictureFlow tracklist density normalization

Verified:
- `python tools/apple2026_skin_audit.py --package-root .does-not-exist` -> `OK`
  - The current audit gate can silently pass a missing runtime tree.
- `apps/gui/quickscreen.c`
  - Apple2026 still uses the fullscreen custom-parent path:
    `viewportmanager_theme_enable(..., NULL)` + `quickscreen_set_parent_apple2026(...)`.
- `rg -n "apple2026_version|APPLE2026_BUILD_VERSION|\\.apple2026_version" .`
  - No live version-stamp/self-heal implementation exists outside the Opus docs.
- `build-sim.sh` / `build-hw.sh`
  - Both still inject `config.cfg`, but neither emits an Apple2026 version sentinel.
- `README.md`
  - Still claims both devices share all UI features including dynamic colors and still uses
    "drop-in replacement" language.
- `apps/plugins/pictureflow/pictureflow.c`
  - `pf_track_row_height()` still returns raw font height; no Apple2026 dense-row floor exists there.

Docs:
- `OpusAuditPriorityPlan.md` now carries 21 items instead of 18 and explicitly records:
  - the audit false-positive
  - the missing version-stamp/self-heal path
  - the still-live quickscreen parent-ownership risk

Next:
- Treat the next coding pass in this order:
  1. live regression closure (`quickscreen`, `lockscreen`, `Music/Files/WPS`, `PictureFlow`)
  2. theme/audit self-heal hardening (`WPSLIST`, audit script, version stamp)
  3. only then README/release trust cleanup and lower-priority visual consistency work

---

## 2026-04-12 - Update3 execution pass: source fixes landed, build/package gates revalidated

Goal: start executing the regression-first Apple2026 priority plan on a new
`Update3` branch, land the highest-value source fixes, and refuse to count an
item as closed unless it was either statically fixed and rebuilt or explicitly
recorded as still needing interactive runtime proof.

Changed:
- Created branch: `Update3`
- Core shell / navigation / list fixes:
  - `apps/plugins/pictureflow/pictureflow.c`
    - clear `skin_render_inhibit_flush(false)` at `cleanup()` and at
      `pictureflow_main()` entry so PictureFlow cannot strand the SBS in a
      non-flushing state after abnormal exits.
  - `apps/gui/quickscreen.c`
    - Apple2026 quickscreen now enables the themed viewport with the actual SBS
      parent viewport instead of `NULL`.
    - removed the fullscreen custom-parent rebuild path and the hardcoded
      `y += 30` / `height -= 30` viewport surgery.
  - `apps/gui/skin_engine/skin_tokens.c`
    - `%Lo` root detection now keys off the actual title (`LANG_ROCKBOX_TITLE`)
      instead of any `ACTIVITY_MAINMENU`, so nested main-menu-backed screens no
      longer masquerade as the root Library surface.
  - `apps/tree.c`
    - Apple2026 `/Music` back guard now keys off the exact curated `/Music`
      root path without the fragile `dirlevel <= 1` dependency.
    - track-number stripping is now limited to audio files in the curated Music
      browser at deeper levels and only strips strict `NN. Title` style prefixes.
  - `apps/gui/list.c`
    - removed the Apple2026 dynamic-fill row-height block so list rhythm no
      longer changes just because a viewport is tall.
  - `apps/gui/list.h`, `apps/gui/list.c`, `apps/gui/bitmap/list.c`,
    `apps/root_menu.h`, `apps/root_menu.c`, `apps/menu.c`, `apps/tree.c`,
    `apps/playlist_viewer.c`
    - moved Apple2026 font-tier ownership from the global mutable into each
      `gui_synclist`.
    - added a per-list navigability callback so chevrons only draw on rows that
      actually navigate.
- Theme / default-setting / self-heal fixes:
  - `wps/WPSLIST`
    - Apple2026 now explicitly reasserts:
      - `dynamic colors: off`
      - `backlight on button hold: normal`
  - `wps/wpsbuild.pl`
    - generator now parses and emits those two settings into generated
      `themes/Apple2026.cfg`, closing the source-to-package gap.
  - `apps/main.c`
    - added Apple2026 version-stamp self-heal (`.apple2026_version`) and
      runtime theme refresh path for stale config states on models `5` and `71`.
  - `build-sim.sh`, `build-hw.sh`
    - inject `.apple2026_version`
    - align injected `config.cfg` with Apple2026 defaults
    - force `font: /.rockbox/fonts/18-SFProText-Regular.fnt`
    - include `dynamic colors: off`
    - include `backlight on button hold: normal`
  - `build-sim.sh`
    - install-only path now rewrites `config.cfg` too; previously it only
      refreshed theme files and left stale runtime config behind.
  - `build-hw.sh`
    - now stages all repo `fonts/*.fnt` into `rockbox.zip`, matching simulator
      runtime behavior and closing the Apple2026 hardware font-packaging gap.
- Build / audit / release trust fixes:
  - `tools/apple2026_skin_audit.py`
    - explicit missing `--package-root` now fails
    - source contract now checks Apple2026 WPSLIST defaults
    - runtime/zip contract now checks `Apple2026.cfg` settings and
      `.apple2026_version`
  - `tools/verify-sim-install.ps1`
    - now checks `.apple2026_version`
  - `BUILD.md`
    - release packaging now targets `Poorfocus/Rockbox-UI-UX-Overhaul`
    - release guidance now points at fresh `build-hw-*/rockbox.zip` artifacts
      instead of stale extracted folders or the upstream `origin` repo
  - `README.md`, `fonts/FONTS.md`
    - removed the misleading "drop-in replacement" / "all UI features including
      dynamic colors" language
    - aligned SF Pro font wording with the actual checked-in Apple-derived
      bitmap fonts and the repo `NOTICE`

Verified:
- Source / audit:
  - `python tools/apple2026_skin_audit.py --source-only`
    - Result: `OK`
- Simulator:
  - `.\build-sim.ps1 -Incremental -SkipDep -StrictInstall`
    - initially failed for a real reason:
      - generated `themes/Apple2026.cfg` was missing
        `dynamic colors: off` and `backlight on button hold: normal`
    - fixed by patching `wpsbuild.pl`
  - `.\build-sim.ps1 -InstallOnly -StrictInstall`
    - Result after fixes: `OK`
  - `.\tools\verify-sim-install.ps1`
    - Result after fixes: `OK`
    - no remaining `config.cfg` vs `Apple2026.cfg` mismatch after the install-only
      rewrite bug was fixed
- Hardware:
  - `.\build-hw.ps1 -Target ipod6g -Incremental -SkipDep`
    - first pass reached `make zip` but failed the Apple2026 audit because the
      zip did not include runtime-only fonts such as
      `20-SFProText-Regular.fnt` / `22-SFProText-Medium.fnt`
    - fixed by staging all repo `fonts/*.fnt` into `rockbox.zip`
    - second pass: `OK`
  - `.\build-hw.ps1 -Target ipodvideo -Incremental -SkipDep`
    - Result after the same font-packaging fix: `OK`
- Current package/runtime truth:
  - both hardware zips now contain:
    - `.rockbox/config.cfg`
    - `.rockbox/.apple2026_version`
    - the Apple2026 runtime font set required by simulator/audit expectations
  - both hardware zips and the simulator runtime now pass
    `tools/apple2026_skin_audit.py`

Assumed / still not interactively proven:
- quickscreen:
  - source-side deterministic parent/viewport fix is in place
  - no headless simulator path in this environment can actually exercise the
    Apple2026 quickscreen interaction flow, so the runtime regression is still
    treated as `needs manual simulator proof`
- lockscreen:
  - source-side default / hold-state hardening is in place
  - duplicate-battery / black-box / `%pS(0.2)` behavior still requires manual
    simulator or hardware exercise before it should be called closed
- Music vs Files / WPS return matrix:
  - root-path guard and context-first routing are materially stronger now
  - still needs manual path-walk validation across:
    - Main Menu -> Music
    - Extras -> Files
    - WPS back/parent
    - playlist / database / Cover Flow return flows
- lockscreen notification art:
  - still not fully normalized under Rockbox's one-`%Cl`-per-skin limitation
  - remains an open Apple2026 polish task, not a build blocker

Docs:
- `OpusAuditPriorityPlan.md`
  - remains the execution handoff, but the code/build items above are now
    actually landed on `Update3`

Next:
- Manual validation pass required for the still-runtime-open items:
  1. quickscreen on live simulator
  2. hold-transition lockscreen behavior on live simulator
  3. Music / Files / WPS back-navigation matrix
- After those are manually proven or disproven:
  1. close any remaining runtime regressions they expose
  2. revisit lockscreen notification art normalization only if the higher-risk
     runtime behaviors are stable

---

## 2026-04-12 - Opus priority continuation: dead playback_source state removed, runtime validation deferred

Goal: continue the priority plan without spending this pass on the broken
simulator-smoke path, and land the highest-value non-runtime simplification
still justified by the live repo.

Changed:
- `apps/settings.h`
  - removed the dead `playback_source` enum, field, and setter declaration
    now that the branch no longer has any readers for that state
- `apps/root_menu.c`, `apps/root_menu.h`
  - removed the dead `playback_source_set()` implementation
  - updated stale comments that still described back-navigation in terms of a
    `playback_source` chain instead of the surviving browse/context model
- `apps/tree.c`
  - renamed `tree_set_playback_source_for_wps()` to
    `tree_set_playback_context_for_wps()`
  - deleted the dead `playback_source` writes so tree playback now stamps only
    the live `playback_context` state
- `apps/playlist_viewer.c`
  - deleted dead `playback_source` writes after playlist playback starts
- `apps/plugins/pictureflow/pictureflow.c`
  - deleted the final dead `playback_source` write after PictureFlow stamps
    `PLAYBACK_CONTEXT_PICTUREFLOW_TRACKLIST`
- `OpusAuditPriorityProgress.md`
  - added explicit status tracking for completed, already-done, and still
    blocked priority items

Verified:
- `rg -n "playback_source" apps`
  - result: no live references remain in `apps/`
- `.\build-sim.ps1 -Incremental -SkipDep -StrictInstall`
  - result: `OK`
- `python tools/apple2026_skin_audit.py --source-only`
  - result: `OK`

Blocked / deferred:
- `#16` quickscreen runtime closure
- `#17` lockscreen hold-transition runtime closure
- `#18` Music / Files / WPS return-matrix runtime closure
- `#14` lockscreen `%pS(0.2)` replacement decision
- the attempted simulator-smoke path was explicitly deferred for now per user
  direction, so these remain runtime-open instead of being guessed closed

Docs:
- created `OpusAuditPriorityProgress.md`
- updated `WORK_LOG.md`

Next:
- return to live simulator or hardware validation for `#16`, `#17`, and `#18`
  when the runtime path is worth revisiting
- if runtime validation stays deferred, the next non-runtime cleanup candidate
  is `#15` stale preload/comment normalization

---

## 2026-04-12 - Opus priority continuation: dead Apple2026 skin leftovers trimmed without runtime work

Goal: keep moving through the priority plan on the static-only path and remove
dead Apple2026 skin leftovers that no longer serve the current shell.

Changed:
- `wps/Apple2026.wps`
  - removed the dead `unlocked` placeholder viewport and simplified the hold
    icon branch to render nothing when hold is off
  - removed the dead `album_line` placeholder viewport and simplified the WPS
    artist/album routing to the one alternating viewport that actually draws
- `wps/Apple2026.sbs`
  - removed the dead `unlocked` placeholder viewport that was no longer
    referenced anywhere
- `tools/apple2026_symbol_assets.py`
  - stopped generating the orphan `albumFramed.bmp` asset
- `tools/apple2026_wps_pill_bars.py`
  - stopped generating the orphan `albumShadow.bmp` asset
- deleted unused Apple2026 bitmap leftovers from `wps/Apple2026/`:
  - `QSArrowDown.bmp`
  - `QSArrowLeft.bmp`
  - `QSArrowRight.bmp`
  - `QSArrowUp.bmp`
  - `QSButtonActive.bmp`
  - `QSButtonInactive.bmp`
  - `albumFramed.bmp`
  - `albumShadow.bmp`
  - `wpsArtCorners.bmp`

Verified:
- `rg` found no remaining references to the removed placeholder assets/viewports
- `python tools/apple2026_skin_audit.py --source-only`
  - result: `OK`
- `.\build-sim.ps1 -Incremental -SkipDep -StrictInstall`
  - result: `OK`

Docs:
- updated `OpusAuditPriorityProgress.md`
- updated `WORK_LOG.md`

Next:
- runtime-gated items remain the highest priority overall:
  1. `#16` quickscreen closure
  2. `#17` lockscreen hold-transition closure
  3. `#18` Music / Files / WPS return-matrix closure
- if runtime work continues to stay deferred, the remaining static cleanup is
  mostly exhausted except for any future follow-on from item `#14`

---

## 2026-04-12 - Update3 follow-up: database browser dense font no longer keys off guessed depth

Goal: continue the Opus pass without relying on the broken simulator capture path,
and keep closing source-confirmable Apple2026 regressions that still survive in
live code.

Changed:
- `apps/tree.c`
  - Apple2026 database-browser font-tier selection no longer uses the coarse
    `tc.dirlevel >= 2` rule.
  - Dense typography now keys off `tagtree_get_attr(&tc) == FILE_ATTR_AUDIO`,
    so database album/category rows stay Normal while actual track lists go
    Dense.

Verified:
- `apps/tree.c`
  - the previous live branch state still used `tc.dirlevel >= 2` for all
    database lists, which was broad enough to collapse some navigational album
    lists into track-style density.
- `apps/tagtree.c`
  - `tagtree_get_attr()` already exposes the reliable source-of-truth the list
    renderer actually needs here:
    - `ATTR_DIRECTORY` for navigational database rows
    - `FILE_ATTR_AUDIO` for playable track rows
- `python tools/apple2026_skin_audit.py --source-only`
  - result: `OK`

Docs:
- `OpusAuditPriorityProgress.md`
  - added this as an extra confirmed Opus follow-up beyond the original ranked
    plan inventory.

Next:
- rebuild hardware targets after the `tree.c` change
- continue re-checking the remaining Opus findings for source-confirmable
  regressions before touching any runtime-only closures

## Apple2026 Now Playing Bar Redesign
**Date:** 2026-04-12
**Type:** Asset + SBS Architecture Update

### 1. Current Implementation Architecture vs Redesign Path
The previous implementation used a rigid, code-driven approach (%dr top border in ottombarsmall viewport). The redesign path shifted to an **asset-driven** approach. Since Rockbox native drawing primitives cannot render anti-aliased soft shadows natively without excessive performance cost, the drop shadow and pill shape were mathematically baked into a 320x54 bitmap (miniplayer_bg.bmp) against the #FAFAF8 background color.

### 2. How the Attached Image Informed Design
The Apple Music mini-player reference heavily informed:
- **Pill Silhouette:** Fully rounded edges within a 304x42 bounds.
- **Drop Shadow:** A gaussian blur (12% opacity, 3.5 radius, +2px Y offset) baked onto the shell color to provide premium lift without muddiness.
- **Internal Margins:** Removed the edge-to-edge feel. The Album Art is centered horizontally within the left edge padding (x=16), texts and icons pushed inward to breathe.

### 3. Decisions and Adjustments
- **Shadow/Pill:** White (#FFFFFF) pill on #FAFAF8 background ensures maximum dynamic range without crashing the white points.
- **Files/Assets Changed:**
    *   Generated wps/Apple2026/miniplayer_bg.bmp via python script (scratch_pill_gen.py).
    *   Updated wps/Apple2026.sbs to load %xd(Z) instead of %dr.
    *   Adjusted main list viewport height from -44 to -54 to allocate space for the shadow bleeding downward.
    *   Re-coordinated all nested viewports (mp_albumart, mp_track, mp_playpause, olumebar_small_noclip) to fit symmetrically within the 54px container at center-Y 211.

### 4. What to verify in the next build
- Check if the shadow depth blends perfectly with the FAFAF8 shell without artifacting.
- Verify album art bounds (32x32) do not clip the top/bottom curve of the 42px pill.
- Ensure the volume bar toggle state correctly overlaps the text bounds within the new pill geometry.



## 2026-04-07 (Apple2026 WPS album art border pop-in fix)

### Issue
- Album art displayed sharp corners briefly before rounded corner mask applied, causing visual flash during cover load

### Root Cause
- Viewport rendering order: album_art viewport rendered first, then album_art_mask overlay applied
- Brief frame where sharp-cornered art visible before mask composited

### Fix Applied
- Reversed viewport definition order: album_art_mask defined before album_art in viewport section
- Reversed display logic order: %Vd(album_art_mask)%Vd(album_art) ensures mask renders first
- Added background color to mask viewport to ensure proper compositing

### Files Touched
- `wps/Apple2026.wps`

### Validation
- Requires simulator/hardware test to confirm no sharp-corner flash during cover load transitions

## 2026-04-13 (Apple2026 WPS Select -> LRC / playlist-viewer regression cleanup)

Changed:
- `apps/settings_list.c`
  - Apple2026 iPod default for `hotkey_wps` is now `HOTKEY_PLUGIN` instead of
    `HOTKEY_VIEW_PLAYLIST`.
  - clarified that `wps_select_action` is the no-context fallback, not the
    primary short-Select action on WPS.
- `apps/open_plugin.c`
  - added helper paths to seed default open-plugin bindings without overriding
    an existing user-selected plugin.
  - added chained plugin-run helper so WPS child plugins can propagate return
    intent cleanly.
- `apps/onplay.c`
  - WPS plugin hotkey now seeds `lrcplayer` as the default target and propagates
    plugin return codes back to WPS.
  - removed the temporary runtime coercion that rewrote an explicit user choice
    of `HOTKEY_VIEW_PLAYLIST` into `HOTKEY_PLUGIN`.
- `apps/gui/wps.c`
  - WPS hotkey/context plugin flows now restore to WPS on plugin error instead
    of treating failure like a hard exit.
  - WPS context-plugin path now seeds `lrcplayer` as the default child plugin
    while preserving an existing user binding.
- `apps/plugins/lrcplayer.c`
  - integrated LRC Player as a WPS child surface:
    - short `Menu` -> WPS
    - long `Menu` -> Main Menu
    - short `Select` remains inert
    - long `Select` -> `LRC Settings`
    - long `Play` -> `Playback Control`
  - playlist/transport semantics now follow WPS-style playback behavior.
- `DESIGN_SYSTEM.md`
  - documented the Apple2026 override: short WPS `Select` defaults to Lyrics /
    LRC via `WPS Hotkey`, playlist viewer stays on `Select+Menu` or an explicit
    user-selected hotkey, and WPS return semantics are driven by
    `playback_context`.

Verified:
- `.\build-sim.ps1 -Incremental`
  - result: `OK`
- `.\tools\verify-sim-install.ps1`
  - result: `OK`
- `python tools\verify_plugin_package.py --build-dir build-sim --package-root build-sim\simdisk\.rockbox`
  - result: `OK`
- static simulator/runtime inspection:
  - current sim profile has no `plugin.dat` / `rb_plugins.dat`
  - `build-sim\simdisk\.rockbox\rocks\apps\lrcplayer.rock` exists
  - WPS hotkey and context-plugin paths now seed `lrcplayer` without
    overriding an explicit user selection
- not yet directly simulator-click-verified:
  - short WPS `Select` on a clean profile opening LRC instead of Playlist
  - visual absence of header / mini-player flicker on that path

Docs:
- updated `DESIGN_SYSTEM.md`
- updated `WORK_LOG.md`

Next:
- run a clean-profile simulator interaction check for:
  - WPS short `Select` -> LRC
  - WPS long `Select` -> context menu
  - `Select+Menu` -> playlist viewer
  - explicit `WPS Hotkey = View Playlist` override still opening Playlist
- if the visual flicker is still reproducible outside the old playlist-viewer
  path, audit WPS restore/theme teardown separately as a second bug.
