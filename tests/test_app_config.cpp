// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2026 Diego Lopes <diego95lopes@gmail.com>
//
// Headless console test (no OBS, no Qt): AppConfig JSON load/save and
// migration coverage, schema v6 (uuid-keyed data sources + titles).
//
//  - v6 save->load round-trip preserves every field, including ids.
//  - v5->v6 migration mints uuids for every data source and title, rewrites
//    each title's path-valued "data_source" to the matching source's minted
//    id, and drops "bind_name" entirely.
//  - v4 (no top-level "data_sources", each title carries a standalone path)
//    still backfills the pool from those paths (deduplicated) and then
//    falls through the v5->v6 step, ending up fully migrated.
//  - v3 (plain-string titles) still loads, fully migrated.
//  - a config with data sources but zero titles round-trips without losing
//    the data sources -- this was a real data-loss regression.
//  - a title whose data_source file no longer exists on disk is still
//    preserved -- AppConfig::Load deliberately does no existence check.
//  - malformed/truncated JSON and a missing file all return an empty
//    config rather than throwing.

#include "app-config.h"
#include "engine/uuid.h"
#include "test_util.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>

using test_util::Check;
using test_util::g_failures;
using test_util::MakeTempDir;
using test_util::WriteFile;

namespace {

void TestV6RoundTrip()
{
    auto dir = MakeTempDir("v6-round-trip");
    auto path = (dir / "config.json").string();

    AppConfig cfg;
    cfg.dataSources = {
        {uuid::GenerateV4(), "/home/u/feed1.lua"},
        {uuid::GenerateV4(), "/home/u/feed2.json"},
    };
    cfg.titles = {
        {uuid::GenerateV4(), "/home/u/title1.ogt", cfg.dataSources[0].id, 5.5},
        {uuid::GenerateV4(), "/home/u/title2.ogt", "", -1.0},
    };
    cfg.Save(path);

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 2, "V6RoundTrip: data_sources count preserved");
    if (loaded.dataSources.size() == 2) {
        Check(loaded.dataSources[0].id == cfg.dataSources[0].id, "V6RoundTrip: data_sources[0].id preserved");
        Check(loaded.dataSources[0].path == "/home/u/feed1.lua", "V6RoundTrip: data_sources[0].path preserved");
        Check(loaded.dataSources[1].id == cfg.dataSources[1].id, "V6RoundTrip: data_sources[1].id preserved");
        Check(loaded.dataSources[1].path == "/home/u/feed2.json", "V6RoundTrip: data_sources[1].path preserved");
    }

    Check(loaded.titles.size() == 2, "V6RoundTrip: titles count preserved");
    if (loaded.titles.size() == 2) {
        Check(loaded.titles[0].id == cfg.titles[0].id, "V6RoundTrip: titles[0].id preserved");
        Check(loaded.titles[0].path == "/home/u/title1.ogt", "V6RoundTrip: titles[0].path preserved");
        Check(loaded.titles[0].dataSourceId == cfg.dataSources[0].id, "V6RoundTrip: titles[0].dataSourceId preserved");
        Check(loaded.titles[0].duration == 5.5, "V6RoundTrip: titles[0].duration preserved");

        Check(loaded.titles[1].id == cfg.titles[1].id, "V6RoundTrip: titles[1].id preserved");
        Check(loaded.titles[1].path == "/home/u/title2.ogt", "V6RoundTrip: titles[1].path preserved");
        Check(loaded.titles[1].dataSourceId.empty(), "V6RoundTrip: titles[1].dataSourceId preserved as empty");
        Check(loaded.titles[1].duration == -1.0, "V6RoundTrip: titles[1].duration preserved");
    }
}

