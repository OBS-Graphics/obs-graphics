# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

This is an OBS Studio plugin ("Graphics Source") that renders animated broadcast graphics overlays. It has two main components:

1. **OBS plugin** (`/` root) — a shared library (`.so`) loaded by OBS, built with the root `CMakeLists.txt`.
2. **editor_v2** (`src/editor_v2/`) — a standalone Qt6 desktop app for authoring scenes, built with its own `CMakeLists.txt`. This is the active editor under development.

There is also an older `src/editor/` that is no longer active.

## Build commands

### editor_v2 (standalone app — most common workflow)

```bash
cd src/editor_v2
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/editor_v2
```

Qt Creator opens this via the `build/Desktop-Debug` kit preset. The build uses `gmake`.

### OBS plugin (root project)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

Requires OBS development headers and Qt6 installed system-wide.

### Adding a new source file

Register it in the `PROJECT_SOURCES` list in `src/editor_v2/CMakeLists.txt` (both `.h` and `.cpp`). Qt's AUTOMOC/AUTOUIC/AUTORCC handle code generation automatically.

## Architecture

### Engine (`src/engine/`) — shared static library

The rendering core, used by both the plugin and editor_v2. Renders via **Cairo/Pango** (not Qt). Key types defined in `types.hpp`:

- **`Paint`** — Solid/Linear/Radial fill. Params layout:
  - Solid: `params[0..3]` = r,g,b,a
  - Linear: `params[0..3]` = x0,y0,x1,y1 (normalized 0–1, multiplied by element bounds at render)
  - Radial: `params[0..2]` = focus cx,cy,r; `params[3..5]` = main cx,cy,r (coords normalized by element width)
- **`Element`** — A renderable item (Rectangle or Text) with `bounds`, `fill`/`stroke` (`Paint`), `cornerRadius[4]`, `opacity`, `rotation`, in/out `AnimationDef`, and text properties. `bounds.x/y` are **local (parent-relative)** when a `parent` is set; `GetGlobalPosition()` walks the parent chain to compute the screen position.
- **`Graphic`** — A named group of `Element`s with a state machine: `Hidden → AnimatingIn → Visible → AnimatingOut → Hidden`.
- **`Scene`** — Top-level container: `width`/`height` (serialized as `"width"`/`"height"` in JSON, default 1920×1080) + vector of `Graphic`s. Loads from JSON via `Scene::Load()` / `Scene::LoadString()`.
- **`AnimationDef`** / **`AnimatedTransform`** — Per-element in/out animation (type, easing, duration, delay). `animation::EvaluateAnimation()` computes the transform at a given timer value.
- **Mask clipping** — `Element::ApplyClipping()` clips to the mask element's bounds. It accepts an optional `const AnimatedTransform* maskXf` so that the mask element's animated offset (from slide animations) is applied to the clip region. `Graphic::Render()` pre-computes all element transforms and passes the mask element's xf when rendering. The mask offset is computed via `GetGlobalPosition()` on both elements — never via raw `bounds` — so clipping is correct even when either element has a parent.

### editor_v2 model layer

- **`SceneDocument`** — Owns `Scene` + `QUndoStack` + file I/O. **All mutations must go through `applyMutation(fn)`**, which updates the scene and emits `documentChanged()`. Raw `Element*` pointers are rebuilt by `resolveElementPointers()` after every structural change (mask/parent relationships stored separately as string IDs in `m_elementRefs`). `setElementRef(gi, ei, maskId, parentId)` updates those IDs and resolves pointers — **it is NOT undoable** and does NOT adjust `bounds`; callers that change the parent must convert coordinates themselves. `load()` clears the undo stack so Ctrl+Z cannot cross file-session boundaries.
- **`EditorScene`** — Wraps `SceneDocument`, tracks the current selection (`SelectionId`: scene/graphic/element level + indices). Emits `selectionChanged`. `validateSelection()` runs on every `documentChanged` and clears any selection whose indices are now out of bounds (e.g. after undo of an add, or delete).
- **`UndoCommands`** — Template-based commands:
  - `SetElementFieldCmd<T>` — generic field setter with optional `mergeWith` for rapid slider edits (pass a `mergeTag`).
  - `SetGraphicFieldCmd<T>` — same pattern for Graphic fields.
  - `SetElementPaintCmd` — handles the full `Paint` struct.
  - `SetSceneDimensionsCmd` — sets `scene.width` / `scene.height` with undo.
  - `SetSceneNameCmd` — sets the scene name with undo.
  - `AddGraphicCmd` / `RemoveGraphicCmd` / `AddElementCmd` / `RemoveElementCmd` — structural commands that serialize to JSON for safe undo storage.
  - `MoveGraphicCmd` / `MoveElementCmd` — move items within the vector (vector order ≠ Z-Order; the tree sorts by Z-Order, so these are mostly used internally).

