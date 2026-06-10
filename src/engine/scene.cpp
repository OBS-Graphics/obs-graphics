#include "scene.h"

#include "json.hpp"
#include <fstream>

// ── JSON loading ─────────────────────────────────────────────────────────────

using json = nlohmann::json;

static Easing ParseEasing(const std::string& s)
{
    if (s == "ease_in")
        return Easing::EaseIn;
    if (s == "ease_out")
        return Easing::EaseOut;
    if (s == "ease_in_out")
        return Easing::EaseInOut;
    return Easing::Linear;
}

static AnimationType ParseAnimationType(const std::string& s)
{
    if (s == "fade")
        return AnimationType::Fade;
    if (s == "slide_up")
        return AnimationType::SlideUp;
    if (s == "slide_down")
        return AnimationType::SlideDown;
    if (s == "slide_left")
        return AnimationType::SlideLeft;
    if (s == "slide_right")
        return AnimationType::SlideRight;
    if (s == "scale_in")
        return AnimationType::ScaleIn;
    if (s == "wipe_up")
        return AnimationType::WipeUp;
    if (s == "wipe_down")
        return AnimationType::WipeDown;
    if (s == "wipe_left")
        return AnimationType::WipeLeft;
    if (s == "wipe_right")
        return AnimationType::WipeRight;
    return AnimationType::None;
}

static AnimationDef ParseAnimationDef(const json& j)
{
    return {
        .type = ParseAnimationType(j.value("type", "none")),
        .easing = ParseEasing(j.value("easing", "linear")),
        .duration = j.value("duration", 0.5f),
        .delay = j.value("delay", 0.0f),
    };
}

static FontWeight ParseFontWeight(const std::string& s)
{
    if (s == "thin")
        return FontWeight::Thin;
    if (s == "ultrathin")
        return FontWeight::UltraThin;
    if (s == "ultralight")
        return FontWeight::UltraLight;
    if (s == "semilight")
        return FontWeight::SemiLight;
    if (s == "light")
        return FontWeight::Light;
    if (s == "book")
        return FontWeight::Book;
    if (s == "medium")
        return FontWeight::Medium;
    if (s == "semibold")
        return FontWeight::SemiBold;
    if (s == "bold")
        return FontWeight::Bold;
    if (s == "ultrabold")
        return FontWeight::UltraBold;
    if (s == "heavy")
        return FontWeight::Heavy;
    if (s == "ultraheavy")
        return FontWeight::UltraHeavy;
    return FontWeight::Normal;
}

static HorizontalAlignment ParseHAlignment(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "center")
        return HorizontalAlignment::Center;
    if (s == "far" || s == "right" || s == "end")
        return HorizontalAlignment::Right;
    if (s == "justify")
        return HorizontalAlignment::Justify;
    return HorizontalAlignment::Left;
}

static VerticalAlignment ParseVAlignment(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "bottom" || s == "end" || s == "down")
        return VerticalAlignment::Bottom;
    if (s == "center" || s == "middle")
        return VerticalAlignment::Middle;
    return VerticalAlignment::Top;
}

static Ellipsize ParseEllipsize(const std::string& s)
{
    if (s == "start")
        return Ellipsize::Start;
    if (s == "middle")
        return Ellipsize::Middle;
    if (s == "end")
        return Ellipsize::End;
    return Ellipsize::None;
}

static WrapMode ParseWrapMode(const std::string& s)
{
    if (s == "char")
        return WrapMode::Char;
    if (s == "word_char")
        return WrapMode::WordChar;
    return WrapMode::Word;
}

static TextTransform ParseTextTransform(const std::string& s)
{
    if (s == "capitalize")
        return TextTransform::Capitalize;
    if (s == "uppercase")
        return TextTransform::Uppercase;
    if (s == "lowercase")
        return TextTransform::Lowercase;
    return TextTransform::None;
}

static Paint ParsePaint(const json& v)
{
    if (v.is_array() && v.size() >= 3) {
        double r = v[0], g = v[1], b = v[2];
        double a = v.size() >= 4 ? (double)v[3] : 1.0;
        return Paint::Solid(r, g, b, a);
    }

    if (v.is_object()) {
        std::string type = v.value("type", "");
        Paint p = (type == "radial")
                      ? Paint::Radial(v.value("cx0", 0.0), v.value("cy0", 0.0), v.value("r0", 0.0),
                                      v.value("cx1", 0.0), v.value("cy1", 0.0), v.value("r1", 1.0))
                      : Paint::Linear(v.value("x0", 0.0), v.value("y0", 0.0), v.value("x1", 1.0),
                                      v.value("y1", 0.0));

        if (v.contains("stops") && v["stops"].is_array()) {
            for (const auto& s : v["stops"]) {
                double offset = s.value("offset", 0.0);
                const auto& c = s.at("color");
                double r = c[0], g = c[1], b = c[2];
                double a = c.size() >= 4 ? (double)c[3] : 1.0;
                p.AddStop(offset, r, g, b, a);
            }
        }
        return p;
    }

    return Paint{};
}

