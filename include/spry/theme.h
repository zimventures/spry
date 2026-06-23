#pragma once
#include <string>
#include <unordered_map>

#include "color.h"

// Data-driven theming (#211). A theme is a bag of named design tokens — colors
// and metrics (radii/spacing/motion) — that widgets read by role. Themes load
// from a file, swap at runtime, and can be interpolated (animated transitions).
namespace spry {

struct Theme {
    std::unordered_map<std::string, Color> colors;
    std::unordered_map<std::string, float> metrics;

    Color color(const std::string& key, Color fallback = {}) const {
        auto it = colors.find(key);
        return it != colors.end() ? it->second : fallback;
    }
    float metric(const std::string& key, float fallback = 0.0f) const {
        auto it = metrics.find(key);
        return it != metrics.end() ? it->second : fallback;
    }

    // A sensible built-in so a Theme always works without a file.
    static Theme builtinDark();

    // Minimal flat-format loader (no external JSON dep — keeps the decoupling
    // contract tight). Lines: `color <key> r g b [a]` / `metric <key> <value>`,
    // `#` comments. A richer format can come later; the host can also populate a
    // Theme programmatically from its own config.
    static bool loadFromFile(const std::string& path, Theme& out);
};

// Per-token interpolation — the basis for animated theme transitions.
Theme lerp(const Theme& a, const Theme& b, float t);

} // namespace spry
