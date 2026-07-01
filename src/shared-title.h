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

#include "engine/data-source.h"
#include "engine/title.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct TitleSlot {
    Title title;
    std::mutex mutex;
    std::atomic<bool> loaded{false};
    std::string path;
    std::unique_ptr<IDataSource> ownedDataSource; // dock owns, title holds raw ptr
    std::string dataSourcePath;

    TitleSlot() = default;
    TitleSlot(const TitleSlot&) = delete;
    TitleSlot& operator=(const TitleSlot&) = delete;
};

using TitleSlotList = std::vector<std::shared_ptr<TitleSlot>>;

// Atomic snapshot: UI thread atomically replaces the list; render thread atomically reads it.
// The shared_ptr ensures in-flight slots stay alive even after removal from the list.
extern std::atomic<std::shared_ptr<TitleSlotList>> g_title_slots;
