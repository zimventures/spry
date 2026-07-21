# Widgets

Spry ships a library of built-in widgets, all from `<spry/spry.h>`. Most controls
follow the same pattern: public fields for state, a `std::function` callback for
changes. Mutate a widget's fields and the next `frame()` re-measures and redraws.

- **Text & surfaces** — `Label`, `Paragraph` (word-wrap), `Panel`, `Card`,
  `ProgressBar`, `Image`.
- **Controls** — `Button`, `Checkbox`, `RadioButton`, `Toggle`, `Slider`, `Combo`.
- **Text input** — `TextField` (single line), `TextArea` (multi-line); the headless
  `EditBuffer` behind them is usable on its own.
- **Color** — `ColorField` (a swatch that opens a picker), `ColorPickerPad`.
- **Data containers** — `ListView`, `Table` (sortable), `TreeView`, `ScrollView`,
  `TabBar` — virtualized where it matters.
- **Overlays** — `Menu`, `Modal`, `Tooltip`, `Toast`.

!!! info "Catalog with screenshots coming in [#7](https://github.com/zimventures/spry/issues/7)"
    A visual catalog — each widget with a screenshot and usage snippet — lands
    there. For now, [`gl_demo.cpp`](https://github.com/zimventures/spry/blob/main/examples/gl_demo.cpp)
    exercises nearly every widget (see the [examples](../examples/index.md)), and each
    type is in the [API reference](../api/index.html).
