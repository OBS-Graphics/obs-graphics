#pragma once

#include "data-source.h"
#include "element.h"
#include <vector>

enum class GraphicState { Hidden, AnimatingIn, Visible, AnimatingOut };

struct Graphic {
    std::string id;
    std::vector<Element> elements;
    GraphicState state{GraphicState::Hidden};
    double timer{0.0f};
    int zOrder{0};

    IDataSource* dataSource{nullptr};
    size_t dataRecordIndex{0};

    void TriggerIn(size_t recordIndex = 0);
    void TriggerOut()
    {
        state = GraphicState::AnimatingOut;
        timer = 0.0f;
    }

    void UpdateData();

    Element& GetById(const std::string& id);

    void Tick(float timeStep);
    void Render(cairo_t* ctx) const;

private:
    double updateTimer{0.0}, prevUpdateTimer{0.0};
};
