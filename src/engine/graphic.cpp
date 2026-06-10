#include "graphic.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

void Graphic::TriggerIn(size_t recordIndex)
{
    dataRecordIndex = recordIndex;
    UpdateData();
    state = GraphicState::AnimatingIn;
    timer = 0.0f;
}

// TODO: In the future, animate data transitions.
void Graphic::UpdateData() {
    if (!dataSource) return;

    auto data = dataSource->GetData();
    if (data.empty()) {
        return;
    }

    const auto& record = data[dataRecordIndex % data.size()];
    for (auto&& rec : record) {
        try {
            auto& el = GetById(rec.first);
            el.text = rec.second;
        } catch (const std::runtime_error&) {
            // No element with this id; ignore
        }
    }
}

Element& Graphic::GetById(const std::string& id)
{
    for (auto& el : elements) {
        if (el.id == id)
            return el;
    }
    throw std::runtime_error("Element with id '" + id + "' not found");
}

void Graphic::Tick(float timeStep)
{
    // Update data timer
    updateTimer += timeStep;

    const double updateInterval = 0.2;
    int prevSlot = static_cast<int>(prevUpdateTimer / updateInterval);
    int currSlot = static_cast<int>(updateTimer / updateInterval);

    if (prevSlot != currSlot) {
        UpdateData();
    }

    prevUpdateTimer = updateTimer;

    if (state == GraphicState::Hidden || state == GraphicState::Visible) {
        return;
    }

    timer += timeStep;

    bool allDone = true;
    for (const auto& el : elements) {
        const auto& def = state == GraphicState::AnimatingIn ? el.inAnimation : el.outAnimation;
        if (timer < def.delay + def.duration) {
            allDone = false;
            break;
        }
    }

    if (allDone) {
        state = state == GraphicState::AnimatingIn ? GraphicState::Visible : GraphicState::Hidden;
    }
}

void Graphic::Render(cairo_t* ctx) const
{
    if (state == GraphicState::Hidden)
        return;

    std::vector<size_t> eOrder(elements.size());
    std::iota(eOrder.begin(), eOrder.end(), 0);
    std::stable_sort(eOrder.begin(), eOrder.end(),
                     [&](size_t a, size_t b) { return elements[a].zOrder < elements[b].zOrder; });

    bool isOut = state == GraphicState::AnimatingOut;

    // Pre-compute all transforms so mask elements' animated offsets can be applied
    std::vector<AnimatedTransform> xforms(elements.size());
    for (size_t i = 0; i < elements.size(); ++i) {
        const auto& def = isOut ? elements[i].outAnimation : elements[i].inAnimation;
        xforms[i] = animation::EvaluateAnimation(def, timer, isOut, elements[i]);
    }

    auto findIdx = [&](const Element* p) -> size_t {
        for (size_t i = 0; i < elements.size(); ++i)
            if (&elements[i] == p) return i;
        return SIZE_MAX;
    };

    for (size_t ei : eOrder) {
        const auto& el = elements[ei];
        if (el.parent != nullptr) continue;

        const AnimatedTransform& xf = xforms[ei];
        const AnimatedTransform* maskXf = nullptr;
        if (el.mask != nullptr) {
            size_t mi = findIdx(el.mask);
            if (mi != SIZE_MAX) maskXf = &xforms[mi];
        }

        auto pos = el.GetGlobalPosition();
        cairo_save(ctx);
        cairo_translate(ctx, pos.x, pos.y);
        el.Render(ctx, xf, maskXf, timer, isOut, 0.0, 0.0);
        cairo_restore(ctx);
    }
}
