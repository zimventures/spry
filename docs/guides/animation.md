# Animation

Animation is a first-class primitive in Spry, not a bolt-on. Widgets own their
own animation state; the building block is `Spring`, a damped spring you `step(dt)`
each frame and read `.value` from as it eases toward a `target`. There are also
one-shot easing functions (`easeOutCubic`, `easeOutBack`), and `Card`, `Toggle`,
and the theme crossfade all use `Spring` internally.

!!! info "Full guide coming in [#6](https://github.com/zimventures/spry/issues/6)"
    For now, see [Getting started §6](../getting-started.md#6-animation) and the
    [`anim.h`](https://github.com/zimventures/spry/blob/main/include/spry/anim.h)
    [API reference](../api/index.html).
