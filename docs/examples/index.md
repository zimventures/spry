# Examples

Runnable programs that build against the public API (`<spry/spry.h>` + one renderer
backend). New to Spry? Read them in this order, alongside the
[Getting started](../getting-started.md) tutorial.

| Example | Target | Backend | What it teaches |
|---|---|---|---|
| [`hello.cpp`](https://github.com/zimventures/spry/blob/main/examples/hello.cpp) | `spry_hello` | SDL | The minimal app in ~90 lines: window → `SdlRenderer` → a `Box` tree with a clickable `Button` → the `handleEvent` / `frame` loop. **Start here.** |
| [`demo.cpp`](https://github.com/zimventures/spry/blob/main/examples/demo.cpp) | `spry_demo` | SDL | Layout and theming: flex `Box` rows/columns, `Label` roles, `Card`s, `Panel`s, and hot-swappable themes loaded from `.theme` files (press **T** to crossfade). |
| [`gl_demo.cpp`](https://github.com/zimventures/spry/blob/main/examples/gl_demo.cpp) | `spry_gl_demo` | OpenGL | The full interactive gallery: `TabBar`, `Table`, `TreeView`, `ListView`, text input, overlays — plus the complete SDL→`InputEvent` host wiring, IME, clipboard, and HiDPI scaling. |

## Building & running

```sh
cmake -B build -G Ninja        # SPRY_BUILD_DEMO is ON by default
cmake --build build
./build/spry_hello
./build/spry_demo              # loads the themes copied next to the exe
./build/spry_gl_demo
```

!!! info "Gallery with screenshots coming in [#8](https://github.com/zimventures/spry/issues/8)"
    An examples gallery — code walkthroughs plus captured screenshots/GIFs of each
    demo — lands there. The sources live in
    [`examples/`](https://github.com/zimventures/spry/tree/main/examples) for now.
