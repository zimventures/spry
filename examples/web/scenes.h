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
    auto* hint = root->emplace<Label>("flip the controls and watch the tree re-lay out", 1.4f);
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

    auto* axis = controls->emplace<Toggle>("Row layout", false); // label tracks the current axis
    axis->onChange = [demo, axis](bool on) {
        demo->axis = on ? Axis::Column : Axis::Row;
        axis->label = on ? "Column layout" : "Row layout";
    };
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

// ── animation: a draggable Spring visualizer (#32) ──
// Click/drag in the pad to set a target; a dot springs toward it each frame. The
// sliders tune stiffness/damping so the reader feels the spring model directly.
class SpringPad : public Widget {
public:
    Spring sx, sy;                  // 2D position springs (value chases target)
    float stiffness = 200.0f;
    float damping = 16.0f;

    Size measure(Renderer&, float availW, float) override {
        // A modest preferred height; grow=1 expands it, so don't claim all availH
        // (that would starve non-grow siblings like the sliders below).
        return Size{availW > 0 ? availW : 480.0f, 200.0f};
    }
    void arrange(Renderer& r, Rect rc) override {
        Widget::arrange(r, rc);
        if (!inited_) { // center the dot on first layout
            sx.value = sx.target = rc.w * 0.5f;
            sy.value = sy.target = rc.h * 0.5f;
            inited_ = true;
        }
    }
    void update(float dt) override {
        sx.stiffness = sy.stiffness = stiffness;
        sx.damping = sy.damping = damping;
        sx.step(dt);
        sy.step(dt);
    }
    bool onMouseDown(float x, float y, int, bool, bool) override {
        sx.target = x - rect.x;
        sy.target = y - rect.y;
        return true;
    }
    void onMouseDrag(float x, float y) override {
        sx.target = x - rect.x;
        sy.target = y - rect.y;
    }
    void paint(Renderer& r, const Theme& th) override {
        float rad = th.metric(tokens::Radius, 8.0f);
        r.fillRoundedRect(rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, rect.w, rect.h, rad,
                          th.color(tokens::SurfaceAlt, {32, 34, 48}), th.color(tokens::Surface, {40, 43, 62}));
        Color acc = th.color(tokens::Accent, {96, 126, 205});
        Color ring{acc.r, acc.g, acc.b, 110};
        r.fillRoundedRect(rect.x + sx.target, rect.y + sy.target, 12, 12, 6, ring, ring); // target
        r.fillRoundedRect(rect.x + sx.value, rect.y + sy.value, 22, 22, 11, acc, acc);    // the dot
    }

private:
    bool inited_ = false;
};

inline std::unique_ptr<Widget> buildAnimation() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(22);
    root->spacing = 12;
    root->emplace<Label>("Animation — springs", 2.0f);
    auto* hint = root->emplace<Label>("click/drag in the pad — the dot springs to the point; tune the springs", 1.4f);
    hint->role = "textDim";

    auto* pad = root->emplace<SpringPad>();
    pad->grow = 1.0f;

    auto* sliders = root->emplace<Box>();
    sliders->axis = Axis::Row;
    sliders->spacing = 12;
    sliders->cross = Align::Center;
    sliders->emplace<Label>("stiffness", 1.3f);
    auto* stf = sliders->emplace<Slider>(20.0f, 500.0f, pad->stiffness);
    stf->prefW = 200;
    stf->onChange = [pad](float v) { pad->stiffness = v; };
    sliders->emplace<Label>("damping", 1.3f);
    auto* dmp = sliders->emplace<Slider>(2.0f, 40.0f, pad->damping);
    dmp->prefW = 200;
    dmp->onChange = [pad](float v) { pad->damping = v; };

    return root;
}

/// The scene registry. Additional scenes are appended here by their tickets
/// (#31–#37).
inline const std::vector<Scene>& registry() {
    static const std::vector<Scene> r = {
        {"theming", "Layout & theming", &buildTheming, false},
        {"controls", "Buttons & controls", &buildControls, true},
        {"layout", "Layout — the flex Box", &buildLayout, true},
        {"animation", "Animation — springs", &buildAnimation, true},
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
