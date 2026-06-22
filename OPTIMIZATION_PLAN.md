# Cairo Render Performance — Investigation & Optimization Plan

**Goal:** decide whether the current Cairo/Pango CPU path can hold a realtime frame
budget (16.67 ms @ 60fps) before considering a different rasterizer.

**Verdict up front:** Yes. The lag is caused by a small number of fixable hotspots —
overwhelmingly the **mesh-gradient drop shadow**, recomputed and repainted full-frame
*every* frame. The bottleneck is **rasterization**, not Pango shaping and not the
texture upload. No rasterizer switch is warranted yet.

---

## 1. The hot path (files & functions)

Per displayed frame, two OBS callbacks run:

**`src/graphics-source.cpp`**
- `source_video_tick()` (`:82`) — render thread. Each frame:
  - `Scene::Tick(seconds)` (`:102`)
  - `cairo_set_operator(CLEAR); cairo_paint()` full-surface clear (`:104-105`)
  - `Scene::Render(cr)` (`:107`)
  - `cairo_surface_flush()` (`:108`)
- `source_render()` (`:111`) — graphics thread. Uploads pixels via
  `gs_texture_set_image()` (`:127`) and draws the sprite. `cairo_t`/`cairo_surface_t`
  are **pooled** (rebuilt only on resize, `rebuild_cairo_surface` `:21`). Good.

**`engine/scene.cpp`**
- `Scene::Render()` (`:358`) — allocates a `gOrder` vector + `stable_sort` **every
  frame**, then calls `Graphic::Render`.

**`engine/graphic.cpp`**
- `Graphic::Render()` (`:88`) — every frame allocates `eOrder` + sort, allocates an
  `xforms` vector, evaluates every element's animation, and does an **O(n²)** `findIdx`
  (`:107`) for mask lookups. Runs in full even when the graphic is static (`Visible`).

**`engine/element.cpp`** — the heavy lifting:
- `Element::Render()` (`:134`):
  - **`DrawDropShadow()`** (`:42`, called `:165`) — builds a `cairo_pattern_create_mesh`
    with dozens of patches (two rings: gradient ring + solid fan), recomputed from
    scratch every frame, then `cairo_paint`ed over the whole region.
  - **`cairo_push_group` (`:184`) … `cairo_pop_group_to_source` + `cairo_paint_with_alpha`
    (`:482-483`)** — wraps *every* element in an intermediate group layer,
    **unconditionally**, even when opacity is 1.0 and there are no children.
  - Fill/stroke via `Paint::Apply()` (`types.hpp:97`) — creates/destroys a
    `cairo_pattern_t` each call for gradients (cheap).
  - Text (`:240`) — creates `PangoLayout` + `PangoFontDescription` + `PangoAttrList`
    every frame and reshapes, even when the string is unchanged.
  - Image (`:353`) — surface is cached (`m_image`), reloaded only on path change. Good.
  - QrCode (`:418`) — re-encoded only on text change. Good.

---

## 2. Measurement harness

Built a standalone, OBS-free harness: **`bench/`** (`bench.cpp` + `CMakeLists.txt`).
It renders representative 1920×1080 overlay content to an offscreen ARGB32 surface and
reports per-phase percentiles over 600 frames. It reuses the engine's own CMake/CPM
setup via `add_subdirectory(../engine)` and calls only the engine's **public API** plus
raw Cairo/Pango — **no production rendering code was modified**. Marked
`[INSTRUMENTATION — SAFE TO DELETE]`; strip by deleting `bench/`.

```
cmake -B build-bench bench -DCMAKE_BUILD_TYPE=Release
cmake --build build-bench -j
./build-bench/obs-graphics-bench
```

### Results (Release, this machine — relative ranking is the point)

