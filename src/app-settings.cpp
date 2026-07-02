/*
StreamCanvas — Animated broadcast graphics source for OBS Studio
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

#include "app-settings.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AppSettings AppSettings::Load(const std::string& settingsPath)
{
    AppSettings settings;
    std::ifstream f(settingsPath);
    if (!f.is_open())
        return settings;

    json j;
    try {
        f >> j;
    } catch (...) {
        return settings;
    }

    settings.editorPath = j.value("editor_path", "");
    return settings;
}

void AppSettings::Save(const std::string& settingsPath) const
{
    json j;
    j["editor_path"] = editorPath;

    std::ofstream f(settingsPath);
    f << j.dump(4);
}
