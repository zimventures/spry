# Input, focus & keyboard nav

Spry is platform-agnostic: you translate your platform's events into a
`spry::InputEvent` and call `ctx.handleEvent(...)`. `Context` routes events to the
hovered/pressed/focused widget, handles Tab focus traversal, and fires `onClick`
when a press and release land on the same widget. If your host is SDL3, the opt-in
[`<spry/sdl_host.h>`](https://github.com/zimventures/spry/blob/main/include/spry/sdl_host.h)
header does the event translation, HiDPI scaling, clipboard, and IME wiring for you.

!!! info "Full guide coming in [#6](https://github.com/zimventures/spry/issues/6)"
    For now, see [Getting started §7](../getting-started.md#7-input-interactivity)
    and the [`input.h`](https://github.com/zimventures/spry/blob/main/include/spry/input.h)
    [API reference](../api/index.html).
