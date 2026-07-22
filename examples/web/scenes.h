// Shared registry of Spry demo scenes (#38). Each scene builds a Widget tree; the
// host program (web_demos.cpp) picks one by id (`?scene=<id>`) so a single WASM
// module serves every embedded demo — the SDL/FreeType/HarfBuzz core downloads
// once. Scene builders are inline + header-only so a native tool (e.g. the capture
// harness) can render them headlessly for verification too.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "spry/spry.h"

namespace spry {
namespace demos {

/// A named demo scene. `interactive` selects how the host handles input:
///   false → the click / T gesture crossfades the theme (showcase scenes);
///   true  → real input is dispatched to the widgets (via `pumpEvent`).
struct Scene {
    const char* id;
    const char* title;
    std::unique_ptr<Widget> (*build)();
    bool interactive;
};

// ── theming: the layout + theming showcase (hover the cards, click/T to swap) ──
inline std::unique_ptr<Widget> buildTheming() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 16;

    root->emplace<Label>("Spry — tree + layout + theming", 2.2f);
    auto* sub = root->emplace<Label>("hover the cards · click (or press T) to swap theme", 1.4f);
    sub->role = "textDim";

    auto* row = root->emplace<Box>();
    row->axis = Axis::Row;
    row->spacing = 14;
    row->prefH = 130;
    for (const char* n : {"Terminal", "SFTP", "Connections", "Keys"}) {
        auto* c = row->emplace<Card>(n);
        c->grow = 1.0f;
    }

    auto* content = root->emplace<Panel>();
    content->grow = 1.0f;
    auto* cbox = content->emplace<Box>();
    cbox->axis = Axis::Column;
    cbox->padding = Edges(18);
    cbox->spacing = 12;
    auto* cl = cbox->emplace<Label>("content area — all colors come from theme tokens", 1.5f);
    cl->role = "textDim";
    cbox->emplace<Label>("HarfBuzz shaping:  -> => != == >= <= |> <| :: www  (ligatures)", 1.6f);

    auto* bar = root->emplace<Box>();
    bar->axis = Axis::Row;
    bar->prefH = 30;
    bar->spacing = 16;
    auto* s1 = bar->emplace<Label>("running in the browser via WebAssembly", 1.4f);
    s1->colorOverride = Color{120, 200, 150};
    bar->emplace<Widget>()->grow = 1.0f; // spacer
    auto* s2 = bar->emplace<Label>("spry::Theme — hot-swappable, animated crossfade", 1.4f);
    s2->role = "textDim";

    return root;
}

// ── controls: a fully-interactive gallery of the basic controls (#34) ──
inline std::unique_ptr<Widget> buildControls() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(26);
    root->spacing = 14;
    root->emplace<Label>("Buttons & controls", 2.0f);
    auto* status = root->emplace<Label>("interact with the controls — they're live", 1.4f);
    status->role = "textDim";

    auto* buttons = root->emplace<Box>();
    buttons->axis = Axis::Row;
    buttons->spacing = 12;
    buttons->emplace<Button>("Primary", [status] { status->text = "Primary clicked"; });
    buttons->emplace<Button>("Secondary", [status] { status->text = "Secondary clicked"; });

    auto* chk = root->emplace<Checkbox>("Enable feature", true);
    chk->onChange = [status](bool on) { status->text = std::string("Checkbox: ") + (on ? "on" : "off"); };

    // Radio group — selecting one clears the others (the group is caller-managed).
    auto* radioRow = root->emplace<Box>();
    radioRow->axis = Axis::Row;
    radioRow->spacing = 18;
    auto group = std::make_shared<std::vector<RadioButton*>>();
    for (const char* name : {"Small", "Medium", "Large"}) {
        auto* rb = radioRow->emplace<RadioButton>(name, group->empty()); // first is selected
        group->push_back(rb);
        rb->onSelect = [group, rb, status, name] {
            for (RadioButton* o : *group) o->selected = (o == rb);
            status->text = std::string("Selected: ") + name;
        };
    }

    auto* tg = root->emplace<Toggle>("Dark mode", true);
    tg->onChange = [status](bool on) { status->text = std::string("Toggle: ") + (on ? "on" : "off"); };

    // The slider drives the progress bar live.
    auto* slider = root->emplace<Slider>(0.0f, 100.0f, 65.0f);
    slider->prefW = 300;
    auto* progress = root->emplace<ProgressBar>();
    progress->value = 0.65f;
    progress->prefW = 300;
    slider->onChange = [progress, status](float v) {
        progress->value = v / 100.0f;
        status->text = "Slider: " + std::to_string((int)v);
    };

    return root;
}

// ── layout: an interactive flex playground (#31) ──
inline std::unique_ptr<Widget> buildLayout() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(22);
    root->spacing = 12;
    root->emplace<Label>("Layout — the flex Box", 2.0f);
    auto* hint = root->emplace<Label>("flip the controls and watch the tree re-lay-out", 1.4f);
    hint->role = "textDim";

    // A control row, then the container the controls manipulate.
    auto* controls = root->emplace<Box>();
    controls->axis = Axis::Row;
    controls->spacing = 12;

    auto* demo = root->emplace<Box>();
    demo->grow = 1.0f;
    demo->axis = Axis::Row;
    demo->spacing = 12;
    demo->padding = Edges(6);
    Card* first = nullptr;
    for (const char* n : {"A", "B", "C"}) {
        auto* c = demo->emplace<Card>(n);
        c->prefW = 130;
        c->prefH = 90;
        if (!first) first = c;
    }

    auto* axis = controls->emplace<Toggle>("Column", false);
    axis->onChange = [demo](bool on) { demo->axis = on ? Axis::Column : Axis::Row; };
    auto* grow = controls->emplace<Toggle>("A grows", false);
    grow->onChange = [first](bool on) { if (first) first->grow = on ? 1.0f : 0.0f; };
    auto* cross = controls->emplace<Combo>(
        std::vector<std::string>{"cross: Stretch", "cross: Start", "cross: Center", "cross: End"}, 0);
    cross->prefW = 190;
    cross->onChange = [demo](int i, const std::string&) {
        demo->cross = i == 1 ? Align::Start : i == 2 ? Align::Center : i == 3 ? Align::End : Align::Stretch;
    };

    return root;
}

/// The scene registry. Additional scenes are appended here by their tickets
/// (#31–#37).
inline const std::vector<Scene>& registry() {
    static const std::vector<Scene> r = {
        {"theming", "Layout & theming", &buildTheming, false},
        {"controls", "Buttons & controls", &buildControls, true},
        {"layout", "Layout — the flex Box", &buildLayout, true},
    };
    return r;
}

/// Look up a scene by id; falls back to the first scene if `id` is unknown.
inline const Scene* find(const std::string& id) {
    for (const Scene& s : registry())
        if (id == s.id) return &s;
    return registry().empty() ? nullptr : &registry().front();
}

} // namespace demos
} // namespace spry
