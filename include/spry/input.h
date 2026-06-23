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
    End
};

struct InputEvent {
    enum Type { MouseMove, MouseDown, MouseUp, Wheel, KeyDown, Text };
    Type type = MouseMove;
    float x = 0, y = 0;         // mouse position in Spry coordinates
    int button = 0;             // 0 = left, 1 = right, 2 = middle
    float wheel = 0;
    Key key = Key::None;
    bool shift = false, ctrl = false, alt = false;
    const char* text = nullptr; // UTF-8 (Text events)
};

} // namespace spry
