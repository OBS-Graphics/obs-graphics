#include "app-config.h"

#include <nlohmann/json.hpp>
#include <fstream>

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

void AppConfig::Save(const std::string& configPath) const
{
    json j;
    j["scene_path"] = scenePath;

    json dsj = json::object();
    for (auto& [id, ds] : dataSources)
        dsj[id] = {{"path", ds.path}, {"type", ds.type}};
    j["data_sources"] = dsj;

    json recj = json::object();
    for (auto& [id, idx] : selectedRecords)
        recj[id] = idx;
    j["selected_records"] = recj;

    std::ofstream f(configPath);
    f << j.dump(4);
}
