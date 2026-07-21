# Renderer backends

Spry draws through a small `Renderer` abstraction with two implementations. Pick
**`SdlRenderer`** (wraps an `SDL_Renderer`) for a simple standalone app, or
**`GlRenderer`** to embed in a host that already owns an OpenGL context. You
include the umbrella header plus exactly one backend header, and the rest of the
API is backend-agnostic.

```cpp
#include <spry/spry.h>
#include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h>
```

!!! info "Full guide coming in [#6](https://github.com/zimventures/spry/issues/6)"
    For now, see [Getting started §1](../getting-started.md#1-add-spry-to-your-build)
    and the [`renderer.h`](https://github.com/zimventures/spry/blob/main/include/spry/renderer.h) /
    [`sdl_renderer.h`](https://github.com/zimventures/spry/blob/main/include/spry/sdl_renderer.h) /
    [`gl_renderer.h`](https://github.com/zimventures/spry/blob/main/include/spry/gl_renderer.h)
    headers in the [API reference](../api/index.html).
