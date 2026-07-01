# StreamCanvas

OBS Studio plugin that renders animated broadcast graphics overlays as a source. Scenes are authored in the [StreamCanvas Editor](https://github.com/OBS-Graphics/obs-graphics-editor) and loaded at runtime.

## Dependencies

- OBS Studio (headers + `obs-frontend-api`)
- Qt6 (Widgets, Core)
- Cairo + Pango (via the engine submodule)
- Lua 5.x (via the engine submodule)
- CMake 3.28+

## Building

```bash
git submodule update --init
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Requires OBS development headers installed system-wide (or pointed to via `libobs_DIR`).

## Structure

```
src/                    OBS plugin sources
  plugin-main.c/cpp     Module entry point + dock registration
  graphics-source.cpp   OBS source implementation (Cairo → OBS texture)
  shared-scene.h/cpp    Global scene mutex + shared Scene instance
  graphics-dock.h/cpp   OBS dock widget (scene/data-source management)
  app-config.h/cpp      Plugin configuration (scene path, data sources)
  plugin-support.h      OBS logging helpers
engine/                 Submodule: obs-graphics-engine
cmake/                  OBS cmake build infrastructure
```

## Related repos

- [StreamCanvas Engine](https://github.com/OBS-Graphics/obs-graphics-engine) — rendering engine (submodule)
- [StreamCanvas Editor](https://github.com/OBS-Graphics/obs-graphics-editor) — standalone scene editor

## AI disclosure

Parts of this project were built with the help of AI coding tools. All AI-assisted changes were reviewed before merging.
