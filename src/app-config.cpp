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

#include "app-config.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AppConfig AppConfig::Load(const std::string& configPath)
{
    AppConfig cfg;
    std::ifstream f(configPath);
    if (!f.is_open())
        return cfg;

    json j;
    try {
        f >> j;
    } catch (...) {
        return cfg;
    }

    if (j.contains("titles") && j["titles"].is_array()) {
        for (auto& entry : j["titles"]) {
            if (entry.is_string()) {
                // v3 compat: plain string path
                cfg.titles.push_back({entry.get<std::string>(), ""});
            } else if (entry.is_object()) {
                TitleEntry te;
                te.path = entry.value("path", "");
                te.dataSourcePath = entry.value("data_source", "");
                cfg.titles.push_back(te);
            }
        }
    }

    return cfg;
}

void AppConfig::Save(const std::string& configPath) const
{
    json j;
    j["version"] = 4;

    json arr = json::array();
    for (auto& te : titles) {
        json e;
        e["path"] = te.path;
        e["data_source"] = te.dataSourcePath;
        arr.push_back(e);
    }
    j["titles"] = arr;

    std::ofstream f(configPath);
    f << j.dump(4);
}
