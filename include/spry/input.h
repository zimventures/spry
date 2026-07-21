#pragma once

/// @file input.h
/// Spry-native input events + key codes (#216). Spry-native so the toolkit stays
/// decoupled from SDL/ImGui — the host translates its platform events into these
/// and feeds them to `Context::handleEvent`.

namespace spry {

/// @addtogroup input
/// @{

/// Named keys Spry reacts to. Letters are carried for editing chords (select-all,
/// clipboard, undo/redo); only keys bound to shortcuts are enumerated — printable
/// text arrives as `InputEvent::Text` events, not keys.
enum class Key {
    None,
    Tab,
    Enter,
    Escape,
    Space,
    Backspace,
    Delete,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    A,
    C,
    V,
    X,
    Y,
    Z
};

/// One translated input event. The host builds these from its platform events and
/// passes them to `Context::handleEvent`. Which fields are meaningful depends on
/// @ref type.
struct InputEvent {
    /// Event kind. `TextEditing` carries IME pre-edit (composition) text that is
    /// not yet committed; `Text` carries committed UTF-8 the host has finalized.
    enum Type { MouseMove, MouseDown, MouseUp, Wheel, KeyDown, Text, TextEditing };
    Type type = MouseMove;      ///< Which kind of event this is.
    float x = 0;                ///< Mouse X, in Spry coordinates.
    float y = 0;                ///< Mouse Y, in Spry coordinates.
    int button = 0;             ///< Mouse button: 0 = left, 1 = right, 2 = middle.
    float wheel = 0;            ///< Wheel delta (for `Wheel` events).
    Key key = Key::None;        ///< Key (for `KeyDown` events).
    bool shift = false;         ///< Shift modifier held.
    bool ctrl = false;          ///< Ctrl (or Cmd, via the SDL host) modifier held.
    bool alt = false;           ///< Alt modifier held.
    const char* text = nullptr; ///< UTF-8 text (for `Text` / `TextEditing` events).
};

/// @}

} // namespace spry
