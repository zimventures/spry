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

// ── animation: springs (drag + kick) and easings (#32) ──
// A draggable 2D Spring, a kick() velocity pulse, an easeOutCubic-vs-easeOutBack
// track, and a hover-lift Card — the pieces of anim.h, made tangible.
class SpringPad : public Widget {
public:
    Spring sx, sy;             // 2D position springs (value chases target)
    float stiffness = 200.0f;
    float damping = 16.0f;

    Size measure(Renderer&, float availW, float) override {
        // Honor explicit prefs (like other leaf widgets); else fill width with a
        // modest height that grow=1 expands (so siblings aren't starved).
        return Size{prefW >= 0 ? prefW : (availW > 0 ? availW : 480.0f), prefH >= 0 ? prefH : 200.0f};
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
        setTarget(x, y);
        return true;
    }
    void onMouseDrag(float x, float y) override { setTarget(x, y); } // capture continues off-widget
    void kick() {
        sx.kick(-360.0f); // a velocity pulse — the dot lurches, then springs back
        sy.kick(-240.0f);
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
    static float clamp(float v, float hi) { return v < 0 ? 0 : (v > hi ? hi : v); }
    void setTarget(float x, float y) { // clamp inside the pad (drag capture runs off-widget)
        sx.target = clamp(x - rect.x, rect.w);
        sy.target = clamp(y - rect.y, rect.h);
    }
    bool inited_ = false;
};

// One-shot easing tracks: press Play and a dot tweens across using easeOutCubic
// (top) and easeOutBack (bottom, overshoots).
class EasingRow : public Widget {
public:
    Size measure(Renderer&, float, float) override {
        // Modest preferred width so grow=1 (not the preferred size) fills the row,
        // leaving room for the Play button beside it.
        return Size{prefW >= 0 ? prefW : 260.0f, 64.0f};
    }
    void update(float dt) override {
        if (t_ < 1.0f) t_ = std::min(1.0f, t_ + dt * 1.4f);
    }
    void play() { t_ = 0.0f; }
    void paint(Renderer& r, const Theme& th) override {
        Color track = th.color(tokens::SurfaceAlt, {32, 34, 48});
        Color acc = th.color(tokens::Accent, {96, 126, 205});
        Color txt = th.color(tokens::TextDim, {140, 144, 160});
        float gutter = 108.0f;
        float x0 = rect.x + gutter, w = rect.w - gutter - 46.0f; // headroom for easeOutBack overshoot
        const char* labels[2] = {"easeOutCubic", "easeOutBack"};
        for (int i = 0; i < 2; ++i) {
            float ry = rect.y + 14.0f + i * 28.0f;
            r.fillRect(x0, ry + 6.0f, w, 3.0f, track);
            float e = i == 0 ? easeOutCubic(t_) : easeOutBack(t_);
            r.fillRoundedRect(x0 + e * w, ry + 7.0f, 15, 15, 7.5f, acc, acc);
            r.text(rect.x, ry, 1.1f, txt, labels[i]);
        }
    }

private:
    float t_ = 1.0f;
};

inline std::unique_ptr<Widget> buildAnimation() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(22);
    root->spacing = 12;
    root->emplace<Label>("Animation — springs & easing", 2.0f);
    auto* hint = root->emplace<Label>("drag the pad, press Kick, Play the easings, and hover the card", 1.4f);
    hint->role = "textDim";

    auto* pad = root->emplace<SpringPad>();
    pad->grow = 1.0f;

    auto* springRow = root->emplace<Box>();
    springRow->axis = Axis::Row;
    springRow->spacing = 12;
    springRow->cross = Align::Center;
    springRow->emplace<Label>("stiffness", 1.3f);
    auto* stf = springRow->emplace<Slider>(20.0f, 500.0f, pad->stiffness);
    stf->prefW = 170;
    stf->onChange = [pad](float v) { pad->stiffness = v; };
    springRow->emplace<Label>("damping", 1.3f);
    auto* dmp = springRow->emplace<Slider>(2.0f, 40.0f, pad->damping);
    dmp->prefW = 150;
    dmp->onChange = [pad](float v) { pad->damping = v; };
    springRow->emplace<Button>("Kick", [pad] { pad->kick(); });

    auto* easeRow = root->emplace<Box>();
    easeRow->axis = Axis::Row;
    easeRow->spacing = 12;
    easeRow->cross = Align::Center;
    auto* ease = easeRow->emplace<EasingRow>();
    ease->grow = 1.0f;
    easeRow->emplace<Button>("Play", [ease] { ease->play(); });

    auto* card = root->emplace<Card>("hover me — the Card lifts on a Spring");
    card->prefH = 56;