### editor_v2 UI layer

- **`ui/`** — Property panel widgets (pure code, no `.ui` files):
  - `elementproperties` — top-level element panel; `onParentChanged()` converts `bounds.x/y` from global → local (or local → global when clearing the parent) to keep the element at its on-screen position. The ID field is made read-only (gray italic + tooltip) when another element in the same graphic has `parent == this element` — detected via pointer comparison after `resolveElementPointers()`.
  - `sceneproperties` — scene name, resolution preset combo + width/height spinboxes.
  - `transformeditor` — bounds, rotation, opacity.
  - `animationeditor` — in/out animation picker.
  - `painteditor` — fill/stroke paint editor (integrates gradient widgets).
  - `adaptivestack` — responsive layout helper.

- **`ui/widgets/`** — Reusable custom widgets (pure code):
  - `CanvasWidget` — Cairo-backed canvas. Scene dimensions come from `m_doc->scene().width/height` via `sceneW()`/`sceneH()` helpers; the Cairo surface is recreated in `onDocumentChanged()` when dimensions change. Supports Shift (aspect-ratio resize) and Ctrl (center-scale resize) modifiers on corner handles.
  - `SceneTreeModel` — Hierarchical model for Scene → Graphics → Elements. Items are **displayed sorted descending by `zOrder`** (highest Z at top = renders on top). Drag-and-drop updates `zOrder` values (using a macro `QUndoCommand` with `SetGraphicFieldCmd`/`SetElementFieldCmd` children) rather than moving items in the vector. Icons via QtAwesome: `fa_film` (scene), `fa_layer_group` (graphic), `fa_square`/`fa_font` (element). Z-Order prefix is not shown in the display text.
  - `GradientEditor` — horizontal gradient bar; stops are draggable circles. Uses `sampleGradient()` for newly added stops.
  - `LinearGradientEditor` — stores endpoints `m_p1`/`m_p2` in **normalized 0–1 coords** internally; `toPixel()`/`toNorm()` helpers convert for rendering and hit-testing.
  - `RadialGradientEditor` — stores `m_center`/`m_radiusEnd` in **normalized 0–1 coords**; `radius()` returns a normalized fraction.
  - `ColorWheel`, `HueSlider`, `GradientSlider`, `ColorPreview`, `ColorLineEdit`, `ColorUtils`.

- **`ui/widgets/designer_plugin/`** — Qt Designer plugin that registers all custom widgets.

- **`MainWindow`** — Full menu bar (File/Edit/View) and toolbar. Toolbar actions `m_addRectAction`, `m_addTextAction`, `m_deleteAction` are enabled/disabled via `updateToolBarState()` which is called on every `selectionChanged`: add-element actions require a graphic or element selection; delete requires either. Add-Graphic/Rect/Text lambdas generate unique IDs by scanning existing IDs for the highest numeric suffix (`graphic_N`, `element_N`, `text_N`) and auto-select the newly created item immediately after the command executes.

## Key conventions

- **Normalized coordinates**: gradient endpoints and radii in `Paint` are normalized (0–1) and scaled by element bounds at Cairo render time. Editor widgets store/emit in the same 0–1 space.
- **Local vs. global bounds**: `Element::bounds.x/y` are local (parent-relative) when `element.parent != nullptr`. Always use `GetGlobalPosition()` for screen position. When assigning a new parent in the editor, adjust `bounds.x/y` to `globalPos - newParent.GetGlobalPosition()`.
- **Z-Order vs. vector order**: `Graphic::Render()` sorts elements by `zOrder` ascending (lower = rendered first = behind). The scene tree displays items sorted by `zOrder` descending (highest = top). Vector order is independent — don't rely on it for rendering priority.
- **Stop ordering**: `GradientEditor` calls `sortStops()` on mouse release (not during drag) to avoid index invalidation mid-drag.
- **`QVector<GradientStop>`**: the shared stop type. `GradientStop` = `{qreal position, QColor color}`.
- **Two-pass handle rendering**: selected handle is drawn last using a `pass == 0 / pass == 1` loop — use this in any widget with selectable handles.
- **`paint` vs `Paint`**: the engine `Paint` holds doubles for Cairo. The Qt-side `GradientStop` uses `QColor`. Conversion happens in the paint editor when writing back to the document.
- **QtAwesome API**: `qta()->icon(fa::fa_solid, fa::fa_iconname)` — always pass both the style (`fa::fa_solid`) and the character constant.
