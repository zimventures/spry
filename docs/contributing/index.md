# Contributing

Contributions to Spry — bug reports, new widgets, docs fixes — are welcome. The
canonical guide is
[`CONTRIBUTING.md`](https://github.com/zimventures/spry/blob/main/CONTRIBUTING.md)
in the repository; the essentials:

- **The decoupling contract is the one hard rule.** Spry depends only on SDL3,
  FreeType, and HarfBuzz (plus OpenGL for the GL backend) — and never on an
  embedding application. New third-party deps must be permissively licensed and
  recorded in [`THIRD_PARTY.md`](https://github.com/zimventures/spry/blob/main/THIRD_PARTY.md).
- **Keep the public API small, documented, and stable** — see the design in the
  [public-API ADR](https://github.com/zimventures/spry/blob/main/docs/adr/0001-spry-public-api.md).
  New widgets are additive; core changes need a strong reason.
- **Build & test** with CMake + Ninja; headless logic is covered by Catch2 tests
  in [`tests/`](https://github.com/zimventures/spry/tree/main/tests). Add or update
  tests with any behavior change.
- **Pull requests**: fork and branch from `main`, keep them focused, ensure the
  build is warning-clean and `ctest` passes, and update docs/examples when public
  behavior changes.

By submitting a contribution you agree to license it under the project's
[zlib license](https://github.com/zimventures/spry/blob/main/LICENSE).

## Working on the docs

See **[Building the docs](../building-docs.md)** for the local MkDocs + Doxygen
workflow.
