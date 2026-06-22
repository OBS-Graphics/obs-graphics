/*
obs-graphics
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
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <filesystem>
#include <string>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

extern struct obs_source_info gGraphicsSourceInfo;

bool obs_module_load(void)
{
    obs_register_source(&gGraphicsSourceInfo);

    auto* mainWin = static_cast<QWidget*>(obs_frontend_get_main_window());

    char* cfgRaw = obs_module_config_path("config.json");
    std::string configPath(cfgRaw);
    bfree(cfgRaw);
    std::filesystem::create_directories(std::filesystem::path(configPath).parent_path());

    auto* dock = new GraphicsDockWidget(mainWin, std::move(configPath));
    obs_frontend_add_dock_by_id("obs-graphics-dock", "Graphics", dock);

    obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_dock("obs-graphics-dock");
    obs_log(LOG_INFO, "plugin unloaded");
}
