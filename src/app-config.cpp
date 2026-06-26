/*
obs-graphics — Animated broadcast graphics source for OBS Studio
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

static SceneConfig sceneConfigFromJson(const json& j)
{
    SceneConfig cfg;
    cfg.scenePath = j.value("scene_path", "");

    if (j.contains("data_sources") && j["data_sources"].is_object()) {
        for (auto& [id, dsj] : j["data_sources"].items()) {
            if (!dsj.is_object())
                continue;
            cfg.dataSources[id] = {dsj.value("path", ""), dsj.value("type", "json")};
        }
    }

    if (j.contains("selected_records") && j["selected_records"].is_object()) {
        for (auto& [id, idx] : j["selected_records"].items()) {
            if (idx.is_number_integer())
                cfg.selectedRecords[id] = idx.get<int>();
        }
    }

    return cfg;
}

static json sceneConfigToJson(const SceneConfig& cfg)
{
    json j;
    j["scene_path"] = cfg.scenePath;

    json dsj = json::object();
    for (auto& [id, ds] : cfg.dataSources)
        dsj[id] = {{"path", ds.path}, {"type", ds.type}};
    j["data_sources"] = dsj;

    json recj = json::object();
    for (auto& [id, idx] : cfg.selectedRecords)
        recj[id] = idx;
    j["selected_records"] = recj;

    return j;
}

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

    // v1 migration: single scene_path at root → wrap as scenes[0]
    if (j.contains("scene_path")) {
        cfg.scenes.push_back(sceneConfigFromJson(j));
        cfg.activeTabIndex = 0;
        return cfg;
    }

    cfg.activeTabIndex = j.value("active_tab", 0);

    if (j.contains("scenes") && j["scenes"].is_array()) {
        for (auto& sj : j["scenes"]) {
            if (sj.is_object())
                cfg.scenes.push_back(sceneConfigFromJson(sj));
        }
    }

    if (cfg.activeTabIndex >= (int)cfg.scenes.size())
        cfg.activeTabIndex = 0;

    return cfg;
}

void AppConfig::Save(const std::string& configPath) const
{
    json j;
    j["version"] = 2;
    j["active_tab"] = activeTabIndex;

    json scenesArr = json::array();
    for (auto& sc : scenes)
        scenesArr.push_back(sceneConfigToJson(sc));
    j["scenes"] = scenesArr;

    std::ofstream f(configPath);
    f << j.dump(4);
}
