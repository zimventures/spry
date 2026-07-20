# Third-party notices

Spry itself is licensed under the zlib license (see [`LICENSE`](LICENSE)). It depends on the
following third-party libraries, fetched at build time (not vendored in this repository). A
binary that links Spry also links these; their licenses are permissive and compatible with
Spry's zlib license. If you redistribute a binary built with Spry, include the acknowledgements
below.

## SDL3 — zlib license

- Project: <https://libsdl.org>
- License: zlib (<https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt>)
- Copyright © 1997–2025 Sam Lantinga and the SDL contributors.

SDL is provided "as-is" under the zlib license; no acknowledgement is required but is
appreciated. Spry uses SDL3 for windowing, input, the GPU/renderer surface, and clipboard.

## FreeType — FreeType License (FTL)

- Project: <https://freetype.org>
- License: the FreeType License (FTL, a BSD-style license), used here in preference to its
  alternative GPLv2 option (<https://gitlab.freedesktop.org/freetype/freetype/-/blob/master/docs/FTL.TXT>)
- Copyright © 1996–2025 David Turner, Robert Wilhelm, and Werner Lemberg.

The FTL requires the following credit in documentation that accompanies a distribution:

> Portions of this software are copyright © 2026 The FreeType Project (www.freetype.org).
> All rights reserved.

Spry uses FreeType for glyph rasterization.

## HarfBuzz — Old MIT license

- Project: <https://harfbuzz.github.io>
- License: "Old MIT" (permissive, MIT-style)
  (<https://github.com/harfbuzz/harfbuzz/blob/main/COPYING>)
- Copyright © 2010–2025 the HarfBuzz contributors (see the project's `AUTHORS`).

Spry uses HarfBuzz for text shaping (ligatures, complex-script layout).

## OpenGL

Spry's GL backend calls the platform's OpenGL implementation, loaded at runtime via
`SDL_GL_GetProcAddress`. OpenGL is an API specification, not bundled code; no separate license
notice is required.