void TestV5ToV6Migration()
{
    auto dir = MakeTempDir("v5-migration");
    auto path = (dir / "config.json").string();

    // Hand-written v5 shape: data_sources entries have no "id" (that's the
    // discriminator), and each title's "data_source" is a *path* referencing
    // one of those entries. A stray "bind_name" must be dropped on load.
    WriteFile(path, R"JSON({
        "version": 5,
        "data_sources": [
            {"path": "/home/u/feedA.lua"},
            {"path": "/home/u/feedB.json"}
        ],
        "titles": [
            {"path": "/home/u/t1.ogt", "data_source": "/home/u/feedA.lua", "duration": -1.0, "bind_name": "a"},
            {"path": "/home/u/t2.ogt", "data_source": "/home/u/feedB.json", "duration": 3.0, "bind_name": "b"},
            {"path": "/home/u/t3.ogt", "data_source": "", "duration": -1.0, "bind_name": "c"}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 2, "V5Migration: both pool entries loaded");
    Check(loaded.titles.size() == 3, "V5Migration: all three titles loaded");
    if (loaded.dataSources.size() != 2 || loaded.titles.size() != 3)
        return;

    Check(!loaded.dataSources[0].id.empty() && uuid::IsV4(loaded.dataSources[0].id),
          "V5Migration: data_sources[0] got a minted v4 uuid");
    Check(!loaded.dataSources[1].id.empty() && uuid::IsV4(loaded.dataSources[1].id),
          "V5Migration: data_sources[1] got a minted v4 uuid");
    Check(loaded.dataSources[0].id != loaded.dataSources[1].id, "V5Migration: the two minted ids differ");

    for (auto& te : loaded.titles)
        Check(!te.id.empty() && uuid::IsV4(te.id), "V5Migration: every title got a minted v4 uuid id");

    Check(loaded.titles[0].dataSourceId == loaded.dataSources[0].id,
          "V5Migration: t1's data_source path rewritten to feedA's minted id");
    Check(loaded.titles[1].dataSourceId == loaded.dataSources[1].id,
          "V5Migration: t2's data_source path rewritten to feedB's minted id");
    Check(loaded.titles[1].duration == 3.0, "V5Migration: t2 duration preserved");
    Check(loaded.titles[2].dataSourceId.empty(), "V5Migration: t3 (empty data_source path) stays unbound");

    // Round-trip through Save and confirm bind_name never resurfaces.
    auto savedPath = (dir / "resaved.json").string();
    loaded.Save(savedPath);
    std::ifstream check(savedPath);
    std::string contents((std::istreambuf_iterator<char>(check)), std::istreambuf_iterator<char>());
    Check(contents.find("bind_name") == std::string::npos, "V5Migration: bind_name absent from the saved v6 output");
    Check(contents.find("\"version\": 6") != std::string::npos, "V5Migration: saved output is tagged version 6");
}

void TestV5UnmatchedDataSourcePathEndsUpUnbound()
{
    // A v5 title whose data_source path doesn't match any registered pool
    // entry must end up with an empty dataSourceId -- not a dangling path
    // and not an invented id.
    auto dir = MakeTempDir("v5-unmatched");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 5,
        "data_sources": [
            {"path": "/home/u/feedA.lua"}
        ],
        "titles": [
            {"path": "/home/u/t1.ogt", "data_source": "/home/u/no-such-feed.lua", "duration": -1.0}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.titles.size() == 1, "V5Unmatched: title loaded");
    if (loaded.titles.size() == 1)
        Check(loaded.titles[0].dataSourceId.empty(), "V5Unmatched: dataSourceId empty for an unmatched path");
}

void TestTwoTitlesSharingOneDataSourceResolveToSameId()
{
    auto dir = MakeTempDir("v5-shared-source");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 5,
        "data_sources": [
            {"path": "/home/u/shared.lua"}
        ],
        "titles": [
            {"path": "/home/u/t1.ogt", "data_source": "/home/u/shared.lua", "duration": -1.0},
            {"path": "/home/u/t2.ogt", "data_source": "/home/u/shared.lua", "duration": -1.0}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 1, "SharedSource: single pool entry");
    Check(loaded.titles.size() == 2, "SharedSource: both titles loaded");
    if (loaded.dataSources.size() == 1 && loaded.titles.size() == 2) {
        Check(!loaded.dataSources[0].id.empty(), "SharedSource: pool entry has a minted id");
        Check(loaded.titles[0].dataSourceId == loaded.dataSources[0].id,
              "SharedSource: t1 resolves to the shared source's id");
        Check(loaded.titles[1].dataSourceId == loaded.dataSources[0].id,
              "SharedSource: t2 resolves to the same shared source's id");
    }
}

void TestIdsStableAcrossLoadSaveLoad()
{
    // Minting must happen once, on the first migration -- not on every
    // subsequent load of an already-migrated file.
    auto dir = MakeTempDir("ids-stable");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 5,
        "data_sources": [
            {"path": "/home/u/feedA.lua"}
        ],
        "titles": [
            {"path": "/home/u/t1.ogt", "data_source": "/home/u/feedA.lua", "duration": -1.0}
        ]
    })JSON");

    AppConfig first = AppConfig::Load(path);
    auto sourceId1 = first.dataSources.empty() ? std::string() : first.dataSources[0].id;
    auto titleId1 = first.titles.empty() ? std::string() : first.titles[0].id;

    auto savedPath = (dir / "resaved.json").string();
    first.Save(savedPath);

    AppConfig second = AppConfig::Load(savedPath);
    auto sourceId2 = second.dataSources.empty() ? std::string() : second.dataSources[0].id;
    auto titleId2 = second.titles.empty() ? std::string() : second.titles[0].id;

    // Load again a third time to be doubly sure re-loading an already-v6
    // file is a fixed point.
    AppConfig third = AppConfig::Load(savedPath);
    auto sourceId3 = third.dataSources.empty() ? std::string() : third.dataSources[0].id;
    auto titleId3 = third.titles.empty() ? std::string() : third.titles[0].id;

    Check(!sourceId1.empty() && sourceId1 == sourceId2 && sourceId2 == sourceId3,
          "IdsStable: data source id stable across Load->Save->Load->Load");
    Check(!titleId1.empty() && titleId1 == titleId2 && titleId2 == titleId3,
          "IdsStable: title id stable across Load->Save->Load->Load");
}

