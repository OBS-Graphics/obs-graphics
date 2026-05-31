#include "shared-scene.h"

std::mutex g_scene_mutex;
Scene      g_active_scene;
bool       g_scene_loaded = false;
