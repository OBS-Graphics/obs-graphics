#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <concepts>
#include <algorithm>

enum class Easing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

enum class AnimationType {
    None,
    Fade,
    SlideUp, SlideDown, SlideLeft, SlideRight,
    ScaleIn,
    WipeUp, WipeDown, WipeLeft, WipeRight
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

enum class ElementType {
    Rectangle,
    Text,
    Layout
};

enum class LayoutDirection {
    Row,
    Column
};

struct Element {
    std::string id;
    ElementType type;

    AnimationDef inAnimation{}, outAnimation{};

    Rectangle bounds{};
    float rotation{0.0f};

    Paint fill, stroke;
    float strokeWidth{0.0f};
    float cornerRadius[4]{0.0f, 0.0f, 0.0f, 0.0f};

    float opacity{1.0f};

    Element* mask;
    Element* layout;

    LayoutDirection layoutDirection{LayoutDirection::Row};
    float layoutGap{0.0f};

    struct {
        std::string family;
        float size{36.0f};
        bool isBold{false};
        bool isItalic{false};
    } font{};

    std::string text;

    void Render(cairo_t* ctx, const AnimatedTransform& xf) const;
    void ApplyClipping(cairo_t* ctx, const AnimatedTransform& xf) const;
};

enum class GraphicState {
    Hidden,
    AnimatingIn,
    Visible,
    AnimatingOut
};

struct Graphic {
    std::string id;
    std::vector<Element> elements;
    GraphicState state{GraphicState::Hidden};
    double timer{0.0f};

    void TriggerIn() { state = GraphicState::AnimatingIn; timer = 0.0f; }
    void TriggerOut() { state = GraphicState::AnimatingOut; timer = 0.0f; }
};

struct Scene {
    int width{1920}, height{1080};
    std::vector<Graphic> graphics;

    static Scene Load(const std::string& jsonPath);
    static Scene LoadString(const std::string& json);
};

template <typename T>
concept FloatingPoint = std::same_as<T, float> || std::same_as<T, double>;

template <FloatingPoint T>
T ApplyEasing(T t, Easing e) {
    t = std::clamp(t, T(0), T(1));
    switch (e) {
    case Easing::Linear:    return t;
    case Easing::EaseIn:    return t * t;
    case Easing::EaseOut:   return T(1) - (T(1) - t) * (T(1) - t);
    case Easing::EaseInOut:
        return t < T(0.5)
            ? 2 * t * t
            : 1 - std::pow(-2 * t + 2, 2) / 2;
    }
    return t;
}

AnimatedTransform EvaluateAnimation(
    const AnimationDef& def,
    double timer,
    bool isOut,
    const Element& el
);

void EngineTickScene(Scene& scene, float timeStep);
void EngineRenderScene(cairo_t* ctx, Scene& scene);
