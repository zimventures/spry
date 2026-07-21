# Guides

Concept guides that explain how Spry works, one piece at a time. Where the
[Getting started](../getting-started.md) tutorial builds an app end-to-end, these
guides go deep on a single subsystem.

- **[Layout & the widget tree](layout.md)** — the retained tree, `Box` flex
  layout, sizing hints, wrapping.
- **[Theming & tokens](theming.md)** — the token vocabulary, `.theme` files,
  animated theme swaps.
- **[Animation](animation.md)** — `Spring`, easing functions, transitions.
- **[Text](text.md)** — FreeType + HarfBuzz shaping, fonts, metrics.
- **[Input, focus & keyboard nav](input.md)** — `InputEvent`, hit-testing, focus
  traversal, the SDL host helper.
- **[Renderer backends](renderer-backends.md)** — `SdlRenderer` vs `GlRenderer`
  and when to pick each.

Each guide carries a runnable snippet and notes what's stable vs experimental. The
design reference behind them is the
[public-API ADR](../adr/0001-spry-public-api.md); the API reference is
[generated from the headers](../api/index.html).
