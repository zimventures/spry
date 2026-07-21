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

## Notes

- Dependency versions in `requirements-docs.txt` are pinned for reproducibility;
  bump them deliberately.
- The full CI build-and-deploy to GitHub Pages is set up in
  [#3](https://github.com/zimventures/spry/issues/3); the nav/landing-page shape in
  [#4](https://github.com/zimventures/spry/issues/4).
