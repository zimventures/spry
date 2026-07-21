# Text

Spry renders text with **FreeType** (glyph rasterization and metrics) and
**HarfBuzz** (shaping), so it handles ligatures, proportional metrics, and
complex scripts rather than fixed-cell blitting. Load a font through the renderer
and use it via `Label`, `Paragraph` (word-wrap), and the text-input widgets.

!!! info "Full guide coming in [#6](https://github.com/zimventures/spry/issues/6)"
    For now, see [Getting started §4](../getting-started.md#4-built-in-widgets) and
    the [`renderer.h`](https://github.com/zimventures/spry/blob/main/include/spry/renderer.h)
    [API reference](../api/index.html).
