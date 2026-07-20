# Spry

A retained-mode, **animation-first** UI toolkit in C++20 — lightweight, themeable,
GPU-rendered. Built for [Cleat](https://github.com/zimventures/cleat), designed
from day one to stand alone as a permissively-licensed open-source library.
Epic: #205.

## Decoupling contract
Spry depends **only** on **SDL3** + **FreeType** + **HarfBuzz** — and **never** on
Cleat. That keeps it cleanly extractable to its own public repo later (#224).
Don't add Cleat includes under `libs/spry/`.

## Status (foundations)
- ✅ Renderer backend abstraction + SDL_Renderer backend (#208)
- ✅ Retained widget tree + flex layout (#209)
- ✅ Animation primitives — `spry::Spring`, easing (#210)
- ✅ Data-driven theming, hot-swappable with animated transitions (#211)
- ✅ Text via FreeType + HarfBuzz — shaping, ligatures, proportional metrics (#212)
- ✅ Public API & module design (#207)
- ✅ OpenGL backend for in-host integration (#217)

## Public API

The public surface is documented in
[`docs/adr/0001-spry-public-api.md`](../../docs/adr/0001-spry-public-api.md): the
Context/frame loop, the Widget tree, layout, theming, animation, input, and the renderer
backend interface — plus what's stable vs experimental and the theme-token vocabulary.

Namespace `spry::`. Include the umbrella header plus exactly one renderer backend:

```cpp
#include <spry/spry.h>          // Context, Widget, Box, built-in widgets, Theme, anim, input
#include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h>
```

`examples/hello.cpp` (target `spry_hello`) is the minimal one-screen example — a themed
window with a clickable button in ~90 lines, against only the public headers.

## Build (standalone demos)
```
cmake -B build -G Ninja
cmake --build build
./build/spry_demo      # SDL backend gallery
./build/spry_hello     # minimal "hello toolkit" example
```

Consume it from another CMake project with `add_subdirectory(libs/spry)` then
`target_link_libraries(myapp PRIVATE spry)`.
