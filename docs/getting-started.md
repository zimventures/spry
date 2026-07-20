# Getting started with Spry

A hands-on tutorial for the Spry UI toolkit. It builds up from a blank window to an
interactive, themed, animated app, teaching the public API one piece at a time.

- For the **reference** view of the API — every module, what's stable vs experimental, the
  stability contract — see [`docs/adr/0001-spry-public-api.md`](adr/0001-spry-public-api.md).
- For runnable code, see [`examples/`](../examples/) (indexed in
  [`examples/README.md`](../examples/README.md)). This guide references those files rather than
  repeating them.

Spry is a **retained-mode** toolkit: you build a persistent tree of `Widget` objects once, then
drive it each frame. The host owns the window, the GPU, and the event loop; Spry owns layout,
drawing, theming, and animation. Everything lives in `namespace spry`.

---

## 1. Add Spry to your build

Spry is a CMake target named `spry`:

```cmake
add_subdirectory(libs/spry)            # or vendor it however you like
target_link_libraries(myapp PRIVATE spry)
```

Its include dir and its dependencies (SDL3, FreeType, HarfBuzz, OpenGL) propagate to you
automatically. If your project already provides `SDL3::SDL3` / `freetype` / `harfbuzz` targets,
Spry reuses them; otherwise it fetches pinned versions.

Include the umbrella header plus **one** renderer backend:

```cpp
#include <spry/spry.h>          // Context, Widget, Box, widgets, Theme, anim, input
#include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h>
using namespace spry;
```

Pick `SdlRenderer` for a simple standalone app (it wraps an `SDL_Renderer`), or `GlRenderer` to
embed in a host that already owns an OpenGL context.

---

## 2. The smallest app

The full minimal program is [`examples/hello.cpp`](../examples/hello.cpp) (~90 lines). Its shape:

```cpp
SdlRenderer ren(sdl);          // wrap a host-owned SDL_Renderer*
ren.loadFont("…/DejaVuSansMono.ttf");

Context ctx;
ctx.setRoot(buildTree());      // a std::unique_ptr<Widget>
ctx.setThemeImmediate(Theme::builtinDark());

while (running) {
    // 1. feed input
    while (SDL_PollEvent(&e)) ctx.handleEvent(translate(e));
    // 2. draw one frame
    ren.beginFrame(ctx.displayedTheme().color("background"));
    ctx.frame(ren, dt, mouseX, mouseY);   // layout → hover → update → draw
    ren.endFrame();
}
```

The two calls that matter are **`handleEvent`** (feed one translated platform event) and
**`frame`** (run one frame). Everything else is host boilerplate.

---

## 3. Building a widget tree

Widgets are plain objects you compose into a tree. `emplace<T>(args…)` constructs a child in
place and hands back a raw pointer so you can keep configuring it:

```cpp
auto root = std::make_unique<Box>();   // Box = flex container
root->axis = Axis::Column;
root->padding = Edges(24);
root->spacing = 16;

root->emplace<Label>("Hello, Spry", 2.4f);

auto* row = root->emplace<Box>();
row->axis = Axis::Row;
row->spacing = 12;
row->emplace<Button>("OK", [] { /* onClick */ });
row->emplace<Button>("Cancel", [] {});
```

Layout is flexbox-like. Each widget carries hints the parent `Box` reads:

- `prefW` / `prefH` — preferred size (`-1` = auto / content-sized).
- `grow` — flex weight along the parent's main axis (a `grow=1` child eats leftover space).
- `margin`, and on `Box`: `padding`, `spacing`, `cross` (`Align::Start/Center/End/Stretch`).

Use `WrapBox` instead of `Box` for left-to-right flow that wraps. Return the root as a
`std::unique_ptr<Widget>` and hand it to `ctx.setRoot(...)`.

---

## 4. Built-in widgets

All from `<spry/spry.h>`:

- **Text & surfaces:** `Label`, `Paragraph` (word-wrap), `Panel`, `Card`, `ProgressBar`, `Image`.
- **Controls:** `Button`, `Checkbox`, `RadioButton`, `Toggle`, `Slider`, `Combo`.
- **Text input:** `TextField` (single line), `TextArea` (multi-line). Both take `onChange`
  callbacks; `TextField` also has `onSubmit`. The headless `EditBuffer` behind them is usable
  on its own if you need an editing model without a widget.
- **Color:** `ColorField` (a swatch that opens a picker), `ColorPickerPad`.
- **Data containers:** `ListView`, `Table` (sortable columns), `TreeView`, `ScrollView`,
  `TabBar` — all virtualized where it matters.

Most controls follow the same pattern: public fields for state, a `std::function` callback for
changes. For example:

