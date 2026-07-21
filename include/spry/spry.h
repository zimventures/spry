#pragma once
// Umbrella header for the toolkit public API.
#include "anim.h"
#include "box.h"
#include "clipboard.h"
#include "color.h"
#include "colorpicker.h"
#include "combo.h"
#include "context.h"
#include "data.h"
#include "input.h"
#include "layout.h"
#include "overlay.h"
#include "renderer.h"
#include "text_edit.h"
#include "textarea.h"
#include "textfield.h"
#include "theme.h"
#include "widget.h"
#include "widgets.h"

/// @file spry.h
/// The umbrella header for Spry's public API. Include this plus **one** renderer
/// backend (`<spry/sdl_renderer.h>` or `<spry/gl_renderer.h>`). Everything lives
/// in `namespace spry`.

/// @mainpage Spry C++ API reference
///
/// This is the generated reference for Spry's public C++ API. For narrative docs —
/// the getting-started tutorial, concept guides, and examples — see the
/// [documentation site](../index.html).
///
/// A consumer includes the umbrella header plus exactly one renderer backend:
/// @code
/// #include <spry/spry.h>          // Context, Widget, Box, widgets, Theme, anim, input
/// #include <spry/sdl_renderer.h>  // OR <spry/gl_renderer.h>
/// using namespace spry;
/// @endcode
///
/// The API is organized into these modules:
/// - @ref context "App / Context & frame loop"
/// - @ref widgets "Widgets & the tree"
/// - @ref theme "Theming & color"
/// - @ref anim "Animation"
/// - @ref input "Input"
/// - @ref renderer "Renderer backends"
///
/// The umbrella header is the contract: if a type is reachable from `<spry/spry.h>`
/// (or a backend header), it's public. See the public-API ADR for the stability
/// contract and what is experimental.

/// @defgroup context App / Context & frame loop
/// @brief `Context` owns the tree, theme, overlays, and interaction state, and
/// drives a frame; `InputEvent` feeds it. See @ref context.h and @ref input.h.

/// @defgroup widgets Widgets & the tree
/// @brief The `Widget` base, layout containers (`Box`/`WrapBox`), layout
/// primitives, and the built-in widget library.

/// @defgroup theme Theming & color
/// @brief `Theme` (the data-driven token bag), the `Color` type, and the core
/// theme-token vocabulary.

/// @defgroup anim Animation
/// @brief `Spring` and easing functions — the toolkit's animation primitives.

/// @defgroup input Input
/// @brief `InputEvent` and the `Key` enum — Spry-native input the host feeds in.

/// @defgroup renderer Renderer backends
/// @brief The abstract `Renderer` interface and the `SdlRenderer` / `GlRenderer`
/// backends widgets draw through.