```
RAW PRIMITIVE PHASES                         mean     p50     p90     p99     max   (ms)
clear: CAIRO_OPERATOR_CLEAR + paint         0.000   0.000   0.000   0.000   0.001
clear: memset                               0.713   0.681   0.832   1.019   1.206
upload proxy: memcpy stride*H               0.583   0.567   0.646   0.733   0.847
solid fill 1600x800                         0.571   0.538   0.734   0.873   0.952
linear gradient fill 1600x800               7.582   7.544   7.697   8.190  11.803
push_group+pop+paint_with_alpha             2.791   2.692   3.054   4.305   4.649

TEXT: SHAPING vs RASTERIZATION
text: create layout + shape (extents)       0.011   0.011   0.011   0.016   0.020
text: show_layout only (pre-shaped)         0.114   0.110   0.125   0.165   0.198

ENGINE SCENE PATH (Tick+clear+Render+flush, mirrors video_tick)
scene: big rect solid                       3.074   2.987   3.414   3.777   4.602
scene: big rect linear gradient            10.239  10.127  10.607  12.131  15.080
scene: big rect + drop shadow (mesh)       70.012  69.442  72.275  79.978  89.487
scene: text only                            3.004   2.917   3.334   3.726   4.061
scene: HEAVY (grad+stroke+shadow+text)     78.848  78.378  81.350  88.275 101.935
scene: HEAVY animating-in                  78.765  78.382  80.546  87.276  98.319
```

### What the numbers say

- **Bottleneck = rasterization, specifically the drop shadow.** A single large shape
  with a shadow costs **70 ms/frame** — 4.2× the 60fps budget, 2× the 30fps budget. It
  is ~85% of the 79 ms "heavy" frame. This is the lag the user reports.
- **Large gradient fill = ~7.5 ms** and is re-rasterized every frame even when static —
  the clear secondary cost.
- **`push_group` ≈ 2.8 ms *per element*, paid unconditionally.** "big rect solid" is
  3.07 ms total but the solid fill is only 0.57 ms — the group layer is the rest. This
  multiplies by element count.
- **Text is not a problem.** Shaping 0.011 ms, rasterization 0.11 ms. Per-frame
  `PangoLayout` recreation is wasteful in principle but ~0.1 ms in practice.
- **Upload is not a problem.** CPU copy of the frame is 0.58 ms. (Caveat: this is a
  `memcpy` proxy; the real GPU `gs_texture_set_image` transfer is not measured, but the
  CPU side is cheap and unchanged by any item here.)
- **Clear is effectively free** (`CAIRO_OPERATOR_CLEAR` ≈ 0 ms; even a raw `memset` is
  0.7 ms). The "use memset instead of CLEAR" pitfall **does not apply here.**

---

## 3. Cairo realtime-pitfall audit (with code locations)

| Pitfall | Applies? | Location / evidence |
|---|---|---|
| **Per-frame `cairo_t`/`cairo_surface_t` churn** | **No** — pooled | `graphics-source.cpp:21,32` rebuild only on resize |
| **Per-frame `PangoLayout`/`FontDescription`/`AttrList`** | **Yes**, but low impact | `element.cpp:265,273,277` created+freed every frame; measured ~0.1 ms |
| **Per-frame `cairo_pattern_t`** | Yes, negligible | `types.hpp:114,123-130` gradient/image pattern per fill; pattern *creation* is cheap, the *rasterization* (7.5 ms) is the cost |
| **Static content re-rasterized** | **Yes**, major | Whole scene re-rendered each frame incl. `Visible` graphics; gradient `:10.2ms`, shadow `:70ms` recomputed every frame |
| **Mesh gradient rebuilt + repainted per frame** | **Yes — primary** | `element.cpp:84-120` `DrawDropShadow` allocates vectors, trig per vertex, builds full mesh, paints full-frame, every frame |
| **`OPERATOR_CLEAR` full paint vs memset** | **No** | CLEAR measured ~0 ms; memset is *slower* (0.7 ms). Skip. |
| **`OPERATOR_OVER` where `SOURCE` suffices** | Minor | `graphics-source.cpp:106` + the unconditional `push_group`/`paint_with_alpha` (`element.cpp:184,483`) force OVER compositing even for opaque elements |
| **Non-fast-path buffer (stride/format)** | **No** | ARGB32, `stride=7680 == cairo_format_stride_for_width` (printed by harness); premultiplied; matches `GS_BGRA`. Pixman fast paths are hit. |
| **Antialiasing forced unnecessarily** | Minor | `CAIRO_ANTIALIAS_DEFAULT` everywhere; QR/axis-aligned rect fills could use `NONE`. Not a measured bottleneck. |
| **Rendering on the OBS graphics thread** | Partly | Render runs in `video_tick` (render thread), upload in `video_render` (graphics thread). A worker + double buffer is possible but likely unnecessary after the fixes below. |
| **O(n²) / per-frame allocations in traversal** | Yes, small | `graphic.cpp:107` `findIdx`; `scene.cpp:360` + `graphic.cpp:93,101` vectors+sort every frame |

