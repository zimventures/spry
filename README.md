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
- 🚧 Public API (#207); OpenGL backend for in-Cleat integration (#217)

## Build (standalone demo)
```
cmake -B build -G Ninja
cmake --build build
./build/spry_demo
```

Namespace `spry::`; umbrella header `<spry/spry.h>`.
