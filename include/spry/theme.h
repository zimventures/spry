#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "color.h"
#include "theme_tokens.h" // the core token vocabulary widgets read (spry::tokens)

/// @file theme.h
/// Data-driven theming (#211). A theme is a bag of named design tokens — colors
/// and metrics (radii/spacing/motion) — that widgets read by role. Themes load
/// from a file, swap at runtime, and can be interpolated (animated transitions).
/// The core token names widgets rely on live in @ref theme_tokens.h.

namespace spry {

/// @addtogroup theme
/// @{

/// A data-driven token bag: named @ref colors and @ref metrics that widgets read
/// by role rather than by hard-coded literal. See the
/// <a href="../guides/theme-tokens/">theme-token reference</a> for the core vocabulary.
struct Theme {
    std::unordered_map<std::string, Color> colors;   ///< Color tokens (name → `Color`).
    std::unordered_map<std::string, float> metrics;  ///< Metric tokens (name → `float`).

    /// Look up a color token, returning `fallback` if it isn't defined.
    Color color(const std::string& key, Color fallback = {}) const {
        auto it = colors.find(key);
        return it != colors.end() ? it->second : fallback;
    }
    /// Look up a metric token, returning `fallback` if it isn't defined.
    float metric(const std::string& key, float fallback = 0.0f) const {
        auto it = metrics.find(key);
        return it != metrics.end() ? it->second : fallback;
    }

    /// Core tokens (@ref theme_tokens.h) this theme does **not** define, so a host
    /// can warn about typos or omissions after loading a theme. Empty means the
    /// theme covers the full core vocabulary; any listed token still works at
    /// runtime via each widget's fallback.
    std::vector<std::string> missingCoreTokens() const {
        std::vector<std::string> missing;
        for (const char* t : kCoreColorTokens)
            if (!colors.count(t)) missing.push_back(t);
        for (const char* t : kCoreMetricTokens)
            if (!metrics.count(t)) missing.push_back(t);
        return missing;
    }

    /// A sensible built-in dark theme, so a `Theme` always works without a file.
    static Theme builtinDark();

    /// Load a theme file over `out`, returning `false` (and leaving `out`
    /// untouched) if the file can't be read. Minimal flat format (no JSON dep):
    /// lines `color <key> r g b [a]` / `metric <key> <value>`, `#` for comments.
    static bool loadFromFile(const std::string& path, Theme& out);
};

/// Per-token interpolation between two themes — the basis for animated
/// transitions. Interpolates the tokens `a` (the outgoing theme) defines.
Theme lerp(const Theme& a, const Theme& b, float t);

/// @}

} // namespace spry
