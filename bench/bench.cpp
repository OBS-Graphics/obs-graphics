// =============================================================================
// obs-graphics render benchmark + visual-regression harness
//                                            [INSTRUMENTATION — SAFE TO DELETE]
//
// Standalone, offscreen, no OBS instance required. Three modes:
//
//   (no args)        Timing benchmark. Renders representative overlay content to
//                    an in-memory ARGB32 surface and prints per-phase percentiles.
//   golden <dir>     Render the canonical scene set deterministically and write
//                    <dir>/<name>.png (viewable) + <dir>/checksums.txt (exact
//                    64-bit FNV-1a hash of the raw ARGB32 buffer per scene).
//   check  <dir>     Re-render the scene set and compare each buffer hash against
//                    the committed checksums.txt. Exit code = number of mismatches.
//                    On mismatch, also writes <name>.actual.png + <name>.diff.png
//                    and prints differing-pixel count + max channel delta.
//
// This file contains ZERO production rendering code. It only *calls* the engine's
// public API (Scene/Graphic/Element) and raw Cairo/Pango. To strip: delete the
// bench/ directory. Nothing in src/ or engine/ depends on it.
//
// Build:
//   cmake -B build-bench bench -DCMAKE_BUILD_TYPE=Release
//   cmake --build build-bench -j
//   ./build-bench/obs-graphics-bench                 # timings
//   ./build-bench/obs-graphics-bench golden bench/golden
//   ./build-bench/obs-graphics-bench check  bench/golden
// =============================================================================

#include "scene.h"
#include "graphic.h"
#include "element.h"

#ifdef __APPLE__
#include <cairo.h>
#else
#include <cairo/cairo.h>
#endif
#include <pango/pangocairo.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>

static const int W = 1920;
static const int H = 1080;
static const int ITERS = 600; // 10s @ 60fps worth of frames

// ── timing ───────────────────────────────────────────────────────────────────

using clk = std::chrono::steady_clock;

