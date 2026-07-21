# Input, focus & keyboard nav

Spry's core is **platform-agnostic**: it consumes already-translated events. You
turn your platform's events into a `spry::InputEvent` and call `ctx.handleEvent(...)`;
`Context` routes each event to the right widget and manages focus. The host keeps
ownership of the window, the real event loop, the clipboard, and platform text
input.

## InputEvent

`InputEvent` is a small tagged struct. Its `type` is one of `MouseMove`, `MouseDown`,
`MouseUp`, `Wheel`, `KeyDown`, `Text`, or `TextEditing`, with the relevant fields
filled in — `x` / `y` (mouse position in Spry coordinates), `button` (`0`=left,
`1`=right, `2`=middle), and `text` (UTF-8 for `Text` / `TextEditing`). Keys use the
`spry::Key` enum; printable input arrives as `Text` (committed) or `TextEditing`
(IME pre-edit, not yet committed) rather than as key events.

```cpp
InputEvent ev;
ev.type = InputEvent::MouseDown;
ev.x = px; ev.y = py; ev.button = 0;
ctx.handleEvent(ev);
```

`Context` routes events to the hovered / pressed / focused widget, and fires a
widget's `onClick` when a press and release land on the same widget. Widgets react
by overriding `onClick`, `onKey(Key, shift, ctrl, alt)`, and `onWheel(dx, dy)`.

## Focus & keyboard navigation

A widget opts into keyboard focus by setting `focusable = true`. `Context` then:

- Moves focus on click, and traverses focusable widgets with **Tab** /
  **Shift-Tab**.
- Exposes `setFocus(Widget*)` and `focused()` so you can drive focus in code.
- Delivers key events to the focused widget first.

Each frame, read `ctx.cursor()` and apply the returned platform cursor — Spry asks
for e.g. a resize cursor when the mouse is over a draggable divider.

## The SDL host helper

The core ships **no** built-in SDL→`InputEvent` pump, to preserve the decoupling
contract. But if your host is SDL3, the opt-in
[`<spry/sdl_host.h>`](https://github.com/zimventures/spry/blob/main/include/spry/sdl_host.h)
header packages the standard wiring so you don't copy boilerplate:

- `pumpEvent(ctx, e, win)` — translate one `SDL_Event` and dispatch it (keycodes,
  wheel, text, IME, Cmd→Ctrl).
- `mouseToSpry(win, ...)` — scale window points to Spry pixels (HiDPI-correct).
- `installSdlHost(ctx, win)` — wire up clipboard + text-input/IME handlers once.

```cpp
installSdlHost(ctx, win);
while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running = false;   // lifecycle stays yours
        pumpEvent(ctx, e, win);
    }
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    mouseToSpry(win, mx, my, mx, my);   // frame()'s mouse must match pumpEvent's space
    ren.beginFrame(ctx.displayedTheme().color("background"));
    ctx.frame(ren, dt, mx, my);
    ren.endFrame();
}
```

It's outside the umbrella header (it pulls in SDL3) and entirely optional — a
non-SDL host translates its own events into `InputEvent` the same way. For text
input, register a handler with `ctx.setTextInputHandler(...)` so the host can show
the IME at the caret.

## Accessibility

!!! warning "Experimental"
    The accessibility surface is **[experimental]** per the
    [public-API ADR](../adr/0001-spry-public-api.md). The tree exposes
    `Widget::accessibleRole()` / `accessibleLabel()` (the `Role` enum) and
    `Context::visitAccessible(...)` walks it, but there is **no platform a11y
    backend yet** — nothing consumes the tree. The shapes may change when a backend
    lands, so don't build on them expecting stability.

## Stability

`InputEvent`, `Key`, the `Context` routing/focus API, and `sdl_host.h` are **stable
public surface**. Accessibility is experimental (above).

## Related

- [Built-in widgets](../widgets/index.md) — `TextField` / `TextArea` and the edit
  buffer.
- [Text](text.md) — how committed/IME text becomes glyphs.
- [Getting started §7](../getting-started.md#7-input-interactivity).
