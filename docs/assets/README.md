# Docs media assets

Media used by the documentation site. **These files are committed** so the site
build just consumes them — it never renders anything.

The docs now embed **live WASM demos** instead of static screenshots (see
[`wasm/README.md`](wasm/README.md)). The only rendered images left are the
**fallback stills** each demo shows while its module loads or when WebAssembly
isn't available.

## What's here

| Path | What it is |
|---|---|
| `wasm/` | The live demo module (`spry_demos.{js,wasm}`), its host page (`demo.html`), and one **fallback still per scene** (`scene-*.png`). See [`wasm/README.md`](wasm/README.md). |
| `logo.svg` | The site logo (hand-authored; referenced from `mkdocs.yml`). |

## Regenerating the fallback stills

The `scene-*.png` stills are rendered headlessly by `tools/capture/capture.cpp`
(the `spry_capture` target), which links only the **public** Spry API + SDL3,
exactly like a consumer:

- It renders each scene from [`examples/web/scenes.h`](https://github.com/zimventures/spry/blob/main/examples/web/scenes.h)
  — the same builders the live demos use — through an **offscreen SDL software
  renderer** (no window/display needed, so it runs in CI too), for a few frames so
  springs and overlay animations settle.
- Each still is written as PNG via the vendored `stb_image_write.h`, in the same
  `JetBrainsMono` font the WASM module embeds, so the fallback matches the live view.

```sh
scripts/capture-media.sh      # builds spry_capture, renders every scene-*.png
```

Requires a C++ toolchain + CMake. Regenerate after adding or changing a scene in
`examples/web/scenes.h`, and commit the updated `scene-*.png` alongside the source.
