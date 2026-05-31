#include "engine.h"
#include "json.hpp"
#include <fstream>

AnimatedTransform EvaluateAnimation(
    const AnimationDef& def,
    double timer,
    bool isOut,
    const Element& el
) {
    AnimatedTransform out{};

    double t = (timer - def.delay) / def.duration;
    double p = ApplyEasing(std::clamp(t, 0.0, 1.0), def.easing);
    if (isOut) p = 1.0f - p;

    switch (def.type) {
        case AnimationType::None: break;
        case AnimationType::Fade: out.opacity = p; break;
        case AnimationType::SlideUp: out.offsetY = (1.0 - p) * el.bounds.height; break;
        case AnimationType::SlideDown: out.offsetY = -(1.0 - p) * el.bounds.height; break;
        case AnimationType::SlideLeft: out.offsetX = (1.0 - p) * el.bounds.width; break;
        case AnimationType::SlideRight: out.offsetX = -(1.0 - p) * el.bounds.width; break;
        case AnimationType::WipeUp: out.clipY = 1.0 - p; out.clipH = p; break;
        case AnimationType::WipeDown: out.clipH = p; break;
        case AnimationType::WipeLeft: out.clipX = 1.0 - p; out.clipW = p; break;
        case AnimationType::WipeRight: out.clipW = p; break;
        case AnimationType::ScaleIn:
            out.opacity = p;
            out.scale = p;
            break;
    }

    return out;
}

void EngineTickScene(Scene& scene, float timeStep) {
    for (auto& g : scene.graphics) {
        if (g.state == GraphicState::Hidden || g.state == GraphicState::Visible) {
            continue;
        }

        g.timer += timeStep;

        bool allDone = true;
        for (const auto& el : g.elements) {
            const auto& def = g.state == GraphicState::AnimatingIn
                ? el.inAnimation : el.outAnimation;
            if (g.timer < def.delay + def.duration) {
                allDone = false;
                break;
            }
        }

        if (allDone) {
            g.state = g.state == GraphicState::AnimatingIn
                ? GraphicState::Visible : GraphicState::Hidden;
        }
    }
}

void EngineRenderScene(cairo_t* ctx, Scene &scene) {
    for (auto& g : scene.graphics) {
        if (g.state == GraphicState::Hidden) continue;

        for (const auto& el : g.elements) {
            bool isOut = g.state == GraphicState::AnimatingOut;
            const auto& def = isOut ? el.outAnimation : el.inAnimation;
            AnimatedTransform xf = EvaluateAnimation(def, g.timer, isOut, el);

            cairo_save(ctx);
            cairo_translate(ctx, el.bounds.x, el.bounds.y);
            el.Render(ctx, xf);
            cairo_restore(ctx);
        }
    }
}

// ── JSON loading ─────────────────────────────────────────────────────────────

using json = nlohmann::json;

static Easing ParseEasing(const std::string& s) {
    if (s == "ease_in")     return Easing::EaseIn;
    if (s == "ease_out")    return Easing::EaseOut;
    if (s == "ease_in_out") return Easing::EaseInOut;
    return Easing::Linear;
}

static AnimationType ParseAnimationType(const std::string& s) {
    if (s == "fade")        return AnimationType::Fade;
    if (s == "slide_up")    return AnimationType::SlideUp;
    if (s == "slide_down")  return AnimationType::SlideDown;
    if (s == "slide_left")  return AnimationType::SlideLeft;
    if (s == "slide_right") return AnimationType::SlideRight;
    if (s == "scale_in")    return AnimationType::ScaleIn;
    if (s == "wipe_up")     return AnimationType::WipeUp;
    if (s == "wipe_down")   return AnimationType::WipeDown;
    if (s == "wipe_left")   return AnimationType::WipeLeft;
    if (s == "wipe_right")  return AnimationType::WipeRight;
    return AnimationType::None;
}

static AnimationDef ParseAnimationDef(const json& j) {
    return {
        .type     = ParseAnimationType(j.value("type", "none")),
        .easing   = ParseEasing(j.value("easing", "linear")),
        .duration = j.value("duration", 0.5f),
        .delay    = j.value("delay", 0.0f),
    };
}

static Paint ParsePaint(const json& v) {
    if (v.is_array() && v.size() >= 3) {
        double r = v[0], g = v[1], b = v[2];
        double a = v.size() >= 4 ? (double)v[3] : 1.0;
        return Paint::Solid(r, g, b, a);
    }

    if (v.is_object()) {
        std::string type = v.value("type", "");
        Paint p = (type == "radial")
            ? Paint::Radial(
                v.value("cx0", 0.0), v.value("cy0", 0.0), v.value("r0", 0.0),
                v.value("cx1", 0.0), v.value("cy1", 0.0), v.value("r1", 1.0))
            : Paint::Linear(
                v.value("x0", 0.0), v.value("y0", 0.0),
                v.value("x1", 1.0), v.value("y1", 0.0));

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

static Element ParseElement(const json& j) {
    Element el;
    el.id   = j.value("id", "");
    el.type = j.value("type", "") == "text" ? ElementType::Text : ElementType::Rectangle;

    el.bounds = {
        j.value("x", 0.0), j.value("y", 0.0),
        j.value("w", 0.0), j.value("h", 0.0)
    };

    if (j.contains("fill"))   el.fill   = ParsePaint(j["fill"]);
    if (j.contains("stroke")) el.stroke = ParsePaint(j["stroke"]);
    if (j.contains("color"))  el.fill   = ParsePaint(j["color"]);

    if (j.contains("corner_radius")) {
        const auto& cr = j["corner_radius"];
        if (cr.is_number()) {
            float r = cr.get<float>();
            el.cornerRadius[0] = el.cornerRadius[1] =
            el.cornerRadius[2] = el.cornerRadius[3] = r;
        } else if (cr.is_array() && cr.size() >= 4) {
            for (int i = 0; i < 4; i++) el.cornerRadius[i] = cr[i];
        }
    }

    el.strokeWidth = j.value("stroke_width", 0.0f);
    el.opacity     = j.value("opacity", 1.0f);
    el.rotation    = j.value("rotation", 0.0f);
    el.text        = j.value("text", "");
    el.font.family = j.value("font_family", "Sans");
    el.font.size   = j.value("font_size", 36.0f);

    if (j.contains("anim_in"))  el.inAnimation  = ParseAnimationDef(j["anim_in"]);
    if (j.contains("anim_out")) el.outAnimation = ParseAnimationDef(j["anim_out"]);

    return el;
}

static Graphic ParseGraphic(const json& j) {
    Graphic g;
    g.id = j.value("id", "");
    if (j.contains("elements") && j["elements"].is_array()) {
        for (const auto& ej : j["elements"])
            g.elements.push_back(ParseElement(ej));
    }
    return g;
}

Scene Scene::LoadString(const std::string& jsonStr) {
    Scene scene;
    auto j = json::parse(jsonStr);
    if (j.contains("graphics") && j["graphics"].is_array()) {
        for (const auto& gj : j["graphics"])
            scene.graphics.push_back(ParseGraphic(gj));
    }
    return scene;
}

Scene Scene::Load(const std::string& jsonPath) {
    std::ifstream f(jsonPath);
    return LoadString(std::string(
        std::istreambuf_iterator<char>(f),
        std::istreambuf_iterator<char>()
    ));
}
