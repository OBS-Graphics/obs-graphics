#pragma once

#include <cmath>
#include <cairo/cairo.h>
#include <utility>

template <typename T>
struct InterpolationVariable {
    virtual T Lerp(const T& o, double t) = 0;
};

struct Rectangle : public InterpolationVariable<Rectangle> {
    double x, y, width, height;

    Rectangle() = default;
    Rectangle(double x, double y, double width, double height)
        : x(x), y(y), width(width), height(height) {}

    inline Rectangle Lerp(const Rectangle& o, double t) {
        return Rectangle(
            std::lerp(x, o.x, t),
            std::lerp(y, o.y, t),
            std::lerp(width, o.width, t),
            std::lerp(height, o.height, t)
        );
    }
};

struct Paint {
    cairo_pattern_t* pattern = nullptr;

    Paint() = default;

    // Solid RGBA
    inline static Paint Solid(double r, double g, double b, double a = 1.0) {
        Paint p;
        p.pattern = cairo_pattern_create_rgba(r, g, b, a);
        return p;
    }

    // Linear gradient (x0,y0) → (x1,y1) — add stops after
    inline static Paint Linear(double x0, double y0, double x1, double y1) {
        Paint p;
        p.pattern = cairo_pattern_create_linear(x0, y0, x1, y1);
        return p;
    }

    // Radial gradient
    inline static Paint Radial(double cx0, double cy0, double r0,
                        double cx1, double cy1, double r1) {
        Paint p;
        p.pattern = cairo_pattern_create_radial(cx0, cy0, r0, cx1, cy1, r1);
        return p;
    }

    inline void AddStop(double offset, double r, double g, double b, double a = 1.0) {
        cairo_pattern_add_color_stop_rgba(pattern, offset, r, g, b, a);
    }

    // ── RAII ─────────────────────────────────────────────────────────────────

    ~Paint() { if (pattern) cairo_pattern_destroy(pattern); }

    // Move-only — patterns are ref-counted but we keep it simple
    inline Paint(Paint &&o) noexcept : pattern(o.pattern) { o.pattern = nullptr; }
    inline Paint &operator=(Paint &&o) noexcept {
        if (this != &o) {
            if (pattern) cairo_pattern_destroy(pattern);
            pattern   = o.pattern;
            o.pattern = nullptr;
        }
        return *this;
    }

    // Deep copy via cairo_pattern_reference if you need it
    inline Paint(const Paint &o) : pattern(o.pattern) {
        if (pattern) cairo_pattern_reference(pattern);
    }

    inline Paint &operator=(const Paint &o) {
        if (this != &o) {
            if (pattern) cairo_pattern_destroy(pattern);
            pattern = o.pattern;
            if (pattern) cairo_pattern_reference(pattern);
        }
        return *this;
    }

    inline bool Valid() const { return pattern != nullptr; }

    // Apply to a Cairo context as source
    inline void Apply(cairo_t *cr) const {
        if (pattern) cairo_set_source(cr, pattern);
    }
};