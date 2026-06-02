#pragma once

#include <string>
#include <pango/pangocairo.h>

#include "types.hpp"
#include "animation.h"

enum class ElementType {
    Rectangle,
    Text
};

enum class FontWeight {
    Normal     = PANGO_WEIGHT_NORMAL,
    Medium     = PANGO_WEIGHT_MEDIUM,
    Bold       = PANGO_WEIGHT_BOLD,
    SemiBold   = PANGO_WEIGHT_SEMIBOLD,
    UltraBold  = PANGO_WEIGHT_ULTRABOLD,
    Light      = PANGO_WEIGHT_LIGHT,
    SemiLight  = PANGO_WEIGHT_SEMILIGHT,
    UltraLight = PANGO_WEIGHT_ULTRALIGHT,
    Thin       = PANGO_WEIGHT_THIN,
    UltraThin  = PANGO_WEIGHT_THIN,
    Heavy      = PANGO_WEIGHT_HEAVY,
    UltraHeavy = PANGO_WEIGHT_ULTRAHEAVY,
    Book       = PANGO_WEIGHT_BOOK
};

enum class Alignment {
    Near = 0,
    Center,
    Far
};

enum class Ellipsize {
    None   = PANGO_ELLIPSIZE_NONE,
    Start  = PANGO_ELLIPSIZE_START,
    Middle = PANGO_ELLIPSIZE_MIDDLE,
    End    = PANGO_ELLIPSIZE_END
};

enum class WrapMode {
    Word     = PANGO_WRAP_WORD,
    Char     = PANGO_WRAP_CHAR,
    WordChar = PANGO_WRAP_WORD_CHAR
};

struct Element {
    std::string id;
    ElementType type;

    int zOrder{0};

    AnimationDef inAnimation{}, outAnimation{};

    Rectangle bounds{0.0, 0.0, 100.0, 100.0};
    float rotation{0.0f};

    Paint fill, stroke;
    float strokeWidth{0.0f};
    float cornerRadius[4]{0.0f, 0.0f, 0.0f, 0.0f};

    float opacity{1.0f};

    Element* mask{nullptr};
    Element* parent{nullptr};

    struct {
        std::string family;
        float size{36.0f};
        FontWeight weight{FontWeight::Normal};
        bool isItalic{false};
    } font{};

    std::string text;
    bool autoScale{false};
    Alignment textAlignX{Alignment::Near};
    Alignment textAlignY{Alignment::Near};
    Ellipsize ellipsize{Ellipsize::None};
    WrapMode wrapMode{WrapMode::Word};

    void Render(cairo_t* ctx, const AnimatedTransform& xf) const;
    void ApplyClipping(cairo_t* ctx, const AnimatedTransform& xf) const;
    Point GetGlobalPosition() const;
};
