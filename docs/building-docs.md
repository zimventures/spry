# Building the docs

The Spry documentation site has two independently-built surfaces that are stitched
together at publish time:

| Surface | Tool | Source | Output |
|---|---|---|---|
| Guides, tutorials, examples (this site) | [MkDocs Material](https://squidfunk.github.io/mkdocs-material/) | Markdown in `docs/` | `site/` |
| C++ API reference (`/api/`) | [Doxygen](https://www.doxygen.nl/) + [doxygen-awesome-css](https://jothepro.github.io/doxygen-awesome-css/) | doc-comments in `include/spry/` | `docs/api/` |

Keeping them as separate, cleanly-linked surfaces (rather than bridging with
Breathe/Sphinx) is a deliberate choice — see the epic
([#1](https://github.com/zimventures/spry/issues/1)).

## Prerequisites

- **Python 3.9+** with `pip`
- **Doxygen 1.9.5+** (needed for doxygen-awesome-css) — [install](https://www.doxygen.nl/download.html),
  or `choco install doxygen.install` (Windows) / `brew install doxygen` (macOS) /
  `apt-get install doxygen` (Linux)
- **git** (the API theme is fetched as a pinned clone)

## Prose site (MkDocs)

```sh
python -m venv .venv-docs
. .venv-docs/bin/activate           # Windows: .venv-docs\Scripts\activate
pip install -r requirements-docs.txt

mkdocs serve                         # live preview at http://127.0.0.1:8000
mkdocs build                         # one-shot build into site/
```

`mkdocs serve` hot-reloads as you edit Markdown under `docs/`.

## API reference (Doxygen)

```sh
scripts/fetch-doxygen-awesome.sh     # one-time: fetch the pinned CSS theme into external/
doxygen                              # reads ./Doxyfile → writes docs/api/
```

Open `docs/api/index.html`, or browse it in-site at `/api/` once MkDocs has built
(the API output lives inside `docs/` so MkDocs copies it through as static files).

!!! tip "Full local site"
    To preview both surfaces together, run `doxygen` first (populating `docs/api/`)
    and then `mkdocs serve` — the **API reference** nav entry resolves to the
    generated pages. `docs/api/`, `site/`, and `external/` are all git-ignored.

## Social cards (imaging deps)

The Material **social** plugin generates per-page `og:image` cards, which needs the
imaging extras (`Pillow` + `CairoSVG`, in `requirements-docs.txt`) **and the native
Cairo libraries**:

- **Linux** — `sudo apt-get install libcairo2-dev libfreetype6-dev libffi-dev libjpeg-dev libpng-dev libz-dev`
- **macOS** — `brew install cairo freetype libffi`
- **Windows** — install [GTK for Windows](https://github.com/tschoonj/GTK-for-Windows-Runtime-Environment-Installer) (provides `cairo.dll`)

CI installs the Linux libs automatically. Cards regenerate on every build.

## Live demos & fallback stills

The docs embed **live WASM demos** rather than static screenshots — see
[docs/assets/wasm/README.md](https://github.com/zimventures/spry/blob/main/docs/assets/wasm/README.md)
for the embed pattern and how the module is built and shipped. Each demo falls back
to a committed still (`docs/assets/wasm/scene-*.png`) rendered by a small headless
tool; regenerate them with `scripts/capture-media.sh` (see
[docs/assets/README.md](https://github.com/zimventures/spry/blob/main/docs/assets/README.md)).
The site build only *consumes* the committed media.

## Versioning

The site publishes a **single, unversioned** copy (latest `main`). We deliberately
**defer** multi-version docs (e.g. [`mike`](https://github.com/jimporter/mike)) until
Spry is versioned/extracted (semver begins after the public-repo split, #224). Pre-1.0,
one always-current site is simpler and there are no released versions to preserve;
revisit at the first tagged release.

## Notes

- Dependency versions in `requirements-docs.txt` are pinned for reproducibility;
  bump them deliberately.
- Every push to `main` builds and deploys automatically to
  <https://zimventures.github.io/spry/>.
