# Animation

Animation is Spry's differentiator — a first-class primitive, not a bolt-on.
Widgets own their own animation state (there are no global side-tables), and the
core building block is `Spring`, a damped spring. This is why hover lifts and
toggles feel alive without any global animation bookkeeping — and the same
`anim.h` easings drive the [theme crossfade](theming.md).

## Spring

`spry::Spring` is a sub-stepped damped spring. Set a `target`, call `step(dt)` each
frame, and read `value` as it eases in:

```cpp
Spring lift;                       // value, vel, target; tunable stiffness & damping
lift.target = hovered ? 6.0f : 0.0f;
lift.step(dt);                     // integrate one frame
float y = lift.value;              // eases toward target — no easing bookkeeping
```

The fields:

- `value`, `vel`, `target` — current position, velocity, and where it's heading.
- `stiffness` (default `260`) and `damping` (default `18`) — tune the feel; higher
  stiffness snaps faster, higher damping overshoots less.
- `step(float dt)` — advance the spring (internally sub-stepped for stability at
  large `dt`).
- `kick(float v)` — inject velocity for a pulse/nudge without moving the target.

Because a spring is driven by a `target`, you don't manage animation start/stop
state — you just set where a value should be each frame and the spring handles the
motion. That composes cleanly with the retained tree: a widget holds its springs as
members and steps them in `update(dt)`.

## Easing

For one-shot tweens where you drive `t` from `0 → 1` yourself, `anim.h` provides
`easeOutCubic(t)` and `easeOutBack(t)` (the latter overshoots slightly for a
springy "pop"):

```cpp
float k = easeOutBack(progress);   // progress in [0,1]
```

## Where it's used

`Card` and `Toggle` use `Spring` internally (the hover lift, the press, the toggle
knob) — reading their sources is the fastest way to learn the pattern for a custom
animated widget. Not everything is a spring, though: the
[theme crossfade](theming.md) is a time-based `easeOutCubic` tween over
`lerp`-interpolated tokens, not a `Spring`.

![An animated theme crossfade — even the corner radius eases between themes](../assets/theme-swap.gif)

## Stability

`anim.h` (`Spring`, `easeOutCubic`, `easeOutBack`) is **stable public surface**.

## Related

- [Theming & tokens](theming.md) — theme swaps are an `easeOutCubic` tween over
  `lerp`-interpolated tokens (not springs).
- [Layout & the widget tree](layout.md) — `update(dt)` is where widgets step their
  springs each frame.
- [Getting started §6](../getting-started.md#6-animation) — animation in tutorial
  form.
