---
hide:
  - navigation
  - toc
---

# Spry

<p style="font-size:1.35rem;line-height:1.5;max-width:40rem;">
A retained-mode, <strong>animation-first</strong> UI toolkit in C++20 —
lightweight, themeable, GPU-rendered.
</p>

[![Docs](https://github.com/zimventures/spry/actions/workflows/docs.yml/badge.svg)](https://github.com/zimventures/spry/actions/workflows/docs.yml)
[![CI](https://github.com/zimventures/spry/actions/workflows/ci.yml/badge.svg)](https://github.com/zimventures/spry/actions/workflows/ci.yml)
[![License: zlib](https://img.shields.io/badge/license-zlib-blue.svg)](https://github.com/zimventures/spry/blob/main/LICENSE)

[Get started :material-arrow-right:](getting-started.md){ .md-button .md-button--primary }
[Browse the examples](examples/index.md){ .md-button }
[API reference](api/index.html){ .md-button }

---

## Why Spry

<div class="grid cards" markdown>

-   :material-motion-play-outline:{ .lg .middle } __Animation-first__

    ---

    Animation is a core primitive, not a bolt-on. Widgets own their motion; the
    `Spring` damped-spring drives hover, transitions, and even theme crossfades.

-   :material-sitemap-outline:{ .lg .middle } __Retained-mode & flexbox__

    ---

    Build a persistent tree of `Widget` objects once, then drive it each frame.
    Layout is flexbox-like — rows, columns, `grow`, padding, wrapping.

-   :material-palette-outline:{ .lg .middle } __Data-driven theming__

    ---

    A `Theme` is a bag of named tokens read by role. Swap themes at runtime and
    every token crossfades — animated theme switching for free.

-   :material-expansion-card-variant:{ .lg .middle } __GPU-rendered, tiny deps__

    ---

    SDL or OpenGL backends, text via FreeType + HarfBuzz. Depends only on SDL3,
    FreeType, and HarfBuzz — permissively (zlib) licensed and easy to embed.

</div>

## Quickstart

Include the umbrella header plus **one** renderer backend, build a tree, and drive
a frame loop:

```cpp
#include <spry/spry.h>          // Context, Widget, Box, widgets, Theme, anim, input
#include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h>
using namespace spry;

auto root = std::make_unique<Box>();
root->axis = Axis::Column;
root->padding = Edges(24);
root->spacing = 16;
root->emplace<Label>("Hello, Spry", 2.4f);
root->emplace<Button>("Click me", [] { /* onClick */ });

Context ctx;
ctx.setRoot(std::move(root));
ctx.setThemeImmediate(Theme::builtinDark());

while (running) {
    while (SDL_PollEvent(&e)) ctx.handleEvent(translate(e));  // feed input
    ren.beginFrame(ctx.displayedTheme().color("background"));
    ctx.frame(ren, dt, mouseX, mouseY);                       // layout → hover → update → draw
    ren.endFrame();
}
```

Add Spry to a CMake project:

```cmake
add_subdirectory(libs/spry)            # or vendor it however you like
target_link_libraries(myapp PRIVATE spry)
```

The full minimal program is [`examples/hello.cpp`](examples/index.md); the
[Getting started](getting-started.md) tutorial builds it up one piece at a time.

## Where to next

- **[Getting started](getting-started.md)** — a hands-on tutorial from a blank
  window to an interactive, themed, animated app.
- **[Guides](guides/index.md)** — layout, theming, animation, text, input, and
  renderer backends, one concept at a time.
- **[Widgets](widgets/index.md)** — the built-in widget catalog.
- **[Examples](examples/index.md)** — runnable programs, smallest first.
- **[API reference](api/index.html)** — the generated C++ reference.

Spry is developed by [zimventures](https://github.com/zimventures/spry) and released
under the [zlib license](https://github.com/zimventures/spry/blob/main/LICENSE).
Contributions are [welcome](contributing/index.md).
