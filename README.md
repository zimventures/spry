# Spry

[![Docs](https://github.com/zimventures/spry/actions/workflows/docs.yml/badge.svg)](https://github.com/zimventures/spry/actions/workflows/docs.yml)

A retained-mode, **animation-first** UI toolkit in C++20 — lightweight, themeable,
GPU-rendered. Built for [Cleat](https://github.com/zimventures/cleat), designed
from day one to stand alone as a permissively-licensed open-source library.
Epic: #205.

### 📖 Documentation: **<https://zimventures.github.io/spry/>**

The full docs site is live — a getting-started tutorial, concept guides, a widget
catalog with screenshots, an examples gallery, and the complete
[C++ API reference](https://zimventures.github.io/spry/api/). It builds and deploys
automatically on every push to `main`.

**License:** [zlib](LICENSE) (permissive). Third-party dependency notices:
[`THIRD_PARTY.md`](THIRD_PARTY.md). Contributions welcome — see
[`CONTRIBUTING.md`](CONTRIBUTING.md).

> Spry develops inside the Cleat monorepo under `libs/spry/` and is mirrored to its own public
> repository with history preserved (`git subtree`, via `scripts/spry-split.sh` at the monorepo
> root). The monorepo is the source of truth.

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

## Learn it

Start at the **[documentation site](https://zimventures.github.io/spry/)**:

- **[Getting started](https://zimventures.github.io/spry/getting-started/)** — a hands-on tutorial
  from a blank window to an interactive, themed, animated app.
- **[Guides](https://zimventures.github.io/spry/guides/)** — layout, theming, animation, text,
  input, and renderer backends, one concept at a time.
- **[Widget catalog](https://zimventures.github.io/spry/widgets/)** — every built-in widget with a
  screenshot and usage snippet.
- **[Examples](https://zimventures.github.io/spry/examples/)** — runnable programs, smallest first.
- **[API reference](https://zimventures.github.io/spry/api/)** — generated from the headers; the
  design rationale + stability contract is in [ADR 0001](docs/adr/0001-spry-public-api.md).

Namespace `spry::`. Include the umbrella header plus exactly one renderer backend:

```cpp
#include <spry/spry.h>          // Context, Widget, Box, built-in widgets, Theme, anim, input
#include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h>
```

## Build (standalone demos)
```
cmake -B build -G Ninja
cmake --build build
./build/spry_hello     # minimal "hello toolkit" example — start here
./build/spry_demo      # SDL backend layout + theming gallery
./build/spry_gl_demo   # full interactive gallery (OpenGL backend)
```

Consume it from another CMake project with `add_subdirectory(libs/spry)` then
`target_link_libraries(myapp PRIVATE spry)`.
