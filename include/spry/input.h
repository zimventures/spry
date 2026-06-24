#pragma once

// Input events + key codes (#216). Spry-native so the toolkit stays decoupled
// from SDL/ImGui — the host translates platform events into these.
namespace spry {

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
    // Letters carried for editing chords (select-all, clipboard, undo/redo). Only
    // the ones bound to shortcuts are enumerated — printable text arrives as Text
    // events, not keys.
    A,
    C,
    V,
    X,
    Y,
    Z
};

struct InputEvent {
    // TextEditing carries IME pre-edit (composition) text that is not yet
    // committed; Text carries committed UTF-8 the host has finalized.
    enum Type { MouseMove, MouseDown, MouseUp, Wheel, KeyDown, Text, TextEditing };
    Type type = MouseMove;
    float x = 0, y = 0;         // mouse position in Spry coordinates
    int button = 0;             // 0 = left, 1 = right, 2 = middle
    float wheel = 0;
    Key key = Key::None;
    bool shift = false, ctrl = false, alt = false;
    const char* text = nullptr; // UTF-8 (Text / TextEditing events)
};

} // namespace spry
