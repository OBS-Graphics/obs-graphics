#include "element.h"

#include <cmath>
#include <algorithm>

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

void Element::Render(cairo_t* ctx, const AnimatedTransform& xf,
                     const AnimatedTransform* maskXf) const
{
    ApplyClipping(ctx, xf, maskXf);

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
        case ElementType::Rectangle: {
            RoundRect(ctx, 0, 0, bounds.width, bounds.height, cornerRadius);

            bool hasFill   = fill.Valid();
            bool hasStroke = stroke.Valid() && strokeWidth > 0.0f;

            if (hasFill) {
                fill.Apply(ctx, bounds.width, bounds.height);
                if (hasStroke)
                    cairo_fill_preserve(ctx);
                else
                    cairo_fill(ctx);
            }

            if (hasStroke) {
                stroke.Apply(ctx, bounds.width, bounds.height);
                cairo_set_line_width(ctx, strokeWidth);
                cairo_stroke(ctx);
            }
            break;
        }

        case ElementType::Text: {
            std::string textXf = text;
            switch (transform) {
                case TextTransform::Lowercase: std::transform(textXf.begin(), textXf.end(), textXf.begin(), ::tolower); break;
                case TextTransform::Uppercase: std::transform(textXf.begin(), textXf.end(), textXf.begin(), ::toupper); break;
                case TextTransform::Capitalize: {
                    bool newWord = true;
                    for (char& c : textXf) {
                        c = ::tolower(c);
                        if (::isspace(c)) {
                            newWord = true;
                        } else if (newWord) {
                            c = ::toupper(c);
                            newWord = false;
                        }
                    }
                } break;
                default: break;
            }

            PangoAttrList* attrs = pango_attr_list_new();
            if (font.isUnderline) {
                pango_attr_list_insert(attrs, pango_attr_underline_new(PANGO_UNDERLINE_SINGLE));
            }
            if (font.isStrikethrough) {
                pango_attr_list_insert(attrs, pango_attr_strikethrough_new(TRUE));
            }

            PangoLayout* layout = pango_cairo_create_layout(ctx);
            pango_layout_set_text(layout, textXf.c_str(), -1);
            pango_layout_set_attributes(layout, attrs);

            PangoFontDescription* fd = pango_font_description_new();
            if (!font.family.empty())
                pango_font_description_set_family(fd, font.family.c_str());
            pango_font_description_set_size(fd, static_cast<int>(font.size * PANGO_SCALE));
            pango_font_description_set_weight(fd, static_cast<PangoWeight>(font.weight));
            if (font.isItalic)
                pango_font_description_set_style(fd, PANGO_STYLE_ITALIC);

            pango_layout_set_font_description(layout, fd);

            static constexpr PangoAlignment kAlignMap[] = {
                PANGO_ALIGN_LEFT, PANGO_ALIGN_CENTER, PANGO_ALIGN_LEFT, PANGO_ALIGN_RIGHT
            };
            pango_layout_set_alignment(layout, kAlignMap[(int)textAlignX]);
            pango_layout_set_justify(layout, textAlignX == HorizontalAlignment::Justify);

            double ox = 0.0, oy = 0.0;

            if (autoScale) {
                pango_layout_set_width(layout, -1);
                PangoRectangle nat;
                pango_layout_get_pixel_extents(layout, &nat, nullptr);
                if (nat.width > 0 && nat.height > 0) {
                    double sx = bounds.width  / nat.width;
                    double sy = bounds.height / nat.height;
                    float newSize = font.size * (float)std::min(sx, sy);
                    pango_font_description_set_size(fd, static_cast<int>(newSize * PANGO_SCALE));
                    pango_layout_set_font_description(layout, fd);
                }
                PangoRectangle ink;
                pango_layout_get_pixel_extents(layout, &ink, nullptr);
                ox = -ink.x;
                oy = -ink.y;
            } else {
                pango_layout_set_width(layout, static_cast<int>(bounds.width * PANGO_SCALE));
                pango_layout_set_wrap(layout, static_cast<PangoWrapMode>(wrapMode));
                if (ellipsize != Ellipsize::None) {
                    pango_layout_set_ellipsize(layout, static_cast<PangoEllipsizeMode>(ellipsize));
                    pango_layout_set_height(layout, static_cast<int>(bounds.height * PANGO_SCALE));
                }

                PangoRectangle ink;
                pango_layout_get_pixel_extents(layout, &ink, nullptr);
                switch (textAlignY) {
                    case VerticalAlignment::Top:
                        oy = -ink.y;
                        break;
                    case VerticalAlignment::Bottom:
                        oy = bounds.height - ink.height - ink.y;
                        break;
                    default:
                        oy = (bounds.height - ink.height) / 2.0 - ink.y;
                        break;
                }
            }

            pango_font_description_free(fd);

            if (fill.Valid()) {
                cairo_move_to(ctx, ox, oy);
                fill.Apply(ctx, bounds.width, bounds.height);
                pango_cairo_show_layout(ctx, layout);
            }

            if (stroke.Valid() && strokeWidth > 0.0f) {
                cairo_move_to(ctx, ox, oy);
                pango_cairo_layout_path(ctx, layout);
                stroke.Apply(ctx, bounds.width, bounds.height);
                cairo_set_line_width(ctx, strokeWidth);
                cairo_stroke(ctx);
            }

            pango_attr_list_unref(attrs);
            g_object_unref(layout);
            break;
        }
    }

    cairo_pop_group_to_source(ctx);
    cairo_paint_with_alpha(ctx, opacity * xf.opacity);
}

void Element::ApplyClipping(cairo_t *ctx, const AnimatedTransform &xf,
                             const AnimatedTransform* maskXf) const {
    const bool isWiping = xf.clipX > 0.0 || xf.clipY > 0.0
                       || xf.clipW < 1.0 || xf.clipH < 1.0;
    const bool hasRadius = cornerRadius[0] > 0 || cornerRadius[1] > 0
                        || cornerRadius[2] > 0 || cornerRadius[3] > 0;

    if (isWiping || hasRadius) {
        RoundRect(ctx, 0, 0, bounds.width, bounds.height, cornerRadius);
        cairo_clip(ctx);
    }

    if (mask != nullptr) {
        Point maskGlobal = mask->GetGlobalPosition();
        Point selfGlobal = GetGlobalPosition();
        float mx = (float)(maskGlobal.x - selfGlobal.x);
        float my = (float)(maskGlobal.y - selfGlobal.y);
        if (maskXf) {
            mx += (float)maskXf->offsetX;
            my += (float)maskXf->offsetY;
        }
        RoundRect(ctx, mx, my, mask->bounds.width, mask->bounds.height, mask->cornerRadius);
        cairo_clip(ctx);
    }

    if (isWiping) {
        cairo_rectangle(ctx,
            xf.clipX * bounds.width,
            xf.clipY * bounds.height,
            xf.clipW * bounds.width,
            xf.clipH * bounds.height);
        cairo_clip(ctx);
    }
}

Point Element::GetGlobalPosition() const
{
    Point result{bounds.x, bounds.y};
    if (parent) {
        Point parentPos = parent->GetGlobalPosition();
        result.x += parentPos.x;
        result.y += parentPos.y;
    }
    return result;
}
