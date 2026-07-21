# Theming & tokens

A `Theme` is a bag of named tokens — colors and float metrics — that widgets read
by role (`background`, `surface`, `accent`, `text`, …, and the `radius` metric).
Build one in code or load overrides from a flat `.theme` file; `ctx.setTheme(...)`
crossfades every token over a few frames, so theme switching is animated for free.

!!! info "Full guide coming in [#6](https://github.com/zimventures/spry/issues/6)"
    For now, see [Getting started §5](../getting-started.md#5-theming), the token
    vocabulary in [`theme_tokens.h`](https://github.com/zimventures/spry/blob/main/include/spry/theme_tokens.h),
    and the [`theme.h`](https://github.com/zimventures/spry/blob/main/include/spry/theme.h)
    [API reference](../api/index.html). A dedicated token reference page lands in
    [#9](https://github.com/zimventures/spry/issues/9).
