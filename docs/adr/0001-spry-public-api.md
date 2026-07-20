# ADR 0001 — Spry public API & module design

- **Status:** Accepted
- **Date:** 2026-07-20
- **Ticket:** #207 (part of epic #205 — replace ImGui with an open-sourceable toolkit)
- **Supersedes / superseded by:** —

> This is the first Spry ADR. ADRs live in `libs/spry/docs/adr/` (the root of the standalone
> Spry repo once split), numbered `NNNN-kebab-title.md`, and record a single architectural
> decision with its context and consequences. Keep them
> short and append-only: to revise a decision, add a new ADR that supersedes the old one
> rather than rewriting history.

## Context

Spry (`libs/spry/`) is the retained-mode, animation-first UI toolkit that now renders all of
Cleat (ImGui was removed in epic #222). Epic #205 intends Spry to be **extracted to its own
permissively-licensed public repo** (#224) and used by projects that are not Cleat. Before we
publish it we need to decide, and write down, what its **public API** is: which types and
headers external consumers may depend on, how the pieces fit together, and what the stability
contract is.

The toolkit already has a working header set (`libs/spry/include/spry/`) built incrementally
across #208–#222. This ADR does not redesign it; it **curates the existing surface into a
documented, general-purpose public API** and marks the parts that are internal, experimental,
or Cleat-shaped. The acceptance test for the design is a minimal example that compiles against
only the public headers — see `libs/spry/examples/hello.cpp` (`spry_hello` target).

Constraints that shaped the design (the **decoupling contract**, from `libs/spry/README.md`):

- Spry depends only on **SDL3 + FreeType + HarfBuzz**, never on Cleat. No Cleat headers appear
  under `libs/spry/`.
- The host owns the window, the GPU/renderer, the event loop, the clipboard, and platform text
  input. Spry is fed already-translated input and draws through an abstract renderer.

## Decision

### The public surface is `<spry/spry.h>` plus one backend header

Everything a general consumer needs is reachable through the umbrella header `<spry/spry.h>`,
which aggregates the public headers **except** the concrete renderer backends. A consumer also
includes exactly one backend header for the renderer they use:

```cpp
#include <spry/spry.h>          // Context, Widget, Box, built-in widgets, Theme, anim, input
#include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h> — pick one backend
using namespace spry;           // everything lives in namespace spry::
```

The API is organized into the following modules. Each is stable public surface unless flagged
**[internal]** or **[experimental]** below.

#### 1. App / Context + frame loop — `context.h`, `input.h`

`spry::Context` owns the root widget, the active theme, the overlay stack, and interaction
state. The host drives it with two calls:

- `void handleEvent(const InputEvent&)` — feed one translated platform event (mouse/key/text/wheel).
- `void frame(Renderer&, float dt, float mouseX, float mouseY)` — run one frame: theme
  interpolation → layout (`arrange`) → hover hit-test → `update(dt)` → `draw`.

Plus `setRoot(unique_ptr<Widget>)`, `setTheme` / `setThemeImmediate` (animated vs instant
theme swap), `setFocus` / `focused`, `cursor()` (the platform cursor to show this frame), and
host-integration hooks `setTextInputHandler(...)` and `addOverlay(...)`.

Input is Spry-native: `InputEvent` (a tagged struct of mouse/key/text/wheel) and the `Key`
enum. The host translates its platform events into these — **Spry ships no built-in
SDL→InputEvent pump** (see Consequences).

#### 2. Widget base + tree — `widget.h`, `box.h`

`spry::Widget` is the retained base: a persistent object with layout hints (`prefW/prefH`,
`grow`, `margin`), a computed `rect`, interaction flags, a `tooltip`, and virtual lifecycle
hooks (`measure` / `arrange` / `update` / `draw` / `paint`) and input hooks (`onClick`,
`onKey`, `onWheel`, …). Compose trees with `emplace<T>(args…)` (construct child in place,
return raw pointer) or `add(unique_ptr)`. Layout containers: `Box` (flex row/column with
`spacing`, `padding`, `cross` alignment) and `WrapBox` (flow-wrap).

#### 3. Layout primitives — `layout.h`

`Size`, `Rect` (with `contains`), `Edges` (margins/padding), and the `Axis` / `Align` enums.
Value types shared by every module.

#### 4. Built-in widgets — `widgets.h`, `textfield.h`, `textarea.h`, `text_edit.h`, `combo.h`, `colorpicker.h`, `overlay.h`, `data.h`

- **Basics** (`widgets.h`): `Panel`, `Label`, `Paragraph`, `Button`, `Checkbox`, `RadioButton`,
  `Toggle`, `Slider`, `ProgressBar`, `Image`, `Card`.
- **Text editing**: `TextField` (single-line), `TextArea` (multi-line), both backed by the
  headless, unit-tested `EditBuffer` (`text_edit.h`) which a consumer can also use directly.
- **Selection**: `Combo` (dropdown), `ColorPickerPad` / `ColorPicker` / `ColorField`.
- **Overlays** (`overlay.h`): `Overlay` base + `Modal`, `Menu` / `MenuItem`, `Tooltip`, `Toast`.
  Overlays are pushed onto the `Context` and manage their own open/close animation.
- **Data / containers** (`data.h`): `VirtualList` base + `ListView`, `Table`, `TreeView`,
  `ScrollView`, `TabBar`.

#### 5. Theme — `theme.h`, `color.h`

`spry::Theme` is a data-driven token bag: `colors` (name → `Color`) and `metrics` (name →
`float`), read by role via `color(key, fallback)` / `metric(key, fallback)`. `builtinDark()`
always works without a file; `loadFromFile()` parses a minimal flat format (no JSON
dependency); `lerp(a, b, t)` interpolates per-token for animated transitions. `Color` is a
plain RGBA8 struct with `hsv` / `toHsv` / `lerp` helpers.

**Theme token vocabulary (de-facto public contract).** Widgets read these token names; a theme
should define them. Colors: `background`, `surface`, `surfaceAlt`, `accent`, `accentText`,
`text`, `textDim`, `scrim`. Metric: `radius`. A theme missing a token gets the widget's
fallback. Formalizing this list into a documented registry is tracked as follow-up.

#### 6. Animation — `anim.h`

The differentiator. `easeOutCubic` / `easeOutBack` easings and `Spring` (a sub-stepped damped
spring: set `target`, call `step(dt)`, `kick(v)` for a pulse). Widgets own their own animation
state — there are no global side-tables.

#### 7. Renderer backend interface — `renderer.h` (+ `sdl_renderer.h`, `gl_renderer.h`)

`spry::Renderer` is the abstract 2D drawing interface the widgets emit to: `beginFrame` /
`endFrame` / `outputSize`, `fillRect` / `fillRoundedRect` / `fillMesh` / `text` / `measureText`,
optional `loadImage` / `drawImage`, and base-provided clip and opacity stacks (backends
implement only `applyClip`). Supporting types: `Color`, `Vertex`, `Mesh`, `ImageHandle` (opaque),
and the monospace text-metric helpers (`kTextBasePx`, `textCellW`, `textLineH`).

Two concrete backends ship; a consumer **picks** one (they rarely implement `Renderer` from
scratch):

- **`SdlRenderer`** — wraps a host-owned `SDL_Renderer*`. The simple, standalone path. Text via
  FreeType behind a pimpl.
- **`GlRenderer`** — an OpenGL 3.x backend for embedding into a host that already owns a GL
  context. It renders into its own FBO and blits, which is how it composited with (now-removed)
  ImGui in Cleat.

### What is public vs not

| Marker | Meaning |
|---|---|
| **Public / stable** | Everything in modules 1–7 above, reached via `<spry/spry.h>` + one backend header. Changes follow the stability contract. |
| **[experimental]** | The accessibility surface — `Role`, `Widget::accessibleRole/accessibleLabel`, `Context::visitAccessible`. The tree is walked but there is **no platform a11y backend yet**; shapes may change when one lands. |
| **[Cleat-shaped, but generic]** | `data.h` (framed as "the chrome Cleat's views need"), `ColorField` (built for Cleat's `#RRGGBB` settings), and `GlRenderer`'s FBO/compositing extras (`targetTexture`, `blitToDefault`, `presentBlended`). The behavior is general; only the framing is Cleat-specific. General consumers can use them but shouldn't expect Cleat-tailored ergonomics. |
| **[internal]** | Anything under `private:` / `protected:`, the `src/*.cpp` implementations, and pimpl structs. Not part of the API. |

### Stability contract

- **Small and curated.** New widgets are additive; the core loop (`Context` / `Widget` /
  `Renderer` / `Theme` / `InputEvent`) stays minimal.
- **Semver once extracted.** After the public-repo split (#224), breaking changes to public
  types bump the major version. Until the split, the surface may still shift as #207 follow-ups
  land (notably the theme-token registry; the SDL input-pump helper landed as `sdl_host.h`, #320).
- **The umbrella header is the contract.** If it's not reachable from `<spry/spry.h>` (or a
  backend header), it isn't public.

### Building against Spry

Spry is a CMake `STATIC` library target named `spry`, with `PUBLIC` include dir `include/` and
`PUBLIC` link deps `SDL3::SDL3 freetype harfbuzz harfbuzz-subset OpenGL::GL` (they propagate to
consumers). SDL3/FreeType/HarfBuzz are `FetchContent`-declared only if the parent hasn't already
provided those targets, so a host that supplies them is reused; standalone builds fetch pinned
versions.

```cmake
add_subdirectory(libs/spry)          # or otherwise make the `spry` target available
target_link_libraries(myapp PRIVATE spry)
```

## Consequences

**Positive**

- A consumer can stand up a themed, animated, interactive window in ~90 lines against only
  public headers (`examples/hello.cpp`), with no Cleat dependency.
- The host/toolkit boundary is explicit: window, renderer, event pump, clipboard, and text-input
  are the host's; layout, widgets, theming, and animation are Spry's.
- Two renderer backends cover both "standalone app" (SDL) and "embed in a GL host" (GL) without
  the widgets knowing the difference.

**Negative / follow-ups**

- **SDL event pump.** The core stays platform-agnostic (it consumes `InputEvent`), so an SDL
  host still translates events — but the optional `<spry/sdl_host.h>` header (#320) now packages
  the standard wiring (`toKey`, `pumpEvent`, clipboard + IME handlers), so simple apps don't copy
  boilerplate. It's opt-in and outside the umbrella header (it pulls in SDL3), preserving the
  decoupling contract. A non-SDL host translates events itself.
- **`sdl_renderer.h` pulls in `<SDL3/SDL.h>`**, unlike the pimpl-clean `gl_renderer.h`. Consumers
  who include the SDL backend get SDL in that translation unit.
- **Theme token names are a stringly-typed de-facto contract** scattered across widgets. Until a
  documented registry (or enum) exists, a theme author discovers tokens by reading this ADR.
- **Accessibility is a placeholder.** The a11y tree is exposed but nothing consumes it yet.

## Alternatives considered

- **Immediate-mode API (ImGui-style).** Rejected — the whole point of #205 is a *retained*,
  animation-first tree; immediate mode fights per-widget animation state and accessibility.
- **One "does-everything" renderer instead of an abstract interface.** Rejected — the
  `Renderer` seam is what lets the same widgets run under SDL standalone and inside a GL host,
  and lets a future Vulkan/Metal/`SDL_gpu` backend drop in.
- **Header-only library.** Rejected — text (FreeType/HarfBuzz), the renderer backends, and the
  edit buffer have real `.cpp` implementations; a static lib keeps compile times sane and hides
  FreeType/SDL behind pimpls.
- **JSON theme format.** Rejected for the core — a flat `color`/`metric` text format keeps the
  decoupling contract (no JSON dependency); a host may still build a `Theme` programmatically
  from its own config.
