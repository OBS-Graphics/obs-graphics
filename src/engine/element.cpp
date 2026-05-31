#include "engine.h"
#include <pango/pangocairo.h>
#include <cmath>

static void RoundRect(cairo_t* ctx, double x, double y, double w, double h,
                      const float r[4])
{
    double tl = r[0], tr = r[1], br = r[2], bl = r[3];
    cairo_new_path(ctx);
    cairo_arc(ctx, x + tl,     y + tl,      tl, M_PI,     3*M_PI/2);
    cairo_arc(ctx, x + w - tr, y + tr,      tr, 3*M_PI/2, 0);
    cairo_arc(ctx, x + w - br, y + h - br,  br, 0,        M_PI/2);
    cairo_arc(ctx, x + bl,     y + h - bl,  bl, M_PI/2,   M_PI);
    cairo_close_path(ctx);
}

void Element::Render(cairo_t* ctx, const AnimatedTransform& xf) const
{
    ApplyClipping(ctx, xf);

    // Slide offset
    cairo_translate(ctx, xf.offsetX, xf.offsetY);

    // Scale and rotation around element center
    double cx = bounds.width  / 2.0;
    double cy = bounds.height / 2.0;

    if (xf.scale != 1.0) {
        cairo_translate(ctx,  cx,  cy);
        cairo_scale(ctx, xf.scale, xf.scale);
        cairo_translate(ctx, -cx, -cy);
    }

    if (rotation != 0.0f) {
        cairo_translate(ctx,  cx,  cy);
        cairo_rotate(ctx, rotation * M_PI / 180.0);
        cairo_translate(ctx, -cx, -cy);
    }

    // Render into a temporary group so we can apply the combined opacity
    cairo_push_group(ctx);

    switch (type) {
        case ElementType::Layout: break; // Layout is just for Flex layout
        case ElementType::Rectangle: {
            RoundRect(ctx, 0, 0, bounds.width, bounds.height, cornerRadius);

            bool hasFill   = fill.Valid();
            bool hasStroke = stroke.Valid() && strokeWidth > 0.0f;

            if (hasFill) {
                fill.Apply(ctx);
                if (hasStroke)
                    cairo_fill_preserve(ctx);
                else
                    cairo_fill(ctx);
            }

            if (hasStroke) {
                stroke.Apply(ctx);
                cairo_set_line_width(ctx, strokeWidth);
                cairo_stroke(ctx);
            }
            break;
        }

        case ElementType::Text: {
            cairo_move_to(ctx, 0, 0);

            PangoLayout* layout = pango_cairo_create_layout(ctx);
            pango_layout_set_text(layout, text.c_str(), -1);
            pango_layout_set_width(layout, static_cast<int>(bounds.width * PANGO_SCALE));

            PangoFontDescription* fd = pango_font_description_new();
            if (!font.family.empty())
                pango_font_description_set_family(fd, font.family.c_str());
            pango_font_description_set_size(fd, static_cast<int>(font.size * PANGO_SCALE));
            if (font.isBold)
                pango_font_description_set_weight(fd, PANGO_WEIGHT_BOLD);
            if (font.isItalic)
                pango_font_description_set_style(fd, PANGO_STYLE_ITALIC);
            pango_layout_set_font_description(layout, fd);
            pango_font_description_free(fd);

            if (fill.Valid()) {
                fill.Apply(ctx);
                pango_cairo_show_layout(ctx, layout);
            }

            if (stroke.Valid() && strokeWidth > 0.0f) {
                cairo_move_to(ctx, 0, 0);
                pango_cairo_layout_path(ctx, layout);
                stroke.Apply(ctx);
                cairo_set_line_width(ctx, strokeWidth);
                cairo_stroke(ctx);
            }

            g_object_unref(layout);
            break;
        }
    }

    cairo_pop_group_to_source(ctx);
    cairo_paint_with_alpha(ctx, opacity * xf.opacity);
}

void Element::ApplyClipping(cairo_t *ctx, const AnimatedTransform &xf) const {
    // Clip to the rounded shape, then intersect with the wipe rect
    RoundRect(ctx, 0, 0, bounds.width, bounds.height, cornerRadius);
    cairo_clip(ctx);

    cairo_rectangle(ctx,
        xf.clipX * bounds.width,
        xf.clipY * bounds.height,
        xf.clipW * bounds.width,
        xf.clipH * bounds.height);
    cairo_clip(ctx);
}