---

## 4. Prioritized plan

Estimated combined effect of P1+P2+P3: the HEAVY frame drops from **~79 ms to an
estimated ~4–6 ms** — comfortably under the 60fps budget — which is why a rasterizer
switch and a threading rework are **not** currently justified.

### DO REGARDLESS (measurement-backed, high impact)

**P1 — Cache the drop shadow to an offscreen surface.** *(impact: ~67 ms → ~0.5 ms;
effort: medium; risk: medium)*
- `engine/element.cpp` `DrawDropShadow` (`:42`) + new `mutable` cache members on
  `Element` (alongside `m_image`, `element.h:118-122`).
- Render the mesh once into a small ARGB32 surface keyed on
  `{sw, sh, blur, cornerR, color, effective-opacity}`; each frame blit it with
  `set_source_surface` + `paint`. Invalidate when the key changes.
- During wipe animations the shadow tracks the clip box and changes each frame
  (`element.cpp:152-162`), so the cache only helps in the static/`Visible` case — but
  that is the common broadcast-overlay case and is exactly where 70 ms/frame is being
  burned today.
- **Handoff/CI:** does not touch the `gs_texture` path or any platform code; pure Cairo.
  Adds one cached surface per shadowed element (memory cost — free on invalidation/dtor).

**P2 — Skip the group layer for opaque, childless elements.** *(impact: ~2.8 ms ×
elements; effort: low; risk: low)*
- `engine/element.cpp:184` and `:482-483`. When `opacity * xf.opacity >= ~1.0` **and**
  `children.empty()`, draw directly instead of `push_group`/`pop`/`paint_with_alpha`.
  Keep the group when children exist (opacity must cascade) or when opacity < 1.
- **Handoff/CI:** none affected; pure Cairo.

### DO REGARDLESS (low risk, smaller)

**P3 — Cache static content / bake static graphics to a layer.** *(impact: gradient
7.5 ms → ~0.5 ms, and collapses a whole static graphic to one blit; effort: medium;
risk: medium)*
- When a `Graphic` is `Visible` (not animating) and its elements are unchanged, render
  it once to a cached surface and composite that each frame. Invalidate on
  state/data/property change (`Graphic::UpdateData` `graphic.cpp:16`, triggers).
- **Handoff/CI:** none affected. Memory: one surface per cached graphic.

**P4 — Remove per-frame allocations & O(n²) in traversal.** *(impact: small ms, but
zero-risk and removes allocator churn; effort: low; risk: low)*
- `scene.cpp:360` and `graphic.cpp:93,101,107`: precompute and cache the z-order
  ordering; re-sort only when `zOrder`/membership changes. Reuse scratch `xforms`
  storage instead of allocating each frame. Replace `findIdx` with an index map.

### ONLY IF MEASUREMENTS LATER JUSTIFY

**P5 — Reuse `PangoLayout`/`FontDescription` across frames** (reshape only on
text/font change). *Measured benefit ~0.1 ms — defer.* `element.cpp:265-349`.

**P6 — `CAIRO_ANTIALIAS_NONE` for QR/axis-aligned rect fills.** Not a measured
bottleneck; only revisit if a QR-heavy scene shows up in the harness.

**P7 — Move rendering to a worker thread with double-buffered surfaces.** Large
architectural change touching the `video_tick`/`video_render` split and the
`gs_texture` handoff (`graphics-source.cpp:104-134`). After P1–P3 the frame is already
well under budget, so **do not do this** unless a future profile says otherwise.

**P8 — `memset` instead of `CAIRO_OPERATOR_CLEAR`.** **Rejected** — measurements show
CLEAR is already ~free and memset is slower.

### Explicitly flagged risks to the handoff / CI

- None of P1–P4 change the CPU-buffer → `gs_texture` handoff (`graphics-source.cpp:117-134`)
  or the surface format/stride that keeps pixman fast paths alive — preserve those.
- All P1–P4 changes live in `engine/` Cairo logic and are portable
  (Ubuntu/macOS/Windows). New cached surfaces must be freed in `Element`/`Graphic`
  destructors and on invalidation to avoid leaks; they add bounded memory (one ARGB32
  surface per shadowed/cached item).
