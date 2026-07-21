# Layout & the widget tree

Spry is a **retained-mode** toolkit: you build a persistent tree of `Widget`
objects once, then drive it each frame. Layout is flexbox-like — each widget
carries hints (`prefW`/`prefH`, `grow`, `margin`) that its parent `Box` reads to
place it in a row or column, with `padding`, `spacing`, and cross-axis alignment.

!!! info "Full guide coming in [#6](https://github.com/zimventures/spry/issues/6)"
    For now, see [Getting started §3](../getting-started.md#3-building-a-widget-tree)
    and the [`layout.h`](https://github.com/zimventures/spry/blob/main/include/spry/layout.h) /
    [`box.h`](https://github.com/zimventures/spry/blob/main/include/spry/box.h) headers
    in the [API reference](../api/index.html).
