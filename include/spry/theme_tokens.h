#pragma once
#include <array>

/// @file theme_tokens.h
/// The core theme-token vocabulary (#321): the color and metric names Spry's
/// built-in widgets read from the active `Theme`. Widgets reference these
/// constants instead of ad-hoc string literals, so the vocabulary has one
/// authoritative, documented home. A `Theme` should define them; when a token is
/// absent a widget uses its own fallback (missing tokens degrade quietly rather
/// than crash). Hosts may define any number of extra custom tokens beyond these.

namespace spry {
/// @addtogroup theme
/// @{

/// Core theme-token name constants. Use these with `Theme::color` / `Theme::metric`.
namespace tokens {

// ── Colors ────────────────────────────────────────────────────────────────────
/// Window background. The host reads this for the renderer's `beginFrame()` clear color.
inline constexpr const char* Background = "background";
/// Primary panel / control surface fill.
inline constexpr const char* Surface = "surface";
/// Recessed / secondary surface — slider & scrollbar tracks, alternating rows, insets.
inline constexpr const char* SurfaceAlt = "surfaceAlt";
/// Accent / brand color — selection, focus rings, active controls, sliders, toggles.
inline constexpr const char* Accent = "accent";
/// Text drawn on top of an accent fill (e.g. a selected tab's label).
inline constexpr const char* AccentText = "accentText";
/// Primary text color.
inline constexpr const char* Text = "text";
/// Dimmed / secondary text, placeholders, and inactive borders.
inline constexpr const char* TextDim = "textDim";
/// Scrim drawn behind a `Modal` to dim the content beneath it (uses the color's alpha).
inline constexpr const char* Scrim = "scrim";

// ── Metrics ───────────────────────────────────────────────────────────────────
/// Corner radius for panels and controls, in logical px.
inline constexpr const char* Radius = "radius";

} // namespace tokens

/// The core color tokens, for iteration and validation (see `Theme::missingCoreTokens()`).
inline constexpr std::array<const char*, 8> kCoreColorTokens = {
    tokens::Background, tokens::Surface, tokens::SurfaceAlt, tokens::Accent,
    tokens::AccentText, tokens::Text,   tokens::TextDim,     tokens::Scrim};
/// The core metric tokens, for iteration and validation.
inline constexpr std::array<const char*, 1> kCoreMetricTokens = {tokens::Radius};

/// @}
} // namespace spry
