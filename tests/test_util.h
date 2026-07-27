// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (c) 2026 Diego Lopes <diego95lopes@gmail.com>
//
// Shared test-only helpers for the plugin's headless unit tests
// (test_app_config.cpp). Mirrors the style of engine/tests/script_test_util.h
// so the two repos' test suites read the same way, but is not shared code --
// this one has no engine dependency.

#pragma once

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

namespace test_util {

inline int g_failures = 0;

inline void Check(bool cond, const char* what)
{
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

// A fresh, empty directory under the system temp dir, scoped to one test
// binary invocation (thread-id + clock suffixed so concurrent ctest runs
// don't collide) -- portable, no platform-specific PID header needed.
inline std::filesystem::path MakeTempDir(const std::string& tag)
{
    std::ostringstream suffix;
    suffix << std::this_thread::get_id() << "-"
           << std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = std::filesystem::temp_directory_path() /
               ("obs-graphics-test-" + tag + "-" + suffix.str());
    std::filesystem::create_directories(dir);
    return dir;
}

inline void WriteFile(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream f(path, std::ios::trunc | std::ios::binary);
    f << content;
}

}  // namespace test_util
