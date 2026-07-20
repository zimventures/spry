# Spry examples

Runnable programs that build against the public API (`<spry/spry.h>` + one renderer backend).
New to Spry? Read them in this order, alongside the
[Getting started guide](../docs/getting-started.md).

| Example | Target | Backend | What it teaches |
|---|---|---|---|
| [`hello.cpp`](hello.cpp) | `spry_hello` | SDL | The minimal app in ~90 lines: window → `SdlRenderer` → a `Box` tree with a clickable `Button` → the `handleEvent` / `frame` loop. Start here. |
| [`demo.cpp`](demo.cpp) | `spry_demo` | SDL | Layout and theming: flex `Box` rows/columns, `Label` roles, `Card`s, `Panel`s, and hot-swappable themes loaded from `.theme` files (press **T** to crossfade). Hover-driven; no keyboard. |
| [`gl_demo.cpp`](gl_demo.cpp) | `spry_gl_demo` | OpenGL | The full interactive gallery: `TabBar`, `Table`, `TreeView`, `ListView`, `TextField`, `Slider`, `Toggle`, `Checkbox`, overlay-spawning buttons — plus the complete host wiring (SDL→`InputEvent` pump, text input/IME, clipboard, HiDPI scaling, animated theme swaps). The reference for embedding Spry in a GL host. |

## Building & running

The examples build with Spry. Standalone:

```sh
cmake -B build -G Ninja        # SPRY_BUILD_DEMO is ON by default
cmake --build build
./build/spry_hello
./build/spry_demo              # loads the themes copied next to the exe
./build/spry_gl_demo
```

When Spry is consumed as a subproject, the example targets are still built by default. To
configure them but keep them out of the default `all` build (so an IDE lists them and
`cmake --build … --target spry_hello` builds one on demand), set `SPRY_DEMO_EXCLUDE_FROM_ALL`
in the parent project before adding Spry — Cleat does this:

```cmake
set(SPRY_DEMO_EXCLUDE_FROM_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(libs/spry)
```

Set `SPRY_BUILD_DEMO=OFF` to skip the examples entirely.

`demo.cpp` and `gl_demo.cpp` load `.theme` files from [`themes/`](themes/), which CMake copies
next to their executables. `hello.cpp` uses only the built-in dark theme, so it needs nothing
alongside it.