- Keep the `bench/` harness out of the plugin/CI build (it is a separate out-of-tree
  CMake project) and re-run it after each change to confirm the predicted gains.

---

## Addendum — Drop-shadow box-blur work (status + saved follow-ups)

The drop shadow was reworked away from the mesh approach. Current engine state and the
remaining, **not-yet-implemented** chunks are recorded here for a future session.

### Done
- **P1 (engine `6d28715`, superseded):** cached the *mesh* shadow rasterization to an
  offscreen surface (bit-identical). Hid the cost for static shadows but animated
  shadows still missed the cache every frame (~14.7 ms).
- **Box-blur swap — Chunk 1 (engine `c972c36`, current):** drop shadow is now a 3×
  separable running-sum **box-blur (≈Gaussian)** of the element shape on an **A8**
  surface, composited via `cairo_mask_surface()`. O(pixels), radius-independent.
  Inner loop tuned: fixed-point **reciprocal multiply** (no per-pixel division) and a
  **row-sequential per-column running sum** for the vertical pass. Rendered **per-frame
  (no cache)**. `sigma = shadow.blur`; A8 padded by `sum(box radii)+1` per side; running
  sum `uint32` with clamp-to-edge. Code: `RenderDropShadow` / `BoxBlurH` / `BoxBlurV` /
  `GaussBoxRadii` / `BlurRecip` in `engine/element.cpp`. The look changed (smoother
  Gaussian vs the old boxy linear ring) and was **approved**; the 6 shadow goldens were
  re-baselined.

### Measured (bench `p50`, post-Chunk-1, per-frame box-blur, no cache)
- animated wipe shadow **2.7 ms** (was 14.7 ms with the mesh) — the motivating win.
- static full-size shadow **14.6 ms** per-frame (under the 60fps budget even uncached).
- `shadow_large` (blur 120) **18.4 ms**; `shadow_multi` 18.5 ms.

### Saved follow-ups (implement in order, one per chunk, gated by the harness)

**Chunk 2 — Cache the blurred A8 (static shadows → one mask paint).**
Cache the post-blur A8 surface keyed on `{sw, sh, blur, cornerR, r, g, b, a, frac(sx),
frac(sy)}`; re-blur only on key change, otherwise just `cairo_mask_surface()` the cached
A8. Sub-pixel phase (`frac(sx/sy)`) must be in the key because the shape is rasterized
into the A8 at a fractional offset — including it makes the cache **pixel-identical** to
the per-frame path (zero-diff gate). Static shadows collapse from ~14.6 ms to ~1 ms;
animated shadows (key changes each frame) are unaffected. Low risk. One ARGB32/A8 surface
per shadowed element; free on key change/destroy.

**Chunk 3 — Downsample for large radii (CHANGES OUTPUT → needs approval).**
Above a radius threshold (e.g. `blur > 32`), render the shadow source at 1/2 or 1/4 res,
blur with a proportionally smaller radius, and upscale (bilinear) at composite. Cuts the
blur pixel count 4–16×: `shadow_large` ~18 ms → ~3–5 ms. Approval-gated visual change
(downsampling softens slightly — usually imperceptible for big blurs). Gate strictly
behind the threshold so small/sharp shadows keep the exact path. Watch the upscale filter
at the A8 edges (clamp/extend to avoid a faint seam).

**Chunk 4 — 9-slice rounded-rect shadows (only if measurements justify).**
A rounded-rect's blurred shadow is separable: blur **one corner tile + one horizontal +
one vertical edge strip**, then tile/stretch the 9 regions (4 corners, 4 edges, solid
center) onto the destination. Turns O(area) blur into O(corner + edge). Biggest win for
large solid shapes. High complexity (slice bookkeeping, seam-free tiling, asymmetric
corner radii); only pursue if Chunk 2+3 leave a measured gap. Pixel-near-identical for
uniform-corner rects; verify seams against goldens.

### Constraints carried forward
- Preserve the CPU-buffer → `gs_texture` handoff (`graphics-source.cpp:117-134`) and
  cross-platform builds; all shadow code is portable Cairo in `engine/element.cpp`.
- Engine changes go in the working clone `~/Projects/obs-graphics-engine`, pushed, then
  the `engine/` submodule pointer bumped here.
- Gate every chunk with `bench/` golden checks; caching/refactor chunks must be
  pixel-identical, output-changing chunks (downsample) need explicit approval.
