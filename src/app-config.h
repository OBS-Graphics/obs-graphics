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

#pragma once

#include <string>
#include <vector>

struct TitleEntry {
    std::string id;            // uuid; minted by Load if missing (e.g. a pre-v6 config)
    std::string path;
    std::string dataSourceId;  // pool source uuid; empty = unbound
    double duration{-1.0};     // seconds; -1.0 = auto-hide disabled
};

struct DataSourceEntry {
    std::string id;    // uuid; minted by Load if missing (e.g. a pre-v6 config)
    std::string path;
};

struct AppConfig {
    std::vector<DataSourceEntry> dataSources;
    std::vector<TitleEntry> titles;

    // Always returns a fully-migrated, fully-formed (every id non-empty and a
    // valid v4 uuid) config -- callers never need to backfill an id
    // themselves. Malformed/truncated JSON or a missing file returns an
    // empty AppConfig rather than throwing.
    static AppConfig Load(const std::string& configPath);
    void Save(const std::string& configPath) const;
};