```cpp
auto* slider = box->emplace<Slider>(0.0f, 100.0f, 50.0f);
slider->onChange = [](float v) { /* … */ };

auto* toggle = box->emplace<Toggle>();
toggle->label = "Dark mode";
toggle->onChange = [](bool on) { /* … */ };
```

To update a widget, mutate its public fields (e.g. `label->text = "…"`) — the next `frame()`
re-measures and redraws. See [`examples/gl_demo.cpp`](../examples/gl_demo.cpp) for a gallery
that exercises nearly every widget.

---

## 5. Theming

A `Theme` is a bag of named tokens — colors and float metrics — that widgets read by role:

```cpp
Theme t = Theme::builtinDark();          // always works, no file needed
Theme::loadFromFile("midnight.theme", t); // optional overrides (flat text format)
ctx.setThemeImmediate(t);                 // or ctx.setTheme(t) for an animated crossfade
```

The core token vocabulary widgets expect: colors `background`, `surface`, `surfaceAlt`,
`accent`, `accentText`, `text`, `textDim`, `scrim`; metric `radius`. Read them yourself with
`theme.color("accent")` / `theme.metric("radius")`; a missing token uses your fallback. You can
also build a `Theme` entirely in code from your own config.

`ctx.setTheme(newTheme)` crossfades every token over a few frames — theme switching is animated
for free.

---

## 6. Animation

Animation is a first-class primitive, not a bolt-on. Widgets own their own animation state; the
building block is `Spring`, a damped spring:

```cpp
Spring lift;              // value/vel/target, tunable stiffness & damping
lift.target = hovered ? 6.0f : 0.0f;
lift.step(dt);            // call each frame
float y = lift.value;     // eases toward target, no easing bookkeeping
```

There are also `easeOutCubic` / `easeOutBack` for one-shot tweens. `Card`, `Toggle`, and the
theme crossfade all use `Spring` internally — a good read if you're building an animated widget.

---

## 7. Input & interactivity

Spry is platform-agnostic: you translate your platform's events into `spry::InputEvent` and call
`ctx.handleEvent`. `hello.cpp` translates the mouse; the fuller pump in
[`examples/gl_demo.cpp`](../examples/gl_demo.cpp) also handles keyboard, wheel, text, and IME:

```cpp
InputEvent ev;
ev.type = InputEvent::MouseDown;
ev.x = px; ev.y = py; ev.button = 0;   // 0=left 1=right 2=middle
ctx.handleEvent(ev);
```

`Context` routes events to the hovered/pressed/focused widget, handles Tab focus traversal, and
fires `onClick` when a press and release land on the same widget. For text editing and clipboard,
wire two host hooks once:

- `ctx.setTextInputHandler(fn)` — called with the caret rect when a text widget is focused; wire
  it to `SDL_StartTextInput` / `SDL_SetTextInputArea` / `SDL_StopTextInput`.
- `spry::setClipboardHandlers(get, set)` — back them with `SDL_GetClipboardText` /
  `SDL_SetClipboardText`.

(These are the same few lines in every SDL host — an optional `spry/sdl_host.h` helper to remove
the boilerplate is tracked as a follow-up.)

Each frame, read `ctx.cursor()` and apply the platform cursor (Spry asks for resize cursors over
draggable dividers, etc.).

---

## 8. Overlays: menus, modals, tooltips, toasts

Transient layers live above the tree and manage their own open/close animation. Push one onto the
`Context`:

```cpp
auto menu = std::make_unique<Menu>();
menu->anchorX = x; menu->anchorY = y;
menu->addItem("Rename", [] { /* … */ });
menu->addItem("Delete", [] { /* … */ });
ctx.addOverlay(std::move(menu));
```

`Modal` (centered, dims the background), `Tooltip`, and `Toast` (stacked notifications) work the
same way. A widget's `onClick` can spawn an overlay via `Context::current()->addOverlay(...)` —
that's how `Combo` opens its dropdown. Hover tooltips are automatic: set `widget->tooltip = "…"`
and `Context` shows it after a hover delay.

---

## Where to go next

- **[`examples/hello.cpp`](../examples/hello.cpp)** — the minimal app from §2.
- **[`examples/demo.cpp`](../examples/demo.cpp)** — layout + theming gallery on the SDL backend.
- **[`examples/gl_demo.cpp`](../examples/gl_demo.cpp)** — the full interactive gallery on the GL
  backend: every widget, overlays, text editing, clipboard, HiDPI, animated theme swaps.
- **[`docs/adr/0001-spry-public-api.md`](adr/0001-spry-public-api.md)** — the API
  reference and design rationale.
