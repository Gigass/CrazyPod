# CrazyPod UI architecture

The UI is organized as stable infrastructure plus vertical feature domains.
Files move only when ownership, state, or dependency direction moves with
them.

This document reflects the source and architecture gate on 2026-07-30.

## Topology

```text
crazypod/
├── ui/
│   ├── app/             lifecycle and dependency composition
│   ├── navigation/      route stack, registry, query and dispatch
│   ├── shell/           Home/Desktop, status, lock and system prompts
│   ├── presentation/    stateless widgets, glass, menu, HUD and motion
│   └── features/
│       ├── music/
│       ├── now_playing/
│       ├── books/
│       ├── notes/
│       ├── photos/
│       ├── organizer/
│       ├── customize/
│       ├── settings/
│       └── miniapps/
├── miniapps/            native loader, installer, storage and runtime
├── gameboy/             Rockboy core adapter, ROM catalog and battery saves
├── epub/                EPUB parsing, extraction and cache infrastructure
├── photos/              photo catalog, cache and viewport infrastructure
├── video/               video catalog and poster infrastructure
├── wallpaper/           wallpaper cache, crop and storage infrastructure
├── platform/            display and firmware-facing adapters
└── crazypod_l10n.*      generated-table lookup and current language
```

There are exactly nine UI feature owners. Wallpaper crop belongs to
Customize. Home/Desktop belongs to Shell. Miniapps is an independent UI
feature while its runtime remains outside `ui/`.
The GB/GBC library and native game screen belong to the Miniapps UI feature;
the emulator adapter and storage remain in `gameboy/` without UI dependencies.

Global `controllers/`, `screens/`, `preview/`, `routes/`, `material/`,
`menu/`, `media/`, `features/home/`, and `features/wallpaper/` directories
are forbidden. They were transitional horizontal buckets and have been
removed.

## Public boundaries

- Each feature exposes one `crazypod_<name>_feature.h` facade.
- Other headers in a feature directory are private implementation headers.
- Navigation resolves every route to exactly one feature or Shell.
- Route count, title, item title, current selection, activation, rendering,
  and input are dispatched through the active feature.
- Features request push, pop, and render through navigation commands.
- Feature code may use public domain APIs and Presentation primitives.
- Features must not include another feature's private header.
- Shell must not depend on a concrete feature.
- Presentation must not read route-specific or persistent business state.
- Platform must not contain routes or LVGL page logic.
- Localization owns language selection and string lookup, not page state or
  feature behavior.
- Mutable cross-directory `extern` variables are forbidden.

## Dependency direction

```text
app → shell
app → navigation → feature facade
feature → domain API
feature → presentation
feature / shell → localization
shell → navigation
platform ← app / domain adapters
```

Domain modules do not depend on LVGL, routes, navigation, or features.
Screens render explicit models. Controllers own edit sessions and domain
commands, not application navigation.

## Composition-root contract

`crazypod_ui.c` is complete only when it:

- contains init, tick, input, shutdown, and dependency wiring;
- is 400–1500 lines;
- owns no feature-private `lv_obj_t`, overlay state, editor state, preview
  state, or feature work buffer;
- contains no feature route switch;
- does not change when a page is added to an existing feature.

Temporary callback adapters are allowed only while their implementation is
being moved. Duplicated old and new implementations are not allowed.

## File budgets

- Composition root: 1500 lines maximum.
- Feature facade/coordinator: 800 lines maximum.
- Screen/controller: 600 lines maximum.
- Ordinary feature: 2–5 primary `.c` files.
- Complex features may exceed five only when each extra file owns a real
  screen, state machine, or workflow.
- Every directory has one public facade header; internal headers are not
  cross-directory APIs.

Budgets expose boundary failures; they are not a reason to split cohesive
logic into forwarding files.

## Required gates

Every migration stage must pass:

1. Simulator build.
2. iPod 6G hardware build with zero warnings.
3. UI host tests.
4. Miniapp host tests.
5. EPUB host tests.
6. Strict localization audit.
7. `git diff --check`.

The migration preserves behavior. UI redesign, storage-format migration, and
domain-rule changes are separate work.

## Display presentation contract

Home and Music CoverFlow mix native RGB565 drawing with LVGL partial rendering.
Their moving regions use geometry-aware hardware TE synchronization and must
remain separate from ordinary LVGL dirty rectangles. While the Home wheel is
touched, the present scheduler defers every non-full ordinary partial update
until release and continues to submit only TE-synchronized Home frames.

Do not replace this contract with an FPS cap, animation-speed limit, fixed
delay, or inferred wheel-idle timeout. Read
[the LCD tearing maintenance guide](../../docs/CRAZYPOD_LCD_TEARING.zh-CN.md)
before changing the display adapter, frame clock, click-wheel touch state,
Home native renderer, Now Playing capsule, or Music CoverFlow renderer.

## Enforcement

Run `tests/check-crazypod-ui-architecture.sh` after changing UI ownership.
It rejects horizontal transition directories, cross-feature private
includes, any Feature-private include from App, Shell, Presentation or the
composition root, Domain-to-UI includes, mutable UI `extern` variables,
route/input switches or static LVGL page state in the composition root, and
a root outside the 400–1500-line budget.

`crazypod_ui.c` is now the composition and lifecycle root. Feature route
rendering, activation, input, previews, runtime services, Now Playing,
system prompts, deferred rendering, menu presentation, and artwork widgets
are owned by their declared modules.

Run the complete local gate with:

```sh
sh tests/check-crazypod-ui-architecture.sh
sh tests/run-crazypod-ui-host-tests.sh
sh tests/run-miniapp-host-tests.sh
sh tests/run-epub-host-tests.sh
python3 tools/check-crazypod-l10n.py --strict-bare
git diff --check
```
