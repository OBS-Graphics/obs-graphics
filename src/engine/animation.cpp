#include "animation.h"

#include "element.h"

namespace animation {
AnimatedTransform EvaluateAnimation(const AnimationDef& def, double timer, bool isOut,
                                    const Element& el)
{
    AnimatedTransform out{};

    double t = (timer - def.delay) / def.duration;
    double p = animation::ApplyEasing(std::clamp(t, 0.0, 1.0), def.easing);
    if (isOut)
        p = 1.0f - p;

    switch (def.type) {
    case AnimationType::None:
        break;
    case AnimationType::Fade:
        out.opacity = p;
        break;
    case AnimationType::SlideUp:
        out.offsetY = (1.0 - p) * el.bounds.height;
        out.opacity = p;
        break;
    case AnimationType::SlideDown:
        out.offsetY = -(1.0 - p) * el.bounds.height;
        out.opacity = p;
        break;
    case AnimationType::SlideLeft:
        out.offsetX = (1.0 - p) * el.bounds.width;
        out.opacity = p;
        break;
    case AnimationType::SlideRight:
        out.offsetX = -(1.0 - p) * el.bounds.width;
        out.opacity = p;
        break;
    case AnimationType::WipeUp:
        out.clipY = 1.0 - p;
        out.clipH = p;
        break;
    case AnimationType::WipeDown:
        out.clipH = p;
        break;
    case AnimationType::WipeLeft:
        out.clipX = 1.0 - p;
        out.clipW = p;
        break;
    case AnimationType::WipeRight:
        out.clipW = p;
        break;
    case AnimationType::ScaleIn:
        out.opacity = p;
        out.scale = p;
        break;
    }

    return out;
}
} // namespace animation