void TestV4ToV6Migration()
{
    auto dir = MakeTempDir("v4-migration");
    auto path = (dir / "config.json").string();

    // Hand-written v4 shape: no top-level "data_sources" key at all, and
    // every title carries its own standalone "data_source" path. Two titles
    // share the same feed to exercise dedup during backfill; a third has no
    // data source at all. Falls through v4->v5 backfill then v5->v6 minting.
    WriteFile(path, R"JSON({
        "version": 4,
        "titles": [
            {"path": "/home/u/t1.ogt", "data_source": "/home/u/feedA.lua", "duration": -1.0, "bind_name": "a"},
            {"path": "/home/u/t2.ogt", "data_source": "/home/u/feedA.lua", "duration": 3.0, "bind_name": "b"},
            {"path": "/home/u/t3.ogt", "data_source": "", "duration": -1.0, "bind_name": "c"}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 1, "V4Migration: exactly one pool entry backfilled (deduplicated)");
    if (loaded.dataSources.size() == 1) {
        Check(loaded.dataSources[0].path == "/home/u/feedA.lua", "V4Migration: backfilled pool entry has the shared feed path");
        Check(!loaded.dataSources[0].id.empty() && uuid::IsV4(loaded.dataSources[0].id),
              "V4Migration: backfilled pool entry got a minted v4 uuid");
    }

    Check(loaded.titles.size() == 3, "V4Migration: all three titles loaded");
    if (loaded.titles.size() == 3 && loaded.dataSources.size() == 1) {
        Check(loaded.titles[0].dataSourceId == loaded.dataSources[0].id, "V4Migration: t1 keeps its data_source binding");
        Check(!loaded.titles[0].id.empty() && uuid::IsV4(loaded.titles[0].id), "V4Migration: t1 got a minted id");
        Check(loaded.titles[1].dataSourceId == loaded.dataSources[0].id, "V4Migration: t2 keeps its data_source binding");
        Check(loaded.titles[1].duration == 3.0, "V4Migration: t2 duration preserved");
        Check(loaded.titles[2].dataSourceId.empty(), "V4Migration: t3 has no data_source binding");
        Check(!loaded.titles[2].id.empty() && uuid::IsV4(loaded.titles[2].id), "V4Migration: t3 got a minted id");
    }
}

void TestV3PlainStringTitles()
{
    auto dir = MakeTempDir("v3-plain-strings");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 3,
        "titles": ["/home/u/old1.ogt", "/home/u/old2.ogt"]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.empty(), "V3PlainStrings: no data sources (v3 titles never carried one)");
    Check(loaded.titles.size() == 2, "V3PlainStrings: both plain-string titles loaded");
    if (loaded.titles.size() == 2) {
        Check(loaded.titles[0].path == "/home/u/old1.ogt", "V3PlainStrings: titles[0].path");
        Check(loaded.titles[0].dataSourceId.empty(), "V3PlainStrings: titles[0].dataSourceId defaults empty");
        Check(loaded.titles[0].duration == -1.0, "V3PlainStrings: titles[0].duration defaults -1.0");
        Check(!loaded.titles[0].id.empty() && uuid::IsV4(loaded.titles[0].id), "V3PlainStrings: titles[0] got a minted v4 uuid");
        Check(loaded.titles[1].path == "/home/u/old2.ogt", "V3PlainStrings: titles[1].path");
        Check(loaded.titles[0].id != loaded.titles[1].id, "V3PlainStrings: the two minted ids differ");
    }
}

