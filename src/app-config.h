#pragma once

#include <string>
#include <unordered_map>

struct DataSourceEntry {
    std::string path;
    std::string type; // "json" or "csv"
};

struct AppConfig {
    std::string scenePath;
    std::unordered_map<std::string, DataSourceEntry> dataSources; // graphic id → entry
    std::unordered_map<std::string, int> selectedRecords;         // graphic id → index

    static AppConfig Load(const std::string& configPath);
    void Save(const std::string& configPath) const;
};
