# Renderer backends

Widgets never touch the GPU directly — they draw through `spry::Renderer`, a small
abstract 2D interface. Two concrete backends ship; you **pick** one for your host
(you rarely implement `Renderer` yourself). This seam is what lets the same widgets
run standalone under SDL or embedded inside an OpenGL host without knowing the
difference.

```cpp
#include <spry/spry.h>
#include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h> — exactly one
```

## The Renderer interface

`Renderer` is what widgets emit to, and what a backend implements:

- **Frame** — `beginFrame(clearColor)`, `endFrame()`, `outputSize(w, h)`.
- **Primitives** — `fillRect`, `fillRoundedRect`, `fillMesh`, and `text` /
  `measureText` (see [Text](text.md)).
- **Images** — optional `loadImage` / `drawImage` (backends without image support
  make `drawImage` a no-op).
- **Clip & opacity** — `pushClip` / `popClip` and `pushOpacity` are provided by the
  base class, which maintains the stacks; a backend only implements `applyClip`.

Supporting value types: `Color`, `Vertex`, `Mesh`, and the opaque `ImageHandle`.

## Choosing a backend

=== "SdlRenderer — standalone"

    Wraps a host-owned `SDL_Renderer*`. The simple path for a standalone app; text
    is rendered via FreeType behind a pimpl. Note that `sdl_renderer.h` pulls in
    `<SDL3/SDL.h>`, so that translation unit sees SDL.

    ```cpp
    SdlRenderer ren(sdl);   // sdl is your SDL_Renderer*
    ```

=== "GlRenderer — embed in a GL host"

    An OpenGL 3.x backend for embedding into a host that already owns a GL context.
    It renders into its own FBO and blits, which is how it composites with a larger
    GL application. `gl_renderer.h` is pimpl-clean (it doesn't leak SDL into your
    translation unit).

    A current GL context must exist before you construct it, and the host provides
    the drawable size via `setSize` (call it again on resize — the backend doesn't
    own the window):

    ```cpp
    GlRenderer ren;                 // a current GL context must already exist
    ren.loadFont("…/DejaVuSansMono.ttf");
    ren.setSize(fbW, fbH);          // drawable size in pixels; call again on resize
    ren.setContentScale(dpiScale);  // optional: HiDPI framebuffer scale (default 1)
    ```

    The host then composites Spry's FBO into its own frame with `blitToDefault` or
    `presentBlended` (see Stability below).

Rule of thumb: building a small app or tool? Use **`SdlRenderer`**. Dropping a
Spry panel into an existing OpenGL application? Use **`GlRenderer`**.

## Stability

The `Renderer` interface and both backends are **stable public surface**. One
caveat from the [public-API ADR](../adr/0001-spry-public-api.md): `GlRenderer`'s
FBO/compositing extras (`targetTexture`, `blitToDefault`, `presentBlended`) are
**[Cleat-shaped but generic]** — the behavior is general, but they were built for a
specific host's compositing needs, so don't expect tailored ergonomics. The
abstract `Renderer` seam also leaves room for a future Vulkan/Metal/`SDL_gpu`
backend to drop in.

## Related

- [Text](text.md) — the FreeType/HarfBuzz path behind `text()`.
- [Layout & the widget tree](layout.md) — where `beginFrame`/`frame`/`endFrame` fit.
- Examples:
  [`demo.cpp`](https://github.com/zimventures/spry/blob/main/examples/demo.cpp) (SDL)
  and [`gl_demo.cpp`](https://github.com/zimventures/spry/blob/main/examples/gl_demo.cpp) (GL).