void TestDataSourcesSurviveZeroTitles()
{
    // Regression: a config that has data sources registered but no titles
    // bound to any of them yet (e.g. the user just added a source from the
    // Data Sources tab and hasn't attached a title) must not lose those
    // sources on the next save/load cycle.
    auto dir = MakeTempDir("zero-titles");
    auto path = (dir / "config.json").string();

    AppConfig cfg;
    cfg.dataSources = {{uuid::GenerateV4(), "/home/u/onlyfeed.lua"}};
    // cfg.titles left empty.
    cfg.Save(path);

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 1, "ZeroTitles: data source survives round-trip with no titles present");
    if (loaded.dataSources.size() == 1) {
        Check(loaded.dataSources[0].path == "/home/u/onlyfeed.lua", "ZeroTitles: data source path intact");
        Check(loaded.dataSources[0].id == cfg.dataSources[0].id, "ZeroTitles: data source id intact");
    }
    Check(loaded.titles.empty(), "ZeroTitles: titles list correctly stays empty");
}

void TestTitleWithMissingDataSourceFilePreserved()
{
    // AppConfig::Load deliberately performs no filesystem::exists() check on
    // either a title's path or its data_source path -- that's a host-level
    // concern (graphics-dock.cpp filters missing title files at load time).
    // A dangling data_source reference must still come through intact so
    // the host can decide what to do with it (e.g. show an error, or just
    // leave the title unbound).
    auto dir = MakeTempDir("missing-file");
    auto path = (dir / "config.json").string();

    AppConfig cfg;
    auto dsId = uuid::GenerateV4();
    cfg.dataSources = {{dsId, "/nonexistent/feed.lua"}};
    cfg.titles = {{uuid::GenerateV4(), "/home/u/title.ogt", dsId, -1.0}};
    cfg.Save(path);

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.titles.size() == 1, "MissingFile: title preserved despite nonexistent data_source path");
    if (loaded.titles.size() == 1)
        Check(loaded.titles[0].dataSourceId == dsId,
              "MissingFile: dataSourceId preserved verbatim, no existence check");

    Check(loaded.dataSources.size() == 1, "MissingFile: data source entry preserved for a path that doesn't exist on disk");
    if (loaded.dataSources.size() == 1)
        Check(loaded.dataSources[0].path == "/nonexistent/feed.lua", "MissingFile: data source path matches");
}

