# CrazyPod Mini Apps

CrazyPod Mini Apps are native payloads for the iPod 6G. The current SDK and
loader use ABI 1 and support only the `ipod6g` hardware target and the
simulator target.

This reference reflects SDK revision 3 and package format 2 as of 2026-07-30.

The two reference packages are:

- `dist/miniapps/calculator-1.2.0.cpk`
- `dist/miniapps/pomodoro-1.2.0.cpk`

For a complete Chinese walkthrough that creates, builds, signs, tests, and
installs a new app, read
[从零开发 CrazyPod Mini App](TUTORIAL.zh-CN.md).

Copy a package to `/MiniApps/Install` on the iPod. CrazyPod verifies and
installs it when Mini Apps opens. Firmware builds also bundle both reference
packages under `/.rockbox/crazypod/miniapps/packages`.

## Reference apps

### Calculator

Calculator opens with a `0` display and row-major focus on the first key. The
wheel, Left, and Right move one key at a time; Select activates the focused
key; Play activates equals; Menu returns to the Mini Apps list.

The engine supports standard four-function chaining, contextual percent,
repeat equals, sign, decimal entry, backspace, clear/all-clear, divide-by-zero
handling, range errors, and recovery after an error. Error text follows the
current CrazyPod language.

### Pomodoro

Pomodoro defaults to:

| Setting | Default |
| --- | --- |
| Focus | 25 minutes |
| Short break | 5 minutes |
| Long break | 15 minutes |
| Rounds | 4 |

Ready sessions expose Start and Setup. Running and paused sessions expose
pause/resume, skip, and reset as appropriate. The current phase, remaining
time, configuration, deadline, and alarm token are persisted.

In Setup, wheel movement selects one row at a time. Select enters or leaves
numeric editing, where wheel acceleration may change a value by multiple
steps. Play saves the configuration. Menu leaves editing first, then closes
Setup. Labels, phases, actions, and status messages follow the current
CrazyPod language.

## Installation lifecycle

On opening Mini Apps, CrazyPod scans the bundled system packages and
`/MiniApps/Install`. A package is installed only after its ZIP structure,
manifest, signature, hashes, icon, target, ABI, native header, and available
space pass validation.

- An identical installed version is left unchanged.
- A damaged installation is repaired from a valid package of the same version.
- A higher installed `version_code` is not replaced by an older package.
- Installation is staged and committed under `/.crazypod/miniapps`.
- Per-app state and alarms live under `/.crazypod/miniapp-data`.

Successful package verification records an in-memory trust entry for the
installed app and version. Normal list entry and app launch do not repeat
Ed25519 verification or hash the 160×160 icon. Trust is rebuilt at firmware
startup, after package installation/rescan, and after returning from USB data
mode. An installed app without a matching scanned package is fully verified
on its first launch and then cached for the rest of that session.

## Package format

A format 2 `.cpk` is a flat, uncompressed ZIP archive with exactly five
entries:

```text
manifest.ini
app.arm
icon.bmp
signature.ed25519
resources.bin
```

Legacy format 1 packages remain supported and contain exactly the first four
entries. Simulator packages replace `app.arm` with `app.dylib`. The loader rejects
directories, extra entries, compression, data descriptors, comments, ZIP64,
duplicate names, and inconsistent local or central records.

`manifest.ini` contains one UTF-8 `key=value` field per line:

```ini
format=2
id=calculator
name=Calculator
version=1.2.0
version_code=102000
abi=1
target=ipod6g
binary=app.arm
icon=icon.bmp
resources=resources.bin
symbol=+
accent=26CFF5
summary=Standard click-wheel calculator
binary_sha256=<64 lowercase hexadecimal characters>
icon_sha256=<64 lowercase hexadecimal characters>
resources_sha256=<64 lowercase hexadecimal characters>
```

The 64-byte Ed25519 signature covers the exact bytes of `manifest.ini`.
The manifest hashes cover the payload, icon, and resource container. The
loader also validates the
payload header, target, ABI, API sizes, entry point, load ranges, icon format,
CRC values, and installed-file record before execution.

The icon is exactly 160×160 pixels, 32-bit BI_RGB BMP. The package builder
generates it.

`resources.bin` is a deterministic, indexed container. It is limited to
512 KiB, 32 sorted resource IDs, and 128 KiB per resource. The first
implementation supports opaque blobs and raw little-endian RGB565 bitmaps up
to 160×160. Put source resources under `miniapps/<id>/resources`; bitmap files
use `<id>.<width>x<height>.rgb565`. Apps read resources through
`resource_stat()` and `resource_read()`, never through a firmware path.

## Runtime contract

`sdk/crazypod_miniapp.h` defines the ABI. A Mini App returns semantic drawing
commands instead of creating LVGL objects directly. CrazyPod resolves its
color and font tokens through the current appearance.

