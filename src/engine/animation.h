#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

struct Element;

template <typename T>
concept FloatingPoint = std::same_as<T, float> || std::same_as<T, double>;

enum class Easing { Linear = 0, EaseIn, EaseOut, EaseInOut };

enum class AnimationType {
    None = 0,
    Fade,
    SlideUp,
    SlideDown,
    SlideLeft,
    SlideRight,
    ScaleIn,
    WipeUp,
    WipeDown,
    WipeLeft,
    WipeRight
};

struct AnimationDef {
    AnimationType type{AnimationType::None};
    Easing easing{Easing::Linear};
    float duration{0.5f};
    float delay{0.0f};
};

struct AnimatedTransform {
    double offsetX{0.0}, offsetY{0.0};
    double scale{1.0};
    double opacity{1.0};
    double clipX{0.0f};
    double clipY{0.0f};
    double clipW{1.0f};
    double clipH{1.0f};
};

namespace animation {
template <FloatingPoint T>
T ApplyEasing(T t, Easing e)
{
    t = std::clamp(t, T(0), T(1));
    switch (e) {
    case Easing::Linear:
        return t;
    case Easing::EaseIn:
        return t * t;
    case Easing::EaseOut:
        return T(1) - (T(1) - t) * (T(1) - t);
    case Easing::EaseInOut:
        return t < T(0.5) ? 2 * t * t : 1 - std::pow(-2 * t + 2, 2) / 2;
    }
    return t;
}

AnimatedTransform EvaluateAnimation(const AnimationDef& def, double timer, bool isOut,
                                    const Element& el);
} // namespace animation