void TestMalformedJsonReturnsEmptyConfig()
{
    auto dir = MakeTempDir("malformed-json");
    auto path = (dir / "config.json").string();

    WriteFile(path, "{ this is not valid json, it just trails off ");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.empty(), "MalformedJson: dataSources empty rather than throwing");
    Check(loaded.titles.empty(), "MalformedJson: titles empty rather than throwing");
}

void TestTruncatedJsonReturnsEmptyConfig()
{
    auto dir = MakeTempDir("truncated-json");
    auto path = (dir / "config.json").string();

    // A syntactically-started-but-cut-off document (as if the process died
    // mid-write) -- must not throw past Load().
    WriteFile(path, R"JSON({"version": 6, "data_sources": [{"id": "x", "path": "/home/u/feed)JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.empty(), "TruncatedJson: dataSources empty rather than throwing");
    Check(loaded.titles.empty(), "TruncatedJson: titles empty rather than throwing");
}

void TestMissingFileReturnsEmptyConfig()
{
    auto dir = MakeTempDir("missing-config-file");
    auto path = (dir / "does_not_exist_at_all.json").string();

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.empty(), "MissingConfigFile: dataSources empty for a nonexistent config path");
    Check(loaded.titles.empty(), "MissingConfigFile: titles empty for a nonexistent config path");
}

void TestTitlesArrayMixingPlainStringsAndObjects()
{
    // The per-entry parse loop branches on entry.is_string() vs
    // entry.is_object() independently for each element -- nothing stops a
    // single "titles" array from mixing both shapes (e.g. a v3 leftover
    // entry a user never re-saved, sitting next to a newer object entry).
    // Both must come through correctly in one Load() call.
    auto dir = MakeTempDir("mixed-titles-array");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 6,
        "data_sources": [],
        "titles": [
            "/home/u/plain.ogt",
            {"id": "33333333-3333-4333-8333-333333333333", "path": "/home/u/obj.ogt", "data_source": "", "duration": 2.5}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.titles.size() == 2, "MixedTitlesArray: both the plain-string and object entries loaded");
    if (loaded.titles.size() != 2)
        return;

    Check(loaded.titles[0].path == "/home/u/plain.ogt", "MixedTitlesArray: plain-string entry's path");
    Check(loaded.titles[0].duration == -1.0, "MixedTitlesArray: plain-string entry defaults duration to -1.0");
    Check(!loaded.titles[0].id.empty() && uuid::IsV4(loaded.titles[0].id),
          "MixedTitlesArray: plain-string entry got a minted v4 uuid");

    Check(loaded.titles[1].path == "/home/u/obj.ogt", "MixedTitlesArray: object entry's path");
    Check(loaded.titles[1].id == "33333333-3333-4333-8333-333333333333", "MixedTitlesArray: object entry's own id preserved");
    Check(loaded.titles[1].duration == 2.5, "MixedTitlesArray: object entry's duration preserved");
}

void TestV6HandEditedTitleMissingIdGetsMintedWithoutDisturbingDataSources()
{
    // Contrast case for the bug demonstrated below: when only a *title* is
    // missing its "id" (every data_sources entry still carries a valid,
    // non-empty id), the v5-vs-v6 discriminator correctly stays on the v6
    // path (it only sniffs data_sources), so the title's id gets minted in
    // isolation and its id-valued "data_source" reference is left untouched.
    auto dir = MakeTempDir("v6-title-missing-id");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 6,
        "data_sources": [
            {"id": "11111111-1111-4111-8111-111111111111", "path": "/home/u/feedA.lua"}
        ],
        "titles": [
            {"path": "/home/u/t1.ogt", "data_source": "11111111-1111-4111-8111-111111111111", "duration": -1.0}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 1, "V6TitleMissingId: data source loaded");
    if (loaded.dataSources.size() == 1)
        Check(loaded.dataSources[0].id == "11111111-1111-4111-8111-111111111111",
              "V6TitleMissingId: pre-existing data source id left untouched");

    Check(loaded.titles.size() == 1, "V6TitleMissingId: title loaded");
    if (loaded.titles.size() == 1) {
        Check(!loaded.titles[0].id.empty() && uuid::IsV4(loaded.titles[0].id),
              "V6TitleMissingId: title with a missing id got one minted");
        Check(loaded.titles[0].dataSourceId == "11111111-1111-4111-8111-111111111111",
              "V6TitleMissingId: title's pre-existing id-valued data_source binding survives intact");
    }
}