Input is normalized as:

- wheel clockwise or counter-clockwise, with 1–4 accelerated steps;
- Left and Right, one exact step;
- Select, Play, and Menu.

Wheel direction selects exactly one adjacent item for discrete focus
navigation. Mini Apps may use the accelerated `steps` magnitude only while
editing a continuous or numeric value. This prevents fast wheel movement from
skipping actionable controls.

The current host places wheel events in a bounded eight-event queue and
consumes at most one event when a display frame can be rendered and presented.
Changing direction discards pending events from the old direction. Select,
Play, Menu, Left, or Right clears wheel movement that has not yet appeared on
screen, so an action cannot target an invisible future focus state.

An app exports `cp_miniapp_entry`, returns a `cp_miniapp_ops` table, and
implements `open`, `close`, `event`, `tick`, and `render`. Rendering is limited
to at most 64 semantic rectangle, text, ring, divider, progress, or bitmap
commands.
Apps select appearance-aware color and font tokens rather than creating LVGL
objects or using firmware UI internals directly.

The host API provides clock access, atomic per-app state, four persistent alarm
slots, alarm acknowledgement, system status including the current language,
date/time/duration formatting, short host-rendered toasts, and number
formatting. The host persists UI delivery separately from the current alarm,
so acknowledging an expired timer cannot consume an alert before the shell
receives it, and a new timer can start while the prior alert is pending. The
shell clears the delivery record only after starting the first alert sound. It
does not expose direct audio control, so a Pomodoro alarm can coexist with
music playback.

ABI 1 revision 3 keeps the original host table as an immutable prefix. The
loader accepts older ABI 1 packages whose required host-table size is no larger
than the current table. New apps must check `struct_size`, `capabilities`, and
the function pointer with `CP_HOST_HAS()` before using any optional tail
function.
The current capability bits cover:

- battery, charging, USB, playback, time-validity, and Reduce Motion status;
- duration and date/time formatting without libc;
- four independent alarm slots per app;
- host toasts and an asynchronous request to close the current app;
- semantic divider and linear-progress drawing commands;
- asynchronous host-owned text, choice, and confirmation surfaces;
- bounded resource lookup/read and one RGB565 bitmap command per scene;
- a copied, read-only Now Playing snapshot.

`cp_system_info.language` uses `enum cp_language`. Calculator and Pomodoro use
the generated `sdk/crazypod_miniapp_l10n.h` table, which currently contains 27
strings across the same nine languages as the firmware. Third-party packages
own their translations; the host does not translate arbitrary app text.

Modal UI requests are asynchronous. An app submits a nonzero request ID, then
calls `ui_poll_result()` from `tick()` or `event()`. Only one request or
unconsumed result may exist per app. The text surface is intentionally a basic
printable-ASCII editor, not a Chinese or multilingual IME.

`CP_DRAW_DIVIDER`, `CP_DRAW_PROGRESS`, and `CP_DRAW_BITMAP` reuse the existing
fixed-size drawing command structure, so they do not change the ABI 1 scene
layout. `CP_DRAW_BITMAP.text` contains the resource ID. The current renderer
accepts one bitmap command per scene to keep memory bounded.

## Proposed, not implemented

A mediated file picker and atomic per-app export area remain deferred. Direct
filesystem access, playback control, PCM/audio callbacks, background threads,
networking, sensors, and a multilingual IME are not exposed.

## Security boundary

Signature verification authenticates package origin and detects modification.
It does not isolate native code. A loaded Mini App executes with firmware
privileges and can compromise the device if the trusted signer ships malicious
or vulnerable code.

`keys/development_ed25519.pem` is intentionally committed for reproducible
development builds. Its private half is public and therefore cannot establish
production trust. Production firmware must embed a different public key, and
the matching private key must remain outside this repository.

## Build

Build the payloads and signed hardware packages through the normal firmware
script:

```sh
./build-hw.sh
```

For an existing hardware build:

```sh
make -C build-hw-ipod6g miniapps
python3 tools/build-miniapp-packages.py \
  --build-dir build-hw-ipod6g \
  --target ipod6g \
  --binary app.arm \
  --output dist/miniapps
```

Packaging requires OpenSSL 3 for Ed25519 signing. Set
`CRAZYPOD_OPENSSL=/path/to/openssl` when it is not on `PATH`.

The normal hardware build also verifies 8-byte alignment of all iPod 6G main,
IRQ, and FIQ stack boundaries before packaging. This is part of the Mini App
runtime contract because the host number formatter passes `double` through a
variadic firmware function.

Run the Mini App host suite with:

```sh
sh tests/run-miniapp-host-tests.sh
```

The suite covers manifests, CPK structure, resource access, alarm delivery,
install records, same-version repair, and installer lifecycle behavior.
