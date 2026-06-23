#pragma once
#include <cmath>

// Animation primitives — the differentiator (#210). Graduated from the #204 spike.
// Widgets own their own animation state; no global side-tables.
namespace spry {

inline float easeOutCubic(float t) { return 1.0f - std::pow(1.0f - t, 3.0f); }

inline float easeOutBack(float t) {
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
}

// A damped spring: set a target and it eases there; kick() injects velocity for a
// bouncy pulse. Sub-stepped for stability at low frame rates.
struct Spring {
    float value = 0, vel = 0, target = 0;
    float stiffness = 260.0f, damping = 18.0f;

    void step(float dt) {
        const int n = 4;
        float h = dt / n;
        for (int i = 0; i < n; ++i) {
            float a = stiffness * (target - value) - damping * vel;
            vel += a * h;
            value += vel * h;
        }
    }
    void kick(float v) { vel += v; }
};

} // namespace spry