static double ms(clk::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

struct Stats {
    std::string name;
    std::vector<double> samples;
    void add(double v) { samples.push_back(v); }
    double pct(double p)
    {
        if (samples.empty())
            return 0.0;
        std::vector<double> s = samples;
        std::sort(s.begin(), s.end());
        size_t idx = (size_t)(p / 100.0 * (s.size() - 1) + 0.5);
        return s[std::min(idx, s.size() - 1)];
    }
    double mean()
    {
        if (samples.empty())
            return 0.0;
        double t = 0;
        for (double v : samples)
            t += v;
        return t / samples.size();
    }
};

static Stats bench(const std::string& name, int iters, const std::function<void()>& fn)
{
    Stats st;
    st.name = name;
    for (int i = 0; i < 10; ++i)
        fn();
    for (int i = 0; i < iters; ++i) {
        auto t0 = clk::now();
        fn();
        auto t1 = clk::now();
        st.add(ms(t1 - t0));
    }
    return st;
}

static void print_header(const char* section)
{
    std::printf("\n=== %s ===\n", section);
    std::printf("%-44s %8s %8s %8s %8s %8s\n", "phase", "mean", "p50", "p90", "p99", "max");
}

static void print_stats(Stats& s)
{
    std::printf("%-44s %8.3f %8.3f %8.3f %8.3f %8.3f\n", s.name.c_str(), s.mean(), s.pct(50),
                s.pct(90), s.pct(99), s.pct(100));
}

// ── canonical scene set (shared by timing, golden, check) ─────────────────────
//
// Each scene is rendered deterministically: the graphic state + timer are forced
// to fixed values, then Render() is called once (no Tick). Animating scenes pick a
// mid-animation timer so opacity/clip paths are exercised at a stable point.

struct SceneDef {
    std::string name;
    std::string json;
    GraphicState state; // forced
    double timer;       // forced (only meaningful while animating)
};

// Programmatically-built stress scene: many elements with interleaved z-order,
// mixed opacity, masks, and a few parent/child links. Exercises exactly the
// traversal code P4 rewrites (sort + per-frame xforms + O(n^2) mask findIdx),
// and serves as the ordering/masking regression test for that change.
static std::string many_elements_json(int n)
{
    std::string j = R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[)";
    for (int i = 0; i < n; ++i) {
        if (i)
            j += ",";
        int zo = (i * 13) % n; // interleaved, with ties -> tests stable_sort tie-break
        double x = 60 + (i % 10) * 180;
        double y = 60 + (i / 10) * 160;
        double r = (i % 5) / 4.0, g = (i % 7) / 6.0, b = (i % 3) / 2.0;
        double op = (i % 3 == 0) ? 0.6 : 1.0;
        j += "{\"id\":\"e" + std::to_string(i) + "\",\"type\":\"rectangle\"";
        j += ",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y);
        j += ",\"w\":150,\"h\":130,\"corner_radius\":10";
        j += ",\"z_order\":" + std::to_string(zo);
        j += ",\"opacity\":" + std::to_string(op);
        j += ",\"fill\":[" + std::to_string(r) + "," + std::to_string(g) + "," +
             std::to_string(b) + ",1.0]";
        if (i > 0 && i % 4 == 0) // ~1/4 masked -> stresses findIdx
            j += ",\"mask\":\"e" + std::to_string(i - 1) + "\"";
        if (i > 0 && i % 9 == 0) // a few children of e0 -> parent/child traversal
            j += ",\"parent\":\"e0\"";
        j += "}";
    }
    j += "]}]}";
    return j;
}

static std::vector<SceneDef> scene_set()
{
    std::vector<SceneDef> v;

    // The user's "large shape", solid.
    v.push_back({"solid",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160,"y":140,"w":1600,"h":800,
         "corner_radius":40,"fill":[0.2,0.4,0.9,1.0]}]}]})",
                 GraphicState::Visible, 0.0});

    // Large gradient fill.
    v.push_back({"gradient",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160,"y":140,"w":1600,"h":800,"corner_radius":40,
         "fill":{"type":"linear","x0":0,"y0":0,"x1":1,"y1":1,
           "stops":[{"offset":0,"color":[0.9,0.2,0.3,1]},{"offset":1,"color":[0.2,0.3,0.9,1]}]}}]}]})",
                 GraphicState::Visible, 0.0});

    // Large shape + drop shadow (mesh) — the primary hotspot (P1).
    v.push_back({"shadow",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160,"y":140,"w":1600,"h":800,"corner_radius":40,
         "fill":[0.2,0.4,0.9,1.0],
         "shadow":{"enabled":true,"offset_x":0,"offset_y":12,"blur":40,"color":[0,0,0,0.7]}}]}]})",
                 GraphicState::Visible, 0.0});

    // Text only.
    v.push_back({"text",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"t","type":"text","x":160,"y":400,"w":1600,"h":280,
         "text":"The quick brown fox jumps over the lazy dog 0123456789",
         "font_size":110,"font_weight":"bold","fill":[1,1,1,1],"text_align_x":"center"}]}]})",
                 GraphicState::Visible, 0.0});

    // Heavy combined: grad + stroke + shadow + text (mirrors user complaint). P1+P2.
    v.push_back({"heavy",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160,"y":140,"w":1600,"h":800,"corner_radius":40,
         "fill":{"type":"linear","x0":0,"y0":0,"x1":1,"y1":1,
           "stops":[{"offset":0,"color":[0.9,0.2,0.3,1]},{"offset":1,"color":[0.2,0.3,0.9,1]}]},
         "stroke":[1,1,1,1],"stroke_width":6,
         "shadow":{"enabled":true,"offset_x":0,"offset_y":12,"blur":40,"color":[0,0,0,0.7]}},
        {"id":"t","type":"text","x":260,"y":260,"w":1400,"h":200,"text":"LIVE BROADCAST OVERLAY",
         "font_size":96,"font_weight":"bold","fill":[1,1,1,1],"text_align_x":"center"}]}]})",
                 GraphicState::Visible, 0.0});

    // Mid-fade: opacity < 1 forces the group/paint_with_alpha path to be kept (P2).
    v.push_back({"fade_mid",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160,"y":140,"w":1600,"h":800,"corner_radius":40,
         "fill":[0.2,0.4,0.9,1.0],
         "anim_in":{"type":"fade","easing":"linear","duration":0.5}}]}]})",
                 GraphicState::AnimatingIn, 0.25});

    // Parent opacity 0.5 with children: group cascade must be preserved (P2).
    v.push_back({"children_opacity",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"p","type":"rectangle","x":200,"y":200,"w":1200,"h":600,"corner_radius":24,
         "opacity":0.5,"fill":[0.1,0.5,0.2,1.0]},
        {"id":"c1","type":"rectangle","parent":"p","x":40,"y":40,"w":400,"h":300,
         "corner_radius":12,"fill":[0.9,0.8,0.1,1.0]},
        {"id":"c2","type":"text","parent":"p","x":40,"y":380,"w":1000,"h":180,
         "text":"CHILD TEXT","font_size":80,"fill":[1,1,1,1]}]}]})",
                 GraphicState::Visible, 0.0});

    // Mid-wipe + shadow: clip path + shadow tracking under animation (P1 animating).
    v.push_back({"wipe_mid",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160,"y":140,"w":1600,"h":800,"corner_radius":40,
         "fill":[0.2,0.4,0.9,1.0],
         "shadow":{"enabled":true,"offset_x":0,"offset_y":12,"blur":40,"color":[0,0,0,0.7]},
         "anim_in":{"type":"wipe_right","easing":"linear","duration":0.5}}]}]})",
                 GraphicState::AnimatingIn, 0.25});

    // Shadow at a fractional pixel position: verifies the P1 shadow-cache keeps
    // sub-pixel phase (cache rendered phase-correct, blitted at integer device
    // coords) and is bit-identical to direct mesh rendering off the grid.
    v.push_back({"shadow_frac",
                 R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160.5,"y":140.25,"w":1600,"h":800,"corner_radius":24,
         "fill":[0.2,0.4,0.9,1.0],
         "shadow":{"enabled":true,"offset_x":6,"offset_y":10,"blur":30,"color":[0,0,0,0.7]}}]}]})",
                 GraphicState::Visible, 0.0});

    // Traversal stress (P4): 60 elements, interleaved z-order, masks, children.
    v.push_back({"many_elements", many_elements_json(60), GraphicState::Visible, 0.0});

    return v;
}

