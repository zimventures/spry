#pragma once
#include <cmath>

/// @file anim.h
/// Animation primitives — the toolkit's differentiator (#210). Widgets own their
/// own animation state; there are no global side-tables.

namespace spry {

/// @addtogroup anim
/// @{

/// Cubic ease-out easing: fast start, gentle settle. `t` in [0,1].
inline float easeOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }

/// Ease-out with a slight overshoot ("back") for a springy pop. `t` in [0,1].
inline float easeOutBack(float t) {
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

/// A damped spring: set a @ref target and it eases there; @ref kick() injects
/// velocity for a bouncy pulse. Sub-stepped for stability at low frame rates.
///
/// Widgets hold `Spring` members and call @ref step() each frame from `update()`.
struct Spring {
    float value = 0;   ///< Current value (read this each frame).
    float vel = 0;     ///< Current velocity.
    float target = 0;  ///< Where the spring is easing toward.
    float stiffness = 260.0f;  ///< Higher = snaps to the target faster.
    float damping = 18.0f;     ///< Higher = less overshoot / oscillation.

    /// Advance the spring by `dt` seconds (internally sub-stepped ×4).
    void step(float dt) {
        const int n = 4;
        float h = dt / n;
        for (int i = 0; i < n; ++i) {
            float a = stiffness * (target - value) - damping * vel;
            vel += a * h;
            value += vel * h;
        }
    }
    /// Inject velocity `v` without moving the target — a one-off nudge/pulse.
    void kick(float v) { vel += v; }
};

/// @}

} // namespace spry