void TestV6HandEditedMissingDataSourceIdRepairsOnlyThatEntry()
{
    // Regression guard on the v5-vs-v6 discriminator in src/app-config.cpp.
    // It used to be:
    //
    //   isV6 = std::all_of(cfg.dataSources..., [](de){ return !de.id.empty(); });
    //
    // which meant a v6 file that was fully valid except that ONE
    // data_sources entry was missing its "id" (a hand-added entry, a
    // truncated write) made `all_of` false and routed the WHOLE file through
    // the v5->v6 migration branch. That branch re-mints an id for *every*
    // data source -- including ones already carrying a perfectly good v6 id
    // that a Lua script's trigger_out(title) and the dock's persisted
    // bindings both depend on -- and then rewires every title's
    // "data_source" as if it were a v5 *path*, which in a v6 file it isn't,
    // so every binding silently dropped to "". One malformed entry took out
    // the whole file's identity.
    //
    // It's now `any_of`: a single id anywhere proves the file is v6 (no v5
    // shape ever carried one), so the file stays v6 and the defensive
    // minting pass repairs just the broken entry. This test pins that down.
    auto dir = MakeTempDir("v6-partial-ds-id");
    auto path = (dir / "config.json").string();

    const std::string originalGoodId = "11111111-1111-4111-8111-111111111111";
    WriteFile(path, R"JSON({
        "version": 6,
        "data_sources": [
            {"id": ")JSON" + originalGoodId + R"JSON(", "path": "/home/u/feedA.lua"},
            {"path": "/home/u/feedB.json"}
        ],
        "titles": [
            {"id": "22222222-2222-4222-8222-222222222222", "path": "/home/u/t1.ogt", "data_source": ")JSON" +
                        originalGoodId + R"JSON(", "duration": -1.0}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 2, "V6PartialDsId: both data sources still loaded");
    Check(loaded.titles.size() == 1, "V6PartialDsId: title still loaded");
    if (loaded.dataSources.size() != 2 || loaded.titles.size() != 1)
        return;

    // The entry that already had a valid id keeps it, untouched.
    Check(loaded.dataSources[0].id == originalGoodId,
          "V6PartialDsId: the already-valid data source id survives a sibling entry's missing id");

    // The broken entry -- and only it -- gets repaired with a fresh v4 uuid.
    Check(!loaded.dataSources[1].id.empty() && uuid::IsV4(loaded.dataSources[1].id),
          "V6PartialDsId: the id-less data source got a freshly minted v4 uuid");
    Check(loaded.dataSources[1].id != originalGoodId,
          "V6PartialDsId: the minted id is distinct from its sibling's");

    // And the title's id-valued binding still resolves, rather than being
    // rewired as if it were a v5 path and dropped to empty.
    Check(loaded.titles[0].dataSourceId == originalGoodId,
          "V6PartialDsId: the title's id-valued data_source binding is preserved verbatim");
    Check(loaded.titles[0].id == "22222222-2222-4222-8222-222222222222",
          "V6PartialDsId: the title's own id is preserved verbatim");
}

void TestVersionFallbackEmptyDataSourcesArray()
{
    // When "data_sources" is present but has zero entries, the shape-sniff
    // has nothing to examine (std::all_of over an empty range is
    // vacuously true, so the code explicitly special-cases this rather than
    // silently mis-detecting v5 as v6). It falls back to reading the
    // top-level "version" integer -- documented as the one case where doing
    // so is unambiguous. Exercise both sides of that fallback.
    {
        // version < 6 with an empty data_sources array: falls back to "not
        // v6", so titles' path-valued "data_source" gets rewired via an
        // (empty) path->id map -- i.e. it's cleared, since nothing can match.
        auto dir = MakeTempDir("version-fallback-v5-empty-ds");
        auto path = (dir / "config.json").string();
        WriteFile(path, R"JSON({
            "version": 5,
            "data_sources": [],
            "titles": [
                {"path": "/home/u/t1.ogt", "data_source": "/home/u/orphan-path.lua", "duration": -1.0}
            ]
        })JSON");

        AppConfig loaded = AppConfig::Load(path);
        Check(loaded.dataSources.empty(), "VersionFallback-v5: no data sources to backfill against");
        Check(loaded.titles.size() == 1, "VersionFallback-v5: title loaded");
        if (loaded.titles.size() == 1)
            Check(loaded.titles[0].dataSourceId.empty(),
                  "VersionFallback-v5: version<6 fallback treats data_source as an unmatched path -> cleared");
    }
    {
        // version >= 6 with an empty data_sources array: falls back to "is
        // v6", so a title's "data_source" is trusted as already being an id
        // and passed through verbatim, even though it doesn't resolve to any
        // entry in the (empty) data_sources array.
        auto dir = MakeTempDir("version-fallback-v6-empty-ds");
        auto path = (dir / "config.json").string();
        WriteFile(path, R"JSON({
            "version": 6,
            "data_sources": [],
            "titles": [
                {"id": "44444444-4444-4444-8444-444444444444", "path": "/home/u/t1.ogt", "data_source": "some-stale-id", "duration": -1.0}
            ]
        })JSON");

        AppConfig loaded = AppConfig::Load(path);
        Check(loaded.dataSources.empty(), "VersionFallback-v6: no data sources present");
        Check(loaded.titles.size() == 1, "VersionFallback-v6: title loaded");
        if (loaded.titles.size() == 1)
            Check(loaded.titles[0].dataSourceId == "some-stale-id",
                  "VersionFallback-v6: version>=6 fallback trusts data_source as an id and preserves it verbatim");
    }
}