    return root;
}

// ── text: FreeType + HarfBuzz shaping (#33) ──
inline std::unique_ptr<Widget> buildText() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 14;
    root->emplace<Label>("Text — FreeType + HarfBuzz shaping", 2.0f);
    auto* hint = root->emplace<Label>("scale the sample and type below — ligatures & metrics shape live", 1.4f);
    hint->role = "textDim";

    auto* panel = root->emplace<Panel>();
    auto* pbox = panel->emplace<Box>();
    pbox->axis = Axis::Column;
    pbox->padding = Edges(18);
    pbox->spacing = 14;
    pbox->cross = Align::Start;
    auto* sample = pbox->emplace<Label>("-> => != == >= <= |> <| :: www  fi ff ffi", 2.0f);
    auto* scaleRow = pbox->emplace<Box>();
    scaleRow->axis = Axis::Row;
    scaleRow->spacing = 12;
    scaleRow->cross = Align::Center;
    scaleRow->emplace<Label>("scale", 1.2f);
    auto* sl = scaleRow->emplace<Slider>(0.9f, 3.2f, 2.0f);
    sl->prefW = 280;
    sl->onChange = [sample](float v) { sample->scale = v; };

    root->emplace<Paragraph>(
        "Paragraph word-wraps to the available width, with proportional metrics from FreeType and "
        "shaping from HarfBuzz — not fixed-cell blitting. Its height grows with the wrapped line count.",
        1.3f);

    auto* tf = root->emplace<TextField>(std::string("Type here — watch it shape: -> => www"));
    tf->prefW = 480;
    return root;
}

// ── data: virtualized data containers — list, table, tree, tabs (#35) ──
inline std::unique_ptr<Widget> buildData() {
    auto root = std::make_unique<Box>();
    root->axis = Axis::Column;
    root->padding = Edges(24);
    root->spacing = 12;
    root->emplace<Label>("Data widgets — list · table · tree · tabs", 2.0f);
    auto* hint = root->emplace<Label>(
        "click rows · sort headers · toggle tree · switch tabs · scroll · virtualized",
        1.3f);
    hint->role = "textDim";

    auto* body = root->emplace<Box>();
    body->axis = Axis::Row;
    body->spacing = 16;
    body->grow = 1.0f;

    // Left: a hierarchical TreeView (click the disclosure triangle to expand).
    auto* tree = body->emplace<TreeView>();
    tree->prefW = 240;
    {
        auto& prod = tree->addRoot("Production");
        prod.expanded = true;
        auto& web = prod.add("web");
        web.expanded = true;
        web.add("api-gateway");
        web.add("auth-service");
        prod.add("db-primary");
        prod.add("cache");
        auto& stg = tree->addRoot("Staging");
        stg.add("web-canary");
        stg.add("db-replica");
        tree->addRoot("Archive");
        tree->rebuild();
    }

    // Right: a TabBar switching the main area between a sortable Table and a ListView.
    auto* main = body->emplace<Box>();
    main->axis = Axis::Column;
    main->spacing = 10;
    main->grow = 1.0f;

    auto* tabs = main->emplace<TabBar>();
    tabs->tabs = {"Table", "List"};

    auto* table = main->emplace<Table>();
    table->grow = 1.0f;
    table->columns = {{"Host", 2.2f}, {"Port", 1.0f}, {"Conns", 1.0f}, {"Status", 1.4f}};
    const char* hosts[] = {"alpha", "bravo", "charlie", "delta", "echo", "foxtrot",
                           "golf", "hotel", "india", "juliet", "kilo", "lima"};
    for (int i = 0; i < (int)(sizeof(hosts) / sizeof(hosts[0])); ++i) {
        table->rows.push_back({std::string(hosts[i]) + ".example.net",
                               std::to_string(2200 + i * 7),
                               std::to_string((i * 37) % 100),
                               (i % 3 == 0) ? "online" : (i % 3 == 1) ? "idle" : "draining"});
    }
    table->sortCol = 0;

    auto* list = main->emplace<ListView>();
    list->grow = 1.0f;
    list->visible = false; // Table shows first; the tab toggles which is visible.
    for (int i = 1; i <= 40; ++i)
        list->items.push_back("session #" + std::to_string(i) + " — sftp://host-" +
                              std::to_string(1000 + i));

    tabs->onChange = [table, list](int i) {
        table->visible = (i == 0);
        list->visible = (i == 1);
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
        {"animation", "Animation — springs", &buildAnimation, true},
        {"text", "Text — shaping", &buildText, true},
        {"data", "Data — list · table · tree", &buildData, true},
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
