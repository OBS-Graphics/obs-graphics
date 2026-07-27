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

#include "engine/scene.h"
#include "engine/data-pool.h"
#include "engine/data-source.h"
#include "engine/title.h"
#include <mutex>
#include <string>

// The one Scene this plugin runs: owns every Title and the DataPool behind it.
extern Scene g_scene;

// Guards every access to g_scene, and to any Title it owns:
// 1. Any read or write of g_scene, or of a Title* it owns, happens under this
//    mutex. engine/scene.h documents Scene as single-threaded; this mutex is
//    how the Qt UI thread and the OBS render thread take turns being that
//    thread.
// 2. DataPool calls that don't touch a Title (Pool().Add/Remove/Has/Ids/Get/
//    Data/DataBlocking/DataVersion) are individually thread-safe on their own
//    and must NOT be made while holding this mutex when they can block
//    (DataBlocking) — see engine/data-pool.h's own threading note.
// 3. Title::onTriggerIn/onTriggerOut subscribers fire with this mutex held
//    (render thread, inside Scene::Tick) — they must stay non-blocking.
//    Qt::QueuedConnection only, never BlockingQueuedConnection.
extern std::mutex g_scene_mutex;

// Host-side row metadata for one title. Deliberately Qt-free so the dialogs
// (title-settings-dialog.h) can include this header without pulling in the
// dock widget.
struct TitleRow {
    Title* title{nullptr};     // borrowed from g_scene; valid until RemoveTitle
    std::string id;            // persisted uuid, == title->id
    std::string path;          // .ogt path
    std::string dataSourceId;  // pool source uuid; empty = unbound
    double duration{-1.0};     // seconds; -1.0 = auto-hide disabled
};
