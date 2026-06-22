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

#pragma once

#include <string>
#include <unordered_map>

struct DataSourceEntry {
    std::string path;
    std::string type; // "json", "csv", or "lua"
};

struct AppConfig {
    std::string scenePath;
    std::unordered_map<std::string, DataSourceEntry> dataSources; // graphic id → entry
    std::unordered_map<std::string, int> selectedRecords;         // graphic id → index

    static AppConfig Load(const std::string& configPath);
    void Save(const std::string& configPath) const;
};
