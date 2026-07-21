# Layout & the widget tree

Spry is a **retained-mode** toolkit: you build a persistent tree of `Widget`
objects once and then drive it each frame. The host owns the window, the GPU, and
the event loop; Spry owns layout, drawing, theming, and animation. This guide
covers the tree, the frame loop that drives it, and the flexbox-style layout that
positions it.

## The frame loop

`spry::Context` owns the root widget, the active theme, the overlay stack, and
interaction state. The host drives it with just two calls:

- `handleEvent(const InputEvent&)` — feed one translated platform event.
- `frame(Renderer&, float dt, float mouseX, float mouseY)` — run one frame:
  theme interpolation → layout (`arrange`) → hover hit-test → `update(dt)` →
  `draw`.

```cpp
Context ctx;
ctx.setRoot(buildTree());                 // a std::unique_ptr<Widget>
ctx.setThemeImmediate(Theme::builtinDark());

while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) pumpEvent(ctx, e, win);   // → handleEvent
    ren.beginFrame(ctx.displayedTheme().color("background"));
    ctx.frame(ren, dt, mouseX, mouseY);
    ren.endFrame();
}
```

Everything else — the window, the renderer, the event pump — is host boilerplate.
See [Input, focus & keyboard nav](input.md) for the event side and
[Renderer backends](renderer-backends.md) for the drawing side.

## The Widget base

`spry::Widget` is the retained base class. Every widget carries:

- **Layout hints** the parent reads: `prefW` / `prefH` (preferred size; `-1` means
  auto / content-sized), `grow` (flex weight along the parent's main axis), and
  `margin`.
- A computed `rect` (filled in during layout), interaction flags (`focusable`,
  `focused`, …), and a `tooltip` string.
- Virtual lifecycle hooks — `measure` → `arrange` → `update(dt)` → `draw` /
  `paint` — and input hooks (`onClick`, `onKey`, `onWheel`). You rarely override
  these unless you're building a custom widget; the built-ins cover most needs.

Compose a tree with **`emplace<T>(args…)`** (construct a child in place, get a raw
pointer back so you can keep configuring it) or **`add(std::unique_ptr<Widget>)`**:

```cpp
auto root = std::make_unique<Box>();   // Box = flex container
root->axis = Axis::Column;
root->padding = Edges(24);
root->spacing = 16;

root->emplace<Label>("Hello, Spry", 2.4f);

auto* row = root->emplace<Box>();      // nested row
row->axis = Axis::Row;
row->spacing = 12;
row->emplace<Button>("OK", [] { /* onClick */ });
row->emplace<Button>("Cancel", [] {});
```

Hand the root to `ctx.setRoot(std::move(root))`. To change the UI later, mutate a
widget's public fields (e.g. `label->text = "…"`) — the next `frame()` re-measures
and redraws.

## Flex layout

Layout is flexbox-like. The two containers are **`Box`** (a row or column) and
**`WrapBox`** (left-to-right flow that wraps). A `Box` reads each child's hints and
places it along its main axis:

- `axis` — `Axis::Row` or `Axis::Column`.
- `spacing` — gap between children; `padding` (`Edges`) — inset around them.
- `cross` — cross-axis alignment: `Align::Start` / `Center` / `End` / `Stretch`.
- A child with `grow = 1` eats leftover space along the main axis; `prefW`/`prefH`
  set a preferred size (`-1` = content-sized).

The shared value types live in `layout.h`: `Size`, `Rect` (with `contains`),
`Edges` (margins/padding), and the `Axis` / `Align` enums.

> 📸 **Screenshot — a flex row/column layout.** _Placeholder; captured by the media
> pipeline ([#11](https://github.com/zimventures/spry/issues/11))._

## Stability

The widget tree, the `Context` frame loop, and the layout primitives are all
**stable public surface** (modules 1–3 of the
[public-API ADR](../adr/0001-spry-public-api.md)). The umbrella header
`<spry/spry.h>` is the contract: if a type is reachable from it, it's public.

## Related

- [Built-in widgets](../widgets/index.md) — what you put in the tree.
- [Theming & tokens](theming.md) — how widgets pick their colors.
- [Animation](animation.md) — how widgets move between frames.
- [Getting started §3](../getting-started.md#3-building-a-widget-tree) — the same
  tree, in tutorial form.
