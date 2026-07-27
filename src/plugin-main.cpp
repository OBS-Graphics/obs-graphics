/*
StreamCanvas
Copyright (C) 2026 Diego Lopes diego95lopes@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "graphics-dock.h"
#include "icons.h"
#include "shared-title.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <filesystem>
#include <mutex>
#include <string>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

extern struct obs_source_info gGraphicsSourceInfo;

// Owned by the OBS frontend (Qt) once added as a dock; only ever one instance.
static GraphicsDockWidget* gDock = nullptr;

// There is exactly one g_scene but potentially many Graphics Source instances
// in a scene collection. Ticking the Scene from source_video_tick would
// advance every animation once per instance per frame (2 instances = 2x
// speed); ticking it once here, globally, keeps it advancing at a single,
// correct rate no matter how many sources render it.
static void scene_tick(void*, float seconds)
{
    std::lock_guard<std::mutex> lock(g_scene_mutex);
    g_scene.Tick(seconds);
}

// Titles are persisted per profile + scene collection, so the dock needs to
// reload whenever OBS finishes starting up or the user switches either one.
static void onFrontendEvent(enum obs_frontend_event event, void*)
{
    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
    case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        if (gDock)
            gDock->reloadForCurrentContext();
        break;
    default:
        break;
    }
}

bool obs_module_load(void)
{
    obs_register_source(&gGraphicsSourceInfo);

    initIcons();

    auto* mainWin = static_cast<QWidget*>(obs_frontend_get_main_window());

    char* cfgDirRaw = obs_module_config_path("");
    std::string configDir(cfgDirRaw);
    bfree(cfgDirRaw);
    std::filesystem::create_directories(configDir);

    gDock = new GraphicsDockWidget(mainWin, std::move(configDir));
    obs_frontend_add_dock_by_id("stream-canvas-dock", "Live Graphics Titles", gDock);

    obs_frontend_add_event_callback(onFrontendEvent, nullptr);
    obs_add_tick_callback(scene_tick, nullptr);

    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    // Must come before the dock (and thus g_scene) goes away: scene_tick
    // dereferences g_scene, so it can't still be registered afterward.
    obs_remove_tick_callback(scene_tick, nullptr);
    obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
    obs_frontend_remove_dock("stream-canvas-dock");
    gDock = nullptr;
    obs_log(LOG_INFO, "plugin unloaded");
}
