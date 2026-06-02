#pragma once

#include "graphic.h"

struct Scene {
    int width{1920}, height{1080};
    std::vector<Graphic> graphics;

    Scene() = default;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = default;
    Scene& operator=(Scene&&) = default;

    void Tick(float timeStep);
    void Render(cairo_t* ctx) const;

    Graphic& GetById(const std::string& id);

    static Scene Load(const std::string& jsonPath);
    static Scene LoadString(const std::string& json);
};
