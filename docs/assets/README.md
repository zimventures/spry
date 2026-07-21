# Docs media assets

Screenshots and GIFs used by the documentation site. **These files are committed**
so the site build just consumes them — it never renders anything. To refresh them,
regenerate with one command (see below); don't edit the images by hand.

## How they're captured

Media is rendered by a small headless tool, `tools/capture/capture.cpp` (the
`spry_capture` target), which links only the **public** Spry API + SDL3, exactly
like a consumer:

- It renders each scene through an **offscreen SDL software renderer** into an
  in-memory surface — no window or display is required, so it runs in CI too.
- Scenes are defined in `capture.cpp`; each is a real `Widget` tree driven by a
  `Context` for a few frames so springs and overlay animations settle.
- Screenshots are written as PNG via the vendored `stb_image_write.h`.
- GIFs are assembled by **ffmpeg** from a frame sequence the tool dumps during an
  animated theme crossfade.

Themes: the built-in dark theme (`Theme::builtinDark()`) and a light theme
(`builtinLight()` in the tool, mirrored by `examples/themes/daylight.theme`), so
the docs show both.

## Regenerating

```sh
scripts/capture-media.sh
```

Requires a C++ toolchain + CMake (to build `spry_capture`) and **ffmpeg** (for the
GIFs; screenshots still update without it). The script builds the tool, renders
every asset into this directory, and assembles the GIF.

## The asset set

| File | Scene | Theme |
|---|---|---|
| `hello-dark.png` | The minimal "hello" app | dark |
| `layout-dark.png` | Flex layout (cards + a grow split) | dark |
| `widgets-dark.png` | Control gallery | dark |
| `widgets-light.png` | Control gallery | light |
| `menu-dark.png` | An open context menu (overlay) | dark |
| `theme-swap.gif` | Animated dark ↔ light theme crossfade | both |