void TestV5DuplicateDataSourcePathsKeepSeparateEntriesLastOneWinsBinding()
{
    // Nothing in the v5 load path deduplicates "data_sources" entries
    // themselves (only the v4 title-path backfill dedupes). Two entries
    // sharing an identical path both survive as distinct rows with distinct
    // minted ids -- but since the path->id rewrite map is keyed by path, a
    // title referencing that shared path resolves to whichever entry was
    // inserted into the map last (array order), not necessarily the first.
    // This documents that actual last-write-wins resolution.
    auto dir = MakeTempDir("v5-duplicate-ds-paths");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 5,
        "data_sources": [
            {"path": "/home/u/dup.lua"},
            {"path": "/home/u/dup.lua"}
        ],
        "titles": [
            {"path": "/home/u/t1.ogt", "data_source": "/home/u/dup.lua", "duration": -1.0}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == 2, "DuplicateDsPaths: both duplicate-path entries kept as separate rows");
    Check(loaded.titles.size() == 1, "DuplicateDsPaths: title loaded");
    if (loaded.dataSources.size() != 2 || loaded.titles.size() != 1)
        return;

    Check(loaded.dataSources[0].id != loaded.dataSources[1].id,
          "DuplicateDsPaths: the two duplicate-path entries got distinct minted ids");
    Check(loaded.titles[0].dataSourceId == loaded.dataSources[1].id,
          "DuplicateDsPaths: title's shared-path binding resolves to the LAST matching entry in the array");
}

