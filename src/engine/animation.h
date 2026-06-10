#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>

struct Element;

template <typename T>
concept FloatingPoint = std::same_as<T, float> || std::same_as<T, double>;

enum class Easing {
    Linear = 0, EaseIn, EaseOut, EaseInOut,
    EaseInCubic, EaseOutCubic, EaseInOutCubic,
    EaseInExpo,  EaseOutExpo,  EaseInOutExpo,
    EaseInBack,  EaseOutBack,  EaseInOutBack,
    EaseInElastic, EaseOutElastic, EaseInOutElastic,
};

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
    case Easing::EaseInCubic:
        return t * t * t;
    case Easing::EaseOutCubic:
        return T(1) - std::pow(T(1) - t, 3);
    case Easing::EaseInOutCubic:
        return t < T(0.5) ? 4 * t * t * t : T(1) - std::pow(-2 * t + 2, 3) / 2;
    case Easing::EaseInExpo:
        return t == 0 ? T(0) : std::pow(T(2), 10 * t - 10);
    case Easing::EaseOutExpo:
        return t == 1 ? T(1) : T(1) - std::pow(T(2), -10 * t);
    case Easing::EaseInOutExpo:
        return t == 0   ? T(0)
               : t == 1 ? T(1)
               : t < T(0.5) ? std::pow(T(2), 20 * t - 10) / 2
                             : (T(2) - std::pow(T(2), -20 * t + 10)) / 2;
    case Easing::EaseInBack:
        return T(2.70158) * t * t * t - T(1.70158) * t * t;
    case Easing::EaseOutBack: {
        T s = t - 1;
        return T(1) + T(2.70158) * s * s * s + T(1.70158) * s * s;
    }
    case Easing::EaseInOutBack:
        return t < T(0.5)
                   ? (std::pow(2 * t, 2) * ((T(3.5949095)) * 2 * t - T(2.5949095))) / 2
                   : (std::pow(2 * t - 2, 2) * (T(3.5949095) * (2 * t - 2) + T(2.5949095)) + 2) / 2;
    case Easing::EaseInElastic:
        return t == 0 ? T(0)
               : t == 1
                   ? T(1)
                   : -std::pow(T(2), 10 * t - 10) * std::sin((10 * t - 10.75) * T(2 * M_PI / 3));
    case Easing::EaseOutElastic:
        return t == 0 ? T(0)
               : t == 1
                   ? T(1)
                   : std::pow(T(2), -10 * t) * std::sin((10 * t - 0.75) * T(2 * M_PI / 3)) + T(1);
    case Easing::EaseInOutElastic:
        return t == 0   ? T(0)
               : t == 1 ? T(1)
               : t < T(0.5)
                   ? -(std::pow(T(2), 20 * t - 10) * std::sin((20 * t - 11.125) * T(2 * M_PI / 4.5))) / 2
                   :  (std::pow(T(2), -20 * t + 10) * std::sin((20 * t - 11.125) * T(2 * M_PI / 4.5))) / 2 + T(1);
    }
    return t;
}

AnimatedTransform EvaluateAnimation(const AnimationDef& def, double timer, bool isOut,
                                    const Element& el);
} // namespace animation
