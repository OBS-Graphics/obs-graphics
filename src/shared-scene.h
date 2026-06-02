#pragma once

#include <mutex>
#include "engine/scene.h"

extern std::mutex g_scene_mutex;
extern Scene      g_active_scene;
extern bool       g_scene_loaded;
