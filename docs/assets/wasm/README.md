# Live WASM demo assets

The **real Spry toolkit compiled to WebAssembly**, plus the host page and the
fallback stills the docs embed. Like the rest of `docs/assets/`, **these files are
committed** — the site build only serves them, it never builds anything.

## What's here

| File | What it is |
|---|---|
| `spry_demos.js` + `spry_demos.wasm` | One shared module serving **every** demo scene. The SDL/FreeType/HarfBuzz core + all scenes download once. |
| `demo.html` | The host page the docs `<iframe>` loads. Reads `?scene=<id>`, passes it to `main()` as `argv[1]`, and shows the scene's still (`scene-<id>.png`) as a **poster** behind the canvas — while the module downloads, and as the **fallback** if WebAssembly is unavailable, the script fails to load, or startup times out. |
| `scene-*.png` | A still of each scene, rendered by `spry_capture`. Serves as the poster / no-JS / no-WASM fallback (see below) and as the source image for social cards. |

## Embedding a demo in a page (#40)

Every embed follows one pattern — a lazy `<iframe>` with the shared `.spry-demo`
class (styled in `docs/stylesheets/extra.css`), pointed at a scene:

```html
<iframe class="spry-demo" src="../assets/wasm/demo.html?scene=<id>"
        title="Spry live demo — <description>" loading="lazy"
        sandbox="allow-scripts allow-same-origin"></iframe>
```

- **Lazy-load** — `loading="lazy"` defers the whole module until the iframe scrolls
  into view, so a page with several demos doesn't download megabytes up front.
- **Fallback** — `demo.html` itself shows `scene-<id>.png` as a poster and keeps it
  if WebAssembly is unavailable, the script fails to load, or startup times out, so
  the box is never empty. Keep descriptive prose next to each embed — a `<canvas>`
  isn't screen-reader accessible, so the surrounding text and the poster's `alt`
  carry the meaning.
- **No-JS (optional)** — the poster is selected by script (the scene id is in the
  URL), so a browser with JavaScript fully disabled shows an empty frame. For a page
  that must survive that, add a `<noscript>` static image at the embed site, where
  the scene is known: `<noscript><img class="spry-demo" src="../assets/wasm/scene-<id>.png" alt="…"></noscript>`.
- **Scene ids** — the `id` values registered in
  [`examples/web/scenes.h`](https://github.com/zimventures/spry/blob/main/examples/web/scenes.h):
  `theming`, `controls`, `layout`, `animation`, `text`, `data`, `overlays`,
  `textinput`.

Wrap it in `<figure class="spry-demo-fig">…<figcaption>…</figcaption></figure>` for a
caption.

The scenes themselves live in
[`examples/web/scenes.h`](https://github.com/zimventures/spry/blob/main/examples/web/scenes.h)
and are driven by
[`examples/web/web_demos.cpp`](https://github.com/zimventures/spry/blob/main/examples/web/web_demos.cpp).

## How the artifacts reach the deployed site (the #39 decision)

**We commit the built `.js`/`.wasm` rather than building them in CI.** The docs
workflow needs no Emscripten step; it just publishes `docs/`.

Why commit rather than build in CI:

- **One module, not many.** A single multiplexed module (`spry_demos`) serves all
  scenes via `?scene=<id>`, so there's exactly one ~2.7 MB `.wasm` in the repo — it
  doesn't grow as scenes are added.
- **It changes rarely.** The module only needs rebuilding when `scenes.h` /
  `web_demos.cpp` change — not on every docs edit. Committing keeps the docs deploy
  fast and hermetic (no emsdk + SDL/FreeType/HarfBuzz WASM build, and nothing to
  cache, on every run).
- **Diffs stay clean.** `.gitattributes` marks the artifacts
  `linguist-generated` (and the `.wasm` `binary`), so they're collapsed in reviews
  and excluded from language stats.

If the module count ever grows enough that committing becomes unwieldy, the
alternative is a CI job (e.g. `mymindstorm/setup-emsdk` + a cached WASM build) that
stages the output into the site and adds the two generated artifacts —
`spry_demos.js` and `spry_demos.wasm` — to `.gitignore`. That trade wasn't worth it
for a single, rarely-changing module.

## Rebuilding

Rebuild the module (requires the [Emscripten SDK](https://emscripten.org); point
`$EMSDK` at it):

```sh
scripts/build-web.sh          # emcmake build → copies spry_demos.{js,wasm} here
```

Refresh the `scene-*.png` fallback stills (rendered headlessly, no browser needed):

```sh
scripts/capture-media.sh      # builds spry_capture, renders every still
```

Rebuild the module **whenever you add or change a scene in `scenes.h`**, and commit
the updated `spry_demos.{js,wasm}` alongside the source — the committed binary is
what the live site runs.
