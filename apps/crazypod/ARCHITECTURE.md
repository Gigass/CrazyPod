# CrazyPod UI architecture

The UI is organized as stable infrastructure plus vertical feature domains.
Files move only when ownership, state, or dependency direction moves with
them.

## Topology

```text
crazypod/
├── ui/
│   ├── app/             lifecycle and dependency composition
│   ├── navigation/      route stack, registry, query and dispatch
│   ├── shell/           Home/Desktop, status, lock and system prompts
│   ├── presentation/    stateless widgets, glass, menu and motion primitives
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
├── miniapps/            VM, installer, storage and runtime infrastructure
├── epub/                EPUB parsing, extraction and cache infrastructure
└── platform/            audio, display, power and storage adapters
```

There are exactly nine UI feature owners. Wallpaper crop belongs to
Customize. Home/Desktop belongs to Shell. Miniapps is an independent UI
feature while its runtime remains outside `ui/`.

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
- Mutable cross-directory `extern` variables are forbidden.

## Dependency direction

```text
app → shell
app → navigation → feature facade
feature → domain API
feature → presentation
shell → navigation
platform ← app / domain adapters
```

Domain modules do not depend on LVGL, routes, navigation, or features.
Screens render explicit models. Controllers own edit sessions and domain
commands, not application navigation.

## Composition-root contract

`crazypod_ui.c` is complete only when it:

- contains init, tick, input, shutdown, and dependency wiring;
- is 400–800 lines;
- owns no feature-private `lv_obj_t`, overlay state, editor state, preview
  state, or feature work buffer;
- contains no feature route switch;
- does not change when a page is added to an existing feature.

Temporary callback adapters are allowed only while their implementation is
being moved. Duplicated old and new implementations are not allowed.

## File budgets

- Composition root: 800 lines maximum.
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
6. `git diff --check`.

The migration preserves behavior. UI redesign, storage-format migration, and
domain-rule changes are separate work.

## Enforcement

Run `tests/check-crazypod-ui-architecture.sh` after changing UI ownership.
It rejects horizontal transition directories, cross-feature private
includes, any Feature-private include from App, Shell, Presentation or the
composition root, Domain-to-UI includes, mutable UI `extern` variables,
route/input switches or static LVGL page state in the composition root, and
a root outside the 400–800-line budget.

`crazypod_ui.c` is now the composition and lifecycle root. Feature route
rendering, activation, input, previews, runtime services, Now Playing,
system prompts, deferred rendering, menu presentation, and artwork widgets
are owned by their declared modules.
