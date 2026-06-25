/*
obs-graphics — Animated broadcast graphics source for OBS Studio
Copyright (C) 2026 Diego Lopes <diego95lopes@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include "engine/scene.h"
#include "shared-scene.h"
#include <obs-module.h>

#ifdef __APPLE__
#include <cairo.h>
#else
#include <cairo/cairo.h>
#endif

struct GraphicsSource {
    obs_source_t* source = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    bool needs_texture_reset = false;
    cairo_surface_t* surface = nullptr;
    cairo_t* cr = nullptr;
    gs_texture_t* texture = nullptr;
};

static void rebuild_cairo_surface(GraphicsSource* s)
{
    if (s->cr) {
        cairo_destroy(s->cr);
        s->cr = nullptr;
    }
    if (s->surface) {
        cairo_surface_destroy(s->surface);
        s->surface = nullptr;
    }

    s->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, s->width, s->height);
    s->cr = cairo_create(s->surface);
}

// ── OBS source callbacks ─────────────────────────────────────────────────────

static const char* source_get_name(void*)
{
    return "Graphics Source";
}

static void* source_create(obs_data_t*, obs_source_t* source)
{
    auto* s = new GraphicsSource();
    s->source = source;

    obs_video_info ovi;
    if (obs_get_video_info(&ovi)) {
        s->width = ovi.base_width;
        s->height = ovi.base_height;
    } else {
        s->width = 1920;
        s->height = 1080;
    }

    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        g_active_scene.width = (int)s->width;
        g_active_scene.height = (int)s->height;
    }

    rebuild_cairo_surface(s);

    return s;
}

static void source_destroy(void* data)
{
    auto* s = static_cast<GraphicsSource*>(data);

    obs_enter_graphics();
    if (s->texture)
        gs_texture_destroy(s->texture);
    obs_leave_graphics();

    cairo_destroy(s->cr);
    cairo_surface_destroy(s->surface);
    delete s;
}

static void source_video_tick(void* data, float seconds)
{
    auto* s = static_cast<GraphicsSource*>(data);

    obs_video_info ovi;
    if (obs_get_video_info(&ovi) && (ovi.base_width != s->width || ovi.base_height != s->height)) {
        s->width = ovi.base_width;
        s->height = ovi.base_height;
        rebuild_cairo_surface(s);
        s->needs_texture_reset = true;
    }

    std::lock_guard<std::mutex> lock(g_scene_mutex);

    if (!g_scene_loaded)
        return;

    g_active_scene.width = (int)s->width;
    g_active_scene.height = (int)s->height;

    g_active_scene.Tick(seconds);

    cairo_set_operator(s->cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(s->cr);
    cairo_set_operator(s->cr, CAIRO_OPERATOR_OVER);
    g_active_scene.Render(s->cr);
    cairo_surface_flush(s->surface);
}

static void source_render(void* data, gs_effect_t*)
{
    auto* s = static_cast<GraphicsSource*>(data);
    if (!s->width || !s->height)
        return;

    uint8_t* pixels = cairo_image_surface_get_data(s->surface);
    uint32_t stride = cairo_image_surface_get_stride(s->surface);

    if (s->needs_texture_reset || !s->texture) {
        if (s->texture)
            gs_texture_destroy(s->texture);
        s->texture = gs_texture_create(s->width, s->height, GS_BGRA, 1, (const uint8_t**)&pixels,
                                       GS_DYNAMIC);
        s->needs_texture_reset = false;
    } else {
        gs_texture_set_image(s->texture, pixels, stride, false);
    }

    gs_effect_t* effect = obs_get_base_effect(OBS_EFFECT_PREMULTIPLIED_ALPHA);
    gs_eparam_t* image = gs_effect_get_param_by_name(effect, "image");
    gs_effect_set_texture(image, s->texture);
    while (gs_effect_loop(effect, "Draw"))
        gs_draw_sprite(s->texture, 0, s->width, s->height);
}

static uint32_t source_get_width(void* data)
{
    auto* s = static_cast<GraphicsSource*>(data);
    std::lock_guard<std::mutex> lock(g_scene_mutex);
    return g_scene_loaded ? (uint32_t)g_active_scene.width : s->width;
}

static uint32_t source_get_height(void* data)
{
    auto* s = static_cast<GraphicsSource*>(data);
    std::lock_guard<std::mutex> lock(g_scene_mutex);
    return g_scene_loaded ? (uint32_t)g_active_scene.height : s->height;
}

// ── Registration ─────────────────────────────────────────────────────────────

struct obs_source_info gGraphicsSourceInfo = {
    .id = "obs_graphics_source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
    .get_name = source_get_name,
    .create = source_create,
    .destroy = source_destroy,
    .get_width = source_get_width,
    .get_height = source_get_height,
    .video_tick = source_video_tick,
    .video_render = source_render,
    .icon_type = OBS_ICON_TYPE_CUSTOM,
};