static Element ParseElement(const json& j)
{
    Element el;
    el.id = j.value("id", "");
    std::string t = j.value("type", "");
    el.type = (t == "text") ? ElementType::Text : ElementType::Rectangle;

    el.bounds = {j.value("x", 0.0), j.value("y", 0.0), j.value("w", 0.0), j.value("h", 0.0)};

    if (j.contains("fill"))
        el.fill = ParsePaint(j["fill"]);
    if (j.contains("stroke"))
        el.stroke = ParsePaint(j["stroke"]);
    if (j.contains("color"))
        el.fill = ParsePaint(j["color"]);

    if (j.contains("corner_radius")) {
        const auto& cr = j["corner_radius"];
        if (cr.is_number()) {
            float r = cr.get<float>();
            el.cornerRadius[0] = el.cornerRadius[1] = el.cornerRadius[2] = el.cornerRadius[3] = r;
        } else if (cr.is_array() && cr.size() >= 4) {
            for (int i = 0; i < 4; i++)
                el.cornerRadius[i] = cr[i];
        }
    }

    el.zOrder = j.value("z_order", 0);
    el.strokeWidth = j.value("stroke_width", 0.0f);
    el.opacity = j.value("opacity", 1.0f);
    el.rotation = j.value("rotation", 0.0f);
    el.text = j.value("text", "");
    el.font.family = j.value("font_family", "Sans");
    el.font.size = j.value("font_size", 36.0f);
    el.font.isItalic = j.value("font_italic", false);
    el.font.isUnderline = j.value("font_underline", false);
    el.font.isStrikethrough = j.value("font_strikethrough", false);
    el.font.weight = ParseFontWeight(j.value("font_weight", "normal"));
    el.autoScale = j.value("auto_scale", false);
    el.textAlignX = ParseHAlignment(j.value("text_align_x", "left"));
    el.textAlignY = ParseVAlignment(j.value("text_align_y", "top"));
    el.ellipsize = ParseEllipsize(j.value("ellipsize", "none"));
    el.wrapMode = ParseWrapMode(j.value("wrap", "word"));
    el.transform = ParseTextTransform(j.value("text_transform", "none"));

    if (j.contains("anim_in"))
        el.inAnimation = ParseAnimationDef(j["anim_in"]);
    if (j.contains("anim_out"))
        el.outAnimation = ParseAnimationDef(j["anim_out"]);

    el.fitToChildren = j.value("fit_to_children", false);
    if (j.contains("children_padding") && j["children_padding"].is_array() &&
        j["children_padding"].size() >= 4) {
        for (int i = 0; i < 4; ++i)
            el.childrenPadding[i] = j["children_padding"][i].get<float>();
    }

    return el;
}

static Graphic ParseGraphic(const json& j)
{
    Graphic g;
    g.id = j.value("id", "");
    g.zOrder = j.value("z_order", 0);

    struct PendingRef {
        size_t idx;
        std::string maskId;
        std::string parentId;
    };
    std::vector<PendingRef> pending;

    if (j.contains("elements") && j["elements"].is_array()) {
        for (const auto& ej : j["elements"]) {
            pending.push_back({.idx = g.elements.size(),
                               .maskId = ej.value("mask", ""),
                               .parentId = ej.value("parent", "")});
            g.elements.push_back(ParseElement(ej));
        }
    }

    std::unordered_map<std::string, size_t> idMap;
    for (size_t i = 0; i < g.elements.size(); i++)
        idMap[g.elements[i].id] = i;

    for (auto& ref : pending) {
        auto& el = g.elements[ref.idx];

        if (!ref.maskId.empty()) {
            auto it = idMap.find(ref.maskId);
            if (it != idMap.end())
                el.mask = &g.elements[it->second];
        }

        if (!ref.parentId.empty()) {
            auto it = idMap.find(ref.parentId);
            if (it != idMap.end()) {
                el.parent = &g.elements[it->second];
                el.parent->children.push_back(&el);
            }
        }
    }

    return g;
}

Scene Scene::LoadString(const std::string& jsonStr)
{
    Scene scene;
    auto j = json::parse(jsonStr);
    scene.width = j.value("width", 1920);
    scene.height = j.value("height", 1080);
    if (j.contains("graphics") && j["graphics"].is_array()) {
        for (const auto& gj : j["graphics"])
            scene.graphics.push_back(ParseGraphic(gj));
    }
    return scene;
}

void Scene::Tick(float timeStep)
{
    for (auto& g : graphics) {
        g.Tick(timeStep);
    }
}

void Scene::Render(cairo_t* ctx) const
{
    std::vector<size_t> gOrder(graphics.size());
    std::iota(gOrder.begin(), gOrder.end(), 0);
    std::stable_sort(gOrder.begin(), gOrder.end(),
                     [&](size_t a, size_t b) { return graphics[a].zOrder < graphics[b].zOrder; });

    for (size_t gi : gOrder) {
        const auto& g = graphics[gi];
        g.Render(ctx);
    }
}

Graphic& Scene::GetById(const std::string& id)
{
    for (auto& g : graphics) {
        if (g.id == id)
            return g;
    }
    throw std::runtime_error("Graphic with id '" + id + "' not found");
}

Scene Scene::Load(const std::string& jsonPath)
{
    std::ifstream f(jsonPath);
    return LoadString(
        std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()));
}
