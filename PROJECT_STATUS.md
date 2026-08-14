# CrazyPod project status

Status date: 2026-07-31 CST

CrazyPod targets only iPod Classic 6G (`ipod6g`): ARMv5, 64 MiB RAM and a
320×240 RGB565 display. Rockbox supplies the kernel, drivers, codecs, storage,
USB, power and playback. CrazyPod supplies the LVGL 9.5 product UI.

## Current Mini App architecture

```text
React-style TypeScript/TSX
  → strict React Profile v1 AOT
  → deterministic C
  → app.arm / app.dylib
  → CPK5
  → Native ABI 1.1
  → host-owned LVGL
```

Device runtime contains no JavaScript engine, React, Solid, Virtual DOM,
`app.js`, bytecode cache or serialized UI command interpreter. CPK4/ABI3 and
older packages are rejected; there is no fallback runtime.

Implemented:

- versioned Native Host/UI/App Ops ABI;
- CPK5 reader, verifier, staging and atomic installation;
- same generated C for simulator and hardware;
- React Native-style imports, `useState`, static StyleSheet/Flexbox, supported
  components, absolute positioning, wrap/overflow, animated images,
  conditional JSX, UI events and Click Wheel input;
- retained scene handles and dynamic-property updates;
- native resources, state persistence, log and request-close services;
- Native Reference, Capability Lab and 2048;
- deterministic builder, CLI and host/simulator regression suites.

Accurate limitations:

- React Profile v1 is not arbitrary TypeScript-to-C;
- it currently lacks custom components, object/array state, list diff,
  effects, context, async and arbitrary npm libraries;
- 2048 uses a build-time platform intrinsic for its domain logic;
- Native ABI 1 does not yet expose the old file picker, player, PCM, effect,
  device-setting or alarm APIs;
- package signing and permission UI are intentionally not implemented.

## Current validation

Completed in this working tree:

- builder and CLI tests;
- strict TypeScript checking;
- generated C with `-Wall -Wextra -Werror`;
- Native ABI, CPK5, installer, state, resource, input and 2048 engine host
  tests;
- simulator build and native dylib loading;
- CPK5 install with explicit absence of JS artifacts;
- Native Reference event/rerender test;
- 2048 → exit → Capability Lab reproduction loop with latency capture;
- UI architecture, UI host, EPUB, font, strict localization and
  `git diff --check` gates;
- current iPod 6G ARM build with zero warnings;
- CPK5 hardware packages containing only manifest, `app.arm`, profile,
  assets and icon;
- firmware ELF and all three native payloads inspected for removed-runtime
  symbols and markers;
- the earlier ABI 1.0 release was copied to the validated iPod 6G volume with
  an external backup, firmware-last atomic installation and source/device
  SHA-256 comparison, then safely ejected;
- separate production (`build-hw-ipod6g/`) and one-shot reproduction
  (`build-hw-ipod6g-repro/`) ABI 1.1 clean builds complete with zero compiler
  warnings, including cleanup of six dead upstream codec variables exposed by
  GCC 16; the reproduction symbols are absent from the production ELF;
- production firmware SHA-256 is
  `8a491ed4a59cfeb4c26d5e02fe70a302ee06b349f2a0b7f4cf18af70d5653b50`;
  reproduction firmware SHA-256 is
  `afab895b04f9ca307d6c3ec61a6845eb639394325dafd1a27a9c5ece263fcca9`.

The retained-handle compiler change reduced the 2048 movement regression from
p95 360ms / max 390ms to aggregate p95 60ms / worst-cycle p95 70ms / max
70ms in the final five-cycle simulator run. Continuous-interaction heartbeat
max was 30ms, button queue max was 1 and Mini App queue max was 0. The
all-phase heartbeat max was 280ms; cold loading is recorded separately and is
not mixed into the continuous-interaction gate. No-op 2048 moves are settled
on their present frame but counted separately from visibly changed frames, so
they cannot shift later latency samples.

Still required before calling the firmware physically certified:

- installing the final ABI 1.1 firmware and packages (the installed ABI 1.0
  build is superseded and is not final evidence);
- booting the freshly installed firmware;
- the full five-cycle 2048/Lab test on the connected iPod;
- USB, playback coexistence, restart persistence and long-duration device
  regression.

Simulator success and ARM compilation do not prove physical-device timing.

## Seven-phase completion audit

| Phase | Result | Authoritative evidence |
| --- | --- | --- |
| 1. Baseline and reproduction | PASS | Five-cycle harness records button queue, present sequence, framebuffer CRC, latency and heartbeat; the pre-refactor 360/390ms regression is preserved in this status. |
| 2. Native ABI, CPK5 and lifecycle | PASS | ABI/manifest/reader/verifier/installer/storage/resource/runtime/lifecycle host tests pass; packages declare CPK5 and ABI 1.1. |
| 3. React Profile AOT | PASS | Builder 22/22, strict TypeScript, deterministic C/CPK and generated C compiled with `-Wall -Wextra -Werror`. |
| 4. Layout, components, events and animation | PASS | The 12-scene framebuffer matrix covers controls, wrap/absolute layout, animated assets, lifecycle and a real modal overlay; unsupported incomplete components are compile errors. |
| 5. Capability Lab | PASS | CPK5 simulator install/load and all Lab directory/controls/assets/lifecycle/modal scenes pass. |
| 6. Native 2048 | PASS | Generated-C engine host tests and five cycles × 32 real button-queue moves pass with per-cycle latency gates. |
| 7. Legacy removal and release | PARTIAL | Legacy runtime files/symbols are absent, all local gates and separate zero-warning production/reproduction ARM builds pass. Final ABI 1.1 device installation and physical tests remain mandatory. |

## Contracts

- [Power management](docs/CRAZYPOD_POWER_MANAGEMENT.zh-CN.md)
- [Native AOT architecture](docs/CRAZYPOD_MINIAPP_NATIVE_AOT_ARCHITECTURE.zh-CN.md)
- [Native AOT development](docs/CRAZYPOD_MINIAPP_NATIVE_AOT_DEVELOPMENT.zh-CN.md)
- [CPK5 format](docs/CRAZYPOD_MINIAPP_CPK5_FORMAT.zh-CN.md)
- [Native AOT verification](docs/CRAZYPOD_MINIAPP_NATIVE_AOT_VERIFICATION.zh-CN.md)
