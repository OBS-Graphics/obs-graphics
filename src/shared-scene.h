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

#include "engine/scene.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct SceneSlot {
    Scene scene;
    std::mutex mutex;
    std::atomic<bool> loaded{false};
    std::string path;

    SceneSlot() = default;
    SceneSlot(const SceneSlot&) = delete;
    SceneSlot& operator=(const SceneSlot&) = delete;
};

// Owned exclusively by the Qt UI thread
extern std::vector<std::shared_ptr<SceneSlot>> g_scene_slots;

// Read atomically by the render thread each frame; written by the UI thread on tab changes.
// The atomic shared_ptr ensures the slot stays alive for the duration of any in-flight tick
// even when a tab is closed concurrently.
extern std::atomic<std::shared_ptr<SceneSlot>> g_active_slot;
