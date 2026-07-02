# CLAUDE.md

OBS Studio plugin that exposes a "Graphics Source" which renders a `Scene` via Cairo and uploads the result as an OBS texture each frame.

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

- `plugin-main.c` — `obs_module_load` / `obs_module_unload` (C entry point required by OBS)
- `plugin-main.cpp` — registers the OBS source type, creates the dock widget on frontend load
- `plugin-support.h` — OBS logging macros (`blog()`-based)
- `graphics-source.cpp` — OBS source implementation: allocates a Cairo surface, calls `scene.Tick()` + `scene.Render()` each frame, uploads pixels to an OBS texture via `gs_texture_set_image()`
- `shared-scene.h/cpp` — singleton `SharedScene`: holds the `Scene` instance + a `std::mutex`; both the source (render thread) and dock (UI thread) lock this to access the scene safely
- `graphics-dock.h/cpp` — OBS dock widget: file picker to load a `.json` scene, data source management table (add/remove JSON/CSV/Lua sources), triggers/animate-in/animate-out buttons
- `app-config.h/cpp` — reads/writes plugin config (scene file path, data source list) to OBS config storage

## Engine submodule

The engine is at `engine/` (submodule: obs-graphics-engine). Include engine headers as:

```cpp
#include "engine/scene.h"
#include "engine/types.hpp"
// etc.
```

Do not add engine headers to `src/` — they live in the submodule.

## Key patterns

- The OBS source tick runs on a **render thread**. Always lock `SharedScene::mutex` before accessing `SharedScene::scene`.
- `graphics-source.cpp` owns the Cairo surface lifecycle. Surface is recreated if scene dimensions change.
- Data sources (`IDataSource`) are registered in `app-config` and injected into the scene via `scene.SetDataSources()`.

## Adding a new source file

Add both `.h` and `.cpp` to `target_sources` in `CMakeLists.txt`.
