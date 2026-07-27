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

#include "engine/uuid.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_map>

using json = nlohmann::json;

namespace {

// Pre-id-resolution parse of a "titles" entry. A v3/v4/v5 file's
// "data_source" field holds a *path*; a v6 file's holds an *id* directly.
// Which one it is gets resolved by the migration step below, once we know
// the schema flavor -- so this struct just carries the raw string through.
struct RawTitle {
    std::string id;             // may be empty (v3/v4/v5, or a hand-edited v6 file)
    std::string path;
    std::string dataSourceRaw;  // a path pre-v6, an id at v6
    double duration{-1.0};
};

}  // namespace

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

    const bool hadTopLevelDataSources = j.contains("data_sources") && j["data_sources"].is_array();

    if (hadTopLevelDataSources) {
        for (auto& entry : j["data_sources"]) {
            if (!entry.is_object())
                continue;
            DataSourceEntry de;
            de.id = entry.value("id", "");
            de.path = entry.value("path", "");
            cfg.dataSources.push_back(de);
        }
    }

    std::vector<RawTitle> rawTitles;
    if (j.contains("titles") && j["titles"].is_array()) {
        for (auto& entry : j["titles"]) {
            if (entry.is_string()) {
                // v3 compat: plain string path, nothing else to carry.
                RawTitle rt;
                rt.path = entry.get<std::string>();
                rawTitles.push_back(std::move(rt));
            } else if (entry.is_object()) {
                RawTitle rt;
                rt.id = entry.value("id", "");
                rt.path = entry.value("path", "");
                rt.dataSourceRaw = entry.value("data_source", "");
                rt.duration = entry.value("duration", -1.0);
                // "bind_name" (v3/v4/v5) is intentionally never read here --
                // uuids replace the whole anti-recycling ledger it implemented.
                rawTitles.push_back(std::move(rt));
            }
        }
    }

    // v4-era backfill: no top-level "data_sources" key at all means every
    // title's "data_source" was a standalone file path rather than a pool
    // reference. Backfill cfg.dataSources from those paths, deduplicated, so
    // existing bindings keep working with no user action. This predates (and
    // is orthogonal to) the id-minting migration below -- it just produces
    // path-only DataSourceEntry rows for that step to mint ids for.
    if (!hadTopLevelDataSources) {
        for (auto& rt : rawTitles) {
            if (rt.dataSourceRaw.empty())
                continue;
            bool present = std::any_of(cfg.dataSources.begin(), cfg.dataSources.end(),
                                       [&](const DataSourceEntry& de) { return de.path == rt.dataSourceRaw; });
            if (!present)
                cfg.dataSources.push_back({"", rt.dataSourceRaw});
        }
    }

    // v5->v6 discriminator: chosen by sniffing shape, not the top-level
    // "version" int, for the same self-healing reason the v3/v4 sniffs above
    // do -- a hand-edited file with a stale/wrong "version" number still
    // migrates correctly. v6 always writes a non-empty "id" on every
    // data_sources entry; v5 *never* wrote one at all (only "path"). So a
    // single id anywhere in the array is proof the file is v6.
    //
    // any_of, not all_of, and the difference matters. all_of would let one
    // entry missing its id (a hand-added source, a truncated write) condemn
    // the whole file to the v5 branch below, which re-mints every *other*
    // already-valid id and then rewires every title's "data_source" as if it
    // held a path -- in a v6 file it holds an id, so nothing matches and
    // every binding silently drops to empty. One malformed entry would take
    // out the whole file's identity. With any_of, the file stays v6 and the
    // defensive minting pass further down repairs just the entry that's
    // actually broken; every other id and every title binding survives. The
    // inverse mistake (a v5 file misread as v6) can't happen, because there
    // is no v5 shape that carries an id.
    //
    // The only case shape can't answer is zero data_sources entries (nothing
    // to sniff) *and* a top-level "data_sources" key present at all (so we're
    // not in the v3/v4 backfill path above) -- there we fall back to the
    // "version" int, which is the only case where reading it is unambiguous.
    bool isV6 = true;
    if (!cfg.dataSources.empty()) {
        isV6 = std::any_of(cfg.dataSources.begin(), cfg.dataSources.end(),
                            [](const DataSourceEntry& de) { return !de.id.empty(); });
    } else if (hadTopLevelDataSources) {
        isV6 = j.value("version", 6) >= 6;
    }

    if (!isV6) {
        // Mint an id for every data source, and rewrite each title's
        // path-valued "data_source" to the matching id (empty if unmatched).
        std::unordered_map<std::string, std::string> pathToId;
        for (auto& de : cfg.dataSources) {
            de.id = uuid::GenerateV4();
            pathToId[de.path] = de.id;
        }
        for (auto& rt : rawTitles) {
            auto it = pathToId.find(rt.dataSourceRaw);
            rt.dataSourceRaw = (it != pathToId.end()) ? it->second : "";
        }
    }

    // Defends a hand-edited (or otherwise malformed) v6 config: any entry
    // whose id came back missing/empty still gets minted, so a fully-formed
    // config is guaranteed regardless of the branch above.
    for (auto& de : cfg.dataSources) {
        if (de.id.empty())
            de.id = uuid::GenerateV4();
    }

    for (auto& rt : rawTitles) {
        TitleEntry te;
        te.id = rt.id.empty() ? uuid::GenerateV4() : rt.id;
        te.path = rt.path;
        te.dataSourceId = rt.dataSourceRaw;
        te.duration = rt.duration;
        cfg.titles.push_back(std::move(te));
    }

    return cfg;
}

void AppConfig::Save(const std::string& configPath) const
{
    json j;
    j["version"] = 6;

    json dsArr = json::array();
    for (auto& de : dataSources) {
        json e;
        e["id"] = de.id;
        e["path"] = de.path;
        dsArr.push_back(e);
    }
    j["data_sources"] = dsArr;

    json arr = json::array();
    for (auto& te : titles) {
        json e;
        e["id"] = te.id;
        e["path"] = te.path;
        e["data_source"] = te.dataSourceId;
        e["duration"] = te.duration;
        arr.push_back(e);
    }
    j["titles"] = arr;

    std::ofstream f(configPath);
    f << j.dump(4);
}