// Render one scene deterministically into `cr`/`surf` (caller clears first).
static void render_scene(const SceneDef& sd, cairo_t* cr, cairo_surface_t* surf)
{
    Scene scene = Scene::LoadString(sd.json);
    for (auto& g : scene.graphics) {
        g.state = sd.state;
        g.timer = sd.timer;
    }
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    scene.Render(cr);
    cairo_surface_flush(surf);
}

// FNV-1a over the exact ARGB32 buffer (stride*H bytes). Never goes through PNG, so
// it is an exact equality test independent of PNG premultiply rounding.
static uint64_t hash_surface(cairo_surface_t* surf)
{
    const uint8_t* data = cairo_image_surface_get_data(surf);
    size_t n = (size_t)cairo_image_surface_get_stride(surf) * H;
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}

// ── timing mode ────────────────────────────────────────────────────────────────

static int run_timing()
{
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t* cr = cairo_create(surf);

    std::printf("obs-graphics benchmark — %dx%d ARGB32, %d frames/phase\n", W, H, ITERS);
    std::printf("stride=%d (cairo) vs format_stride_for_width=%d\n",
                cairo_image_surface_get_stride(surf),
                cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, W));

    print_header("RAW PRIMITIVE PHASES (per-frame cost in isolation)");
    Stats clearOp = bench("clear: CAIRO_OPERATOR_CLEAR + paint", ITERS, [&]() {
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        cairo_surface_flush(surf);
    });
    print_stats(clearOp);

    uint8_t* data = cairo_image_surface_get_data(surf);
    int stride = cairo_image_surface_get_stride(surf);
    Stats clearMemset = bench("clear: memset", ITERS, [&]() {
        cairo_surface_flush(surf);
        std::memset(data, 0, (size_t)stride * H);
        cairo_surface_mark_dirty(surf);
    });
    print_stats(clearMemset);

    std::vector<uint8_t> dst((size_t)stride * H);
    Stats upload = bench("upload proxy: memcpy stride*H", ITERS,
                         [&]() { std::memcpy(dst.data(), data, (size_t)stride * H); });
    print_stats(upload);

    Stats solidFill = bench("solid fill 1600x800", ITERS, [&]() {
        cairo_set_source_rgba(cr, 0.2, 0.4, 0.9, 1.0);
        cairo_rectangle(cr, 160, 140, 1600, 800);
        cairo_fill(cr);
    });
    print_stats(solidFill);

    Stats gradFill = bench("linear gradient fill 1600x800", ITERS, [&]() {
        cairo_pattern_t* p = cairo_pattern_create_linear(160, 140, 1760, 940);
        cairo_pattern_add_color_stop_rgba(p, 0, 0.9, 0.2, 0.3, 1);
        cairo_pattern_add_color_stop_rgba(p, 1, 0.2, 0.3, 0.9, 1);
        cairo_set_source(cr, p);
        cairo_rectangle(cr, 160, 140, 1600, 800);
        cairo_fill(cr);
        cairo_pattern_destroy(p);
    });
    print_stats(gradFill);

    Stats group = bench("push_group+pop+paint_with_alpha (1920x1080)", ITERS, [&]() {
        cairo_push_group(cr);
        cairo_set_source_rgba(cr, 0.2, 0.4, 0.9, 1.0);
        cairo_rectangle(cr, 160, 140, 1600, 800);
        cairo_fill(cr);
        cairo_pop_group_to_source(cr);
        cairo_paint_with_alpha(cr, 0.99);
    });
    print_stats(group);

    print_header("TEXT: SHAPING vs RASTERIZATION");
    {
        const char* str = "LIVE BROADCAST OVERLAY 0123456789";
        Stats shape = bench("text: create layout + shape (extents)", ITERS, [&]() {
            PangoLayout* layout = pango_cairo_create_layout(cr);
            pango_layout_set_text(layout, str, -1);
            PangoFontDescription* fd = pango_font_description_new();
            pango_font_description_set_family(fd, "Sans");
            pango_font_description_set_size(fd, (int)(96 * PANGO_SCALE));
            pango_font_description_set_weight(fd, PANGO_WEIGHT_BOLD);
            pango_layout_set_font_description(layout, fd);
            PangoRectangle ink;
            pango_layout_get_pixel_extents(layout, &ink, nullptr);
            pango_font_description_free(fd);
            g_object_unref(layout);
        });
        print_stats(shape);

        PangoLayout* layout = pango_cairo_create_layout(cr);
        pango_layout_set_text(layout, str, -1);
        PangoFontDescription* fd = pango_font_description_new();
        pango_font_description_set_family(fd, "Sans");
        pango_font_description_set_size(fd, (int)(96 * PANGO_SCALE));
        pango_font_description_set_weight(fd, PANGO_WEIGHT_BOLD);
        pango_layout_set_font_description(layout, fd);
        PangoRectangle ink;
        pango_layout_get_pixel_extents(layout, &ink, nullptr);
        Stats raster = bench("text: show_layout only (pre-shaped)", ITERS, [&]() {
            cairo_move_to(cr, 260, 260);
            cairo_set_source_rgba(cr, 1, 1, 1, 1);
            pango_cairo_show_layout(cr, layout);
        });
        print_stats(raster);
        pango_font_description_free(fd);
        g_object_unref(layout);
    }

    print_header("ENGINE SCENE PATH (clear + Render + flush, per canonical scene)");
    for (auto& sd : scene_set()) {
        Scene scene = Scene::LoadString(sd.json);
        for (auto& g : scene.graphics) {
            g.state = sd.state;
            g.timer = sd.timer;
        }
        Stats s = bench("scene: " + sd.name, ITERS, [&]() {
            cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            scene.Render(cr);
            cairo_surface_flush(surf);
        });
        print_stats(s);
    }

    // Worst case for the P1 shadow cache: an actively-animating wipe whose
    // shadow box changes every frame (Tick advances timer, long duration keeps
    // it animating) -> cache misses every frame. Characterises any regression
    // the cache introduces for animated shadows vs direct mesh rendering.
    print_header("ANIMATED SHADOW (Tick-driven, cache worst case)");
    {
        const char* wipeJson = R"({"width":1920,"height":1080,"graphics":[{"id":"g","elements":[
        {"id":"r","type":"rectangle","x":160,"y":140,"w":1600,"h":800,"corner_radius":40,
         "fill":[0.2,0.4,0.9,1.0],
         "shadow":{"enabled":true,"offset_x":0,"offset_y":12,"blur":40,"color":[0,0,0,0.7]},
         "anim_in":{"type":"wipe_right","easing":"linear","duration":1000.0}}]}]})";
        Scene scene = Scene::LoadString(wipeJson);
        for (auto& g : scene.graphics)
            g.state = GraphicState::AnimatingIn;
        Stats s = bench("wipe shadow animating (per-frame Tick)", ITERS, [&]() {
            scene.Tick(1.0f / 60.0f);
            cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint(cr);
            cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
            scene.Render(cr);
            cairo_surface_flush(surf);
        });
        print_stats(s);
    }

    std::printf("\nBudget reference: 16.67 ms/frame @ 60fps, 33.33 ms @ 30fps.\n");

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return 0;
}