void TestV6ObjectTitleMissingDurationDefaultsToMinusOne()
{
    // A v6-shaped title object that simply omits the "duration" key
    // altogether (not merely "null" or v3's plain-string shape, which never
    // has a "duration" key to omit in the first place) must still default
    // to -1.0 via entry.value("duration", -1.0).
    auto dir = MakeTempDir("v6-title-missing-duration");
    auto path = (dir / "config.json").string();

    WriteFile(path, R"JSON({
        "version": 6,
        "data_sources": [],
        "titles": [
            {"id": "55555555-5555-4555-8555-555555555555", "path": "/home/u/nodur.ogt", "data_source": ""}
        ]
    })JSON");

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.titles.size() == 1, "MissingDuration: title loaded");
    if (loaded.titles.size() == 1)
        Check(loaded.titles[0].duration == -1.0, "MissingDuration: absent 'duration' key defaults to -1.0");
}

void TestMintedIdsUniqueAtScale()
{
    // The pairwise "the two minted ids differ" checks elsewhere only ever
    // compare 2 entries. Mint at a larger scale (distinct data sources and
    // distinct titles in one v5 file) and verify every single minted id --
    // across BOTH categories, not just within each -- is pairwise unique.
    auto dir = MakeTempDir("minted-ids-scale");
    auto path = (dir / "config.json").string();

    constexpr int kCount = 25;
    std::ostringstream dsArr, titleArr;
    for (int i = 0; i < kCount; ++i) {
        if (i)
            dsArr << ",";
        dsArr << R"({"path": "/home/u/feed)" << i << R"(.lua"})";
        if (i)
            titleArr << ",";
        titleArr << R"({"path": "/home/u/t)" << i << R"(.ogt", "data_source": "/home/u/feed)" << i
                 << R"(.lua", "duration": -1.0})";
    }
    std::ostringstream doc;
    doc << R"({"version": 5, "data_sources": [)" << dsArr.str() << R"(], "titles": [)" << titleArr.str() << "]}";
    WriteFile(path, doc.str());

    AppConfig loaded = AppConfig::Load(path);

    Check(loaded.dataSources.size() == kCount, "MintedIdsScale: all data sources loaded");
    Check(loaded.titles.size() == kCount, "MintedIdsScale: all titles loaded");
    if (loaded.dataSources.size() != (size_t)kCount || loaded.titles.size() != (size_t)kCount)
        return;

    std::set<std::string> allIds;
    for (auto& de : loaded.dataSources)
        allIds.insert(de.id);
    for (auto& te : loaded.titles)
        allIds.insert(te.id);

    Check(allIds.size() == (size_t)(2 * kCount),
          "MintedIdsScale: all minted ids (data sources + titles combined) are pairwise unique, no collisions");

    bool everyBindingResolved = true;
    for (int i = 0; i < kCount; ++i) {
        if (loaded.titles[(size_t)i].dataSourceId != loaded.dataSources[(size_t)i].id)
            everyBindingResolved = false;
    }
    Check(everyBindingResolved, "MintedIdsScale: every title's binding resolved to its matching data source's minted id");
}

}  // namespace

int main()
{
    TestV6RoundTrip();
    TestV5ToV6Migration();
    TestV5UnmatchedDataSourcePathEndsUpUnbound();
    TestTwoTitlesSharingOneDataSourceResolveToSameId();
    TestIdsStableAcrossLoadSaveLoad();
    TestV4ToV6Migration();
    TestV3PlainStringTitles();
    TestDataSourcesSurviveZeroTitles();
    TestTitleWithMissingDataSourceFilePreserved();
    TestMalformedJsonReturnsEmptyConfig();
    TestTruncatedJsonReturnsEmptyConfig();
    TestMissingFileReturnsEmptyConfig();
    TestTitlesArrayMixingPlainStringsAndObjects();
    TestV6HandEditedTitleMissingIdGetsMintedWithoutDisturbingDataSources();
    TestV6HandEditedMissingDataSourceIdRepairsOnlyThatEntry();
    TestVersionFallbackEmptyDataSourcesArray();
    TestV5DuplicateDataSourcePathsKeepSeparateEntriesLastOneWinsBinding();
    TestV6ObjectTitleMissingDurationDefaultsToMinusOne();
    TestMintedIdsUniqueAtScale();

    if (g_failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
