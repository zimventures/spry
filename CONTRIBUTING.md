# Contributing to Spry

Thanks for your interest in Spry — a retained-mode, animation-first C++20 UI toolkit. Whether
it's a bug report, a new widget, or a docs fix, contributions are welcome.

## Ground rules

**The decoupling contract is the one hard rule.** Spry depends **only** on SDL3, FreeType, and
HarfBuzz (and OpenGL for the GL backend) — and **never** on any application that embeds it. Don't
add a dependency on an embedding app, and don't introduce a new third-party dependency without
discussing it first (it changes the "lightweight, permissively-licensed" contract and every
downstream binary). New third-party deps must be permissively licensed and recorded in
[`THIRD_PARTY.md`](THIRD_PARTY.md).

Keep the public API small, documented, and stable — see the design in
[`docs/adr/0001-spry-public-api.md`](docs/adr/0001-spry-public-api.md). New widgets are
additive; changes to the core (`Context`, `Widget`,
`Renderer`, `Theme`, `InputEvent`) need a strong reason and, once the library is versioned, a
semver-major bump.

## Building & testing

Standalone:

```sh
cmake -B build -G Ninja        # SPRY_BUILD_DEMO and SPRY_BUILD_TESTS default ON at top level
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests use Catch2 (fetched automatically) and live in [`tests/`](tests/); they cover the headless
logic (input/focus, the edit buffer, widgets, data models) without a GPU. Add or update tests
with any behavior change. The demos (`spry_demo`, `spry_gl_demo`, `spry_hello`) are the manual
visual check — run them if you touch rendering or layout.

## Code style

- C++20, 4-space indent, `namespace spry`. Match the surrounding style of the file you're editing
  (the codebase keeps single-line `if` bodies and inline headers where they read well).
- Public headers are the API surface — document new public types/methods with a brief comment and
  keep implementation detail behind `private:` / pimpl.
- Prefer plain value types and `std::function` callbacks over inheritance-heavy designs, matching
  the existing widgets.

## Pull requests

1. Fork and branch from `main`.
2. Keep PRs focused; describe what changed and why.
3. Ensure `cmake --build` is clean (no new warnings) and `ctest` passes.
4. Update docs/examples when you change public behavior.

By submitting a contribution you agree to license it under the project's
[zlib license](LICENSE).

## Reporting bugs

Open an issue with a minimal repro — ideally a small diff to `examples/hello.cpp` or a failing
test case. Note your platform, compiler, and SDL/FreeType/HarfBuzz versions.