// ── golden mode ──────────────────────────────────────────────────────────────

static int run_golden(const std::string& dir)
{
    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t* cr = cairo_create(surf);

    std::ofstream sums(dir + "/checksums.txt");
    if (!sums) {
        std::fprintf(stderr, "cannot write %s/checksums.txt (does the dir exist?)\n", dir.c_str());
        return 2;
    }
    for (auto& sd : scene_set()) {
        render_scene(sd, cr, surf);
        uint64_t h = hash_surface(surf);
        std::string png = dir + "/" + sd.name + ".png";
        cairo_surface_write_to_png(surf, png.c_str());
        sums << sd.name << " " << std::hex << h << std::dec << "\n";
        std::printf("golden %-18s hash=%016llx -> %s\n", sd.name.c_str(),
                    (unsigned long long)h, png.c_str());
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return 0;
}

// ── check mode ───────────────────────────────────────────────────────────────

// Write an amplified visual diff vs the baseline PNG (diagnostics only — the
// pass/fail gate is the exact buffer hash, not this PNG comparison).
static void write_diff(const SceneDef& sd, cairo_surface_t* live, const std::string& dir,
                       long& diffPixels, int& maxDelta)
{
    std::string basePng = dir + "/" + sd.name + ".png";
    cairo_surface_t* base = cairo_image_surface_create_from_png(basePng.c_str());
    diffPixels = -1;
    maxDelta = -1;
    if (cairo_surface_status(base) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(base);
        return;
    }
    int bw = cairo_image_surface_get_width(base);
    int bh = cairo_image_surface_get_height(base);
    if (bw != W || bh != H) {
        cairo_surface_destroy(base);
        return;
    }
    cairo_surface_flush(live);
    const uint8_t* lp = cairo_image_surface_get_data(live);
    const uint8_t* bp = cairo_image_surface_get_data(base);
    int lstride = cairo_image_surface_get_stride(live);
    int bstride = cairo_image_surface_get_stride(base);

    cairo_surface_t* diff = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    uint8_t* dp = cairo_image_surface_get_data(diff);
    int dstride = cairo_image_surface_get_stride(diff);

    diffPixels = 0;
    maxDelta = 0;
    for (int y = 0; y < H; ++y) {
        const uint8_t* lr = lp + (size_t)y * lstride;
        const uint8_t* br = bp + (size_t)y * bstride;
        uint8_t* dr = dp + (size_t)y * dstride;
        for (int x = 0; x < W; ++x) {
            int md = 0;
            for (int c = 0; c < 4; ++c) {
                int d = std::abs((int)lr[x * 4 + c] - (int)br[x * 4 + c]);
                md = std::max(md, d);
            }
            if (md > 0)
                ++diffPixels;
            maxDelta = std::max(maxDelta, md);
            // Amplify: red where different (>1 = real, <=1 = PNG rounding noise).
            uint8_t v = md > 1 ? 255 : (md == 1 ? 80 : 0);
            dr[x * 4 + 0] = 0;       // B
            dr[x * 4 + 1] = 0;       // G
            dr[x * 4 + 2] = v;       // R
            dr[x * 4 + 3] = v ? 255 : 0;
        }
    }
    cairo_surface_mark_dirty(diff);
    cairo_surface_write_to_png(diff, (dir + "/" + sd.name + ".diff.png").c_str());
    cairo_surface_write_to_png(live, (dir + "/" + sd.name + ".actual.png").c_str());
    cairo_surface_destroy(diff);
    cairo_surface_destroy(base);
}

static int run_check(const std::string& dir)
{
    std::map<std::string, uint64_t> expected;
    {
        std::ifstream sums(dir + "/checksums.txt");
        if (!sums) {
            std::fprintf(stderr, "cannot read %s/checksums.txt — run golden first\n", dir.c_str());
            return 2;
        }
        std::string name, hexh;
        while (sums >> name >> hexh)
            expected[name] = std::stoull(hexh, nullptr, 16);
    }

    cairo_surface_t* surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    cairo_t* cr = cairo_create(surf);

    int mismatches = 0;
    for (auto& sd : scene_set()) {
        render_scene(sd, cr, surf);
        uint64_t h = hash_surface(surf);
        auto it = expected.find(sd.name);
        if (it == expected.end()) {
            std::printf("  %-18s NO BASELINE\n", sd.name.c_str());
            continue;
        }
        if (h == it->second) {
            std::printf("  %-18s OK (exact)\n", sd.name.c_str());
        } else {
            ++mismatches;
            long diffPixels = -1;
            int maxDelta = -1;
            write_diff(sd, surf, dir, diffPixels, maxDelta);
            std::printf("  %-18s MISMATCH  hash %016llx != %016llx  diffPixels=%ld maxChannelDelta=%d"
                        "  (wrote %s.actual.png / %s.diff.png)\n",
                        sd.name.c_str(), (unsigned long long)h, (unsigned long long)it->second,
                        diffPixels, maxDelta, sd.name.c_str(), sd.name.c_str());
        }
    }
    cairo_destroy(cr);
    cairo_surface_destroy(surf);

    std::printf("\n%d/%zu scenes mismatched.\n", mismatches, scene_set().size());
    return mismatches;
}

int main(int argc, char** argv)
{
    if (argc >= 2 && std::string(argv[1]) == "golden") {
        if (argc < 3) {
            std::fprintf(stderr, "usage: %s golden <dir>\n", argv[0]);
            return 2;
        }
        return run_golden(argv[2]);
    }
    if (argc >= 2 && std::string(argv[1]) == "check") {
        if (argc < 3) {
            std::fprintf(stderr, "usage: %s check <dir>\n", argv[0]);
            return 2;
        }
        return run_check(argv[2]);
    }
    return run_timing();
}
