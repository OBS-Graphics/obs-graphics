# CLAUDE.md

OBS Studio plugin that exposes a "Graphics Source" which renders the process-global `Scene`'s `Title`s via Cairo and uploads the result as an OBS texture each frame.

## Build

```bash
git submodule update --init   # populate engine/
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

Requires OBS development headers and Qt6 installed system-wide.

To install into the current user's local OBS config for testing (instead of
system paths), use the `linux-dev` preset:

```bash
cmake --preset linux-dev
cmake --build --preset linux-dev
cmake --install build_linux_dev   # installs into ~/.config/obs-studio/plugins/stream-canvas
```

## Source files (`src/`)

- `plugin-main.cpp` — `obs_module_load` / `obs_module_unload`: registers the OBS source type, creates the dock widget, and installs the one global `obs_add_tick_callback` that drives `Scene::Tick`. (`src/plugin-main.c` also exists on disk but is **not** in `target_sources` — it's a dead leftover of an earlier C entry point and is not compiled.)
- `plugin-support.h` — OBS logging macros (`blog()`-based)
- `graphics-source.cpp` — OBS source implementation: allocates a Cairo surface, calls `g_scene.Render()` into it, uploads pixels to an OBS texture via `gs_texture_set_image()`. It does **not** tick — see "Key patterns"
- `shared-title.h/cpp` — `g_scene`, the process-global `Scene` owning every `Title` and the `DataPool`; `g_scene_mutex`, the single lock serializing all access to it; `TitleRow`, the dock's Qt-free per-row host metadata
- `app-config.h/cpp` — reads/writes plugin config (per-title entries, data-source pool entries) to a per-profile/scene-collection JSON file
- `app-settings.h/cpp` — reads/writes app-level settings (currently: external editor path)
- `settings-dialog.h/cpp` — app-level dialog: editor path plus the "Data Sources" tab (add/reload/remove pool entries; emits requests, does no I/O itself)
- `title-settings-dialog.h/cpp` — per-title dialog: a combo box pointing the title at one of the pool's source ids, the resulting record table/selection, and the auto-hide duration
- `graphics-dock.h/cpp` — OBS dock widget: owns `m_rows` and `g_scene`'s lifecycle (add/reload/remove titles and data sources, config load/save, profile/scene-collection switch)
- `ui-util.h/cpp` — Qt presentation helpers shared by the dock and both dialogs (`makeIconButton`, `displayNameForPath`); no pool access, no I/O
- `icons.h/cpp` — themed icon lookup for dock/dialog buttons

## Engine submodule

The engine is at `engine/` (submodule: obs-graphics-engine). Include engine headers as:

```cpp
#include "engine/scene.h"
#include "engine/title.h"
#include "engine/data-pool.h"
#include "engine/uuid.h"
// etc.
```

Do not add engine headers to `src/` — they live in the submodule.

## Key patterns

- **One global `Scene`, one global mutex.** `engine/scene.h` documents `Scene` and its `Title`s as single-threaded. The plugin has two threads that need them (the Qt UI thread and the OBS render thread), so `g_scene_mutex` is how they take turns being that thread. Every read or write of `g_scene`, or of any `Title*` it owns, happens under that lock.
- **The Scene is ticked once per frame, globally** — from the `obs_add_tick_callback` installed in `plugin-main.cpp`, not from `source_video_tick`. There may be several Graphics Source instances in a scene collection but only one `Scene`; ticking per instance would advance every animation N× per frame. Each source instance only *renders* (`g_scene.Render(cr, w, h)`) into its own Cairo surface.
- `graphics-source.cpp` owns the Cairo surface lifecycle. Surface is recreated if the OBS video canvas size changes.
- **Data is pulled, not pushed.** `DataPool` (`g_scene.Pool()`) owns every `IDataSource` and caches what it polls; it knows nothing about `Title`. A `Title` names the source it reads in `Title::dataSourceId` and pulls that cache itself in its own `Tick` (version-compared, so an unchanged source costs one integer compare). Binding is plain assignment — there is no `Bind`/`Unbind`.
- **Never call `Pool().DataBlocking()` while holding `g_scene_mutex`** — it can take seconds for a network-backed `ScriptDataSource`, and that would freeze OBS rendering. `graphics-dock.cpp`'s `onToggle` therefore fetches on a `QtConcurrent` worker with no lock held, then posts the **records** back to the UI thread and calls the three-argument `Title::TriggerIn(recordIndex, duration, records)`, which applies what it's given. Using the two-argument overload here would be the bug this replaced: that one calls `DataBlocking` itself — deliberately ignoring the cache — so the fetch would happen a second time, under the lock, and stall every frame for its duration. The pool's own registry/cache calls (`Add`/`Remove`/`Ids`/`Get`/`Data`/`Has`) are individually thread-safe and need no scene lock.
- `Title::onTriggerIn`/`onTriggerOut` subscribers fire **under `g_scene_mutex`, on the render thread**, from inside `Scene::Tick` (e.g. an auto-hide timeout or a script's `trigger_out`). They must stay non-blocking: `Qt::QueuedConnection` only, never `BlockingQueuedConnection`.
- **Identity is uuids, and the host persists them.** `Title::id` and `IDataSource::GetId()` are both uuids; `Title::name` is the human-readable name a Lua script searches with `scene.find_titles(name)`. `Scene::AddTitle` regenerates a *colliding* title id and persists nothing, so `app-config.cpp` (schema v6) stores both ids and the dock restores them — `Title::id` before `AddTitle`, and `IDataSource::SetId()` before `Pool().Add()`. That is what keeps a script's `trigger_out(title)` targeting the same title across a reload or a restart.
- `GraphicsDockWidget::clearTitles()` tears down `g_scene.Clear()` then `g_scene.Pool().Clear()` under one lock. `Scene` owns both halves now, so there is no ordering hazard between a `Title` dying and a source still referencing it.

## Adding a new source file

Add both `.h` and `.cpp` to `target_sources` in `CMakeLists.txt`.
