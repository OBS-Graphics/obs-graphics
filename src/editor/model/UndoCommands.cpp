#include "UndoCommands.h"

#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers shared across commands
// ─────────────────────────────────────────────────────────────────────────────

// Re-parse a single graphic JSON blob into the scene
static void insertGraphicFromJson(Scene& scene, const json& gj)
{
    // Build a temporary scene JSON and load just the one graphic.
    // We wrap it in a scene envelope so Scene::LoadString handles pointer fixup.
    json wrapper;
    wrapper["graphics"] = json::array({gj});
    Scene tmp = Scene::LoadString(wrapper.dump());
    if (!tmp.graphics.empty())
        scene.graphics.push_back(std::move(tmp.graphics[0]));
}

// Re-parse a single element JSON blob into a graphic
static void insertElementFromJson(Graphic& g, const json& ej)
{
    // Build a temporary graphic envelope and use Scene::LoadString
    json gWrapper;
    gWrapper["id"]       = g.id;
    gWrapper["z_order"]  = g.zOrder;
    gWrapper["elements"] = json::array({ej});

    json wrapper;
    wrapper["graphics"] = json::array({gWrapper});
    Scene tmp = Scene::LoadString(wrapper.dump());
    if (!tmp.graphics.empty() && !tmp.graphics[0].elements.empty()) {
        // Clear the pointer fixup — the real graphic already has existing
        // elements so raw pointers from the temp scene are invalid.
        Element newEl = std::move(tmp.graphics[0].elements[0]);
        newEl.mask   = nullptr;
        newEl.parent = nullptr;
        g.elements.push_back(std::move(newEl));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SetSceneNameCmd
// ─────────────────────────────────────────────────────────────────────────────

SetSceneNameCmd::SetSceneNameCmd(SceneDocument* doc,
                                 const QString& after,
                                 QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_before(doc->sceneName())
    , m_after(after)
{
    setText("Set scene name");
}

void SetSceneNameCmd::undo()
{
    m_doc->setSceneName(m_before);
}

void SetSceneNameCmd::redo()
{
    m_doc->setSceneName(m_after);
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementPaintCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementPaintCmd::SetElementPaintCmd(SceneDocument* doc,
                                       int             gi,
                                       int             ei,
                                       Target          target,
                                       const Paint&    after,
                                       QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_gi(gi)
    , m_ei(ei)
    , m_target(target)
    , m_after(after)
{
    setText(target == Target::Fill ? "Set fill" : "Set stroke");

    const Scene& s = m_doc->scene();
    if (m_gi >= 0 && m_gi < static_cast<int>(s.graphics.size())) {
        const Graphic& g = s.graphics[m_gi];
        if (m_ei >= 0 && m_ei < static_cast<int>(g.elements.size()))
            m_before = paintRef(const_cast<Element&>(g.elements[m_ei]));
    }
}

Paint& SetElementPaintCmd::paintRef(Element& el) const
{
    return (m_target == Target::Fill) ? el.fill : el.stroke;
}

void SetElementPaintCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size())) {
            Graphic& g = s.graphics[m_gi];
            if (m_ei < static_cast<int>(g.elements.size()))
                paintRef(g.elements[m_ei]) = m_before;
        }
    });
}

void SetElementPaintCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size())) {
            Graphic& g = s.graphics[m_gi];
            if (m_ei < static_cast<int>(g.elements.size()))
                paintRef(g.elements[m_ei]) = m_after;
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementAnimCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementAnimCmd::SetElementAnimCmd(SceneDocument*     doc,
                                     int                 gi,
                                     int                 ei,
                                     Target              target,
                                     const AnimationDef& after,
                                     QUndoCommand*      parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_gi(gi)
    , m_ei(ei)
    , m_target(target)
    , m_after(after)
{
    setText(target == Target::AnimIn ? "Set anim in" : "Set anim out");

    const Scene& s = m_doc->scene();
    if (m_gi >= 0 && m_gi < static_cast<int>(s.graphics.size())) {
        const Graphic& g = s.graphics[m_gi];
        if (m_ei >= 0 && m_ei < static_cast<int>(g.elements.size()))
            m_before = animRef(const_cast<Element&>(g.elements[m_ei]));
    }
}

AnimationDef& SetElementAnimCmd::animRef(Element& el) const
{
    return (m_target == Target::AnimIn) ? el.inAnimation : el.outAnimation;
}

void SetElementAnimCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size())) {
            Graphic& g = s.graphics[m_gi];
            if (m_ei < static_cast<int>(g.elements.size()))
                animRef(g.elements[m_ei]) = m_before;
        }
    });
}

void SetElementAnimCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size())) {
            Graphic& g = s.graphics[m_gi];
            if (m_ei < static_cast<int>(g.elements.size()))
                animRef(g.elements[m_ei]) = m_after;
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// AddGraphicCmd
// ─────────────────────────────────────────────────────────────────────────────

AddGraphicCmd::AddGraphicCmd(SceneDocument* doc,
                             json            graphicJson,
                             QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_json(std::move(graphicJson))
    , m_id(m_json.value("id", ""))
{
    setText("Add graphic");
}

void AddGraphicCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        auto it = std::find_if(s.graphics.begin(), s.graphics.end(),
                               [&](const Graphic& g) { return g.id == m_id; });
        if (it != s.graphics.end())
            s.graphics.erase(it);
    });
}

void AddGraphicCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        insertGraphicFromJson(s, m_json);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// RemoveGraphicCmd
// ─────────────────────────────────────────────────────────────────────────────

RemoveGraphicCmd::RemoveGraphicCmd(SceneDocument* doc,
                                   int             gi,
                                   QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_gi(gi)
    , m_savedZOrder(0)
{
    setText("Remove graphic");

    // Snapshot the graphic as JSON before it is removed
    const Scene& s = m_doc->scene();
    if (m_gi >= 0 && m_gi < static_cast<int>(s.graphics.size())) {
        const Graphic& g = s.graphics[m_gi];
        m_savedZOrder = g.zOrder;

        json fullScene = SceneDocument::sceneToJson(s);
        const auto& garr = fullScene["graphics"];
        if (m_gi < static_cast<int>(garr.size()))
            m_snapshot = garr[m_gi];
    }
}

void RemoveGraphicCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        insertGraphicFromJson(s, m_snapshot);
        // Restore original position: move the last element to m_gi
        int last = static_cast<int>(s.graphics.size()) - 1;
        if (last > m_gi) {
            // Rotate right: the restored graphic is at the end; move to m_gi
            std::rotate(s.graphics.begin() + m_gi,
                        s.graphics.end()   - 1,
                        s.graphics.end());
        }
    });
}

void RemoveGraphicCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi >= 0 && m_gi < static_cast<int>(s.graphics.size()))
            s.graphics.erase(s.graphics.begin() + m_gi);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// AddElementCmd
// ─────────────────────────────────────────────────────────────────────────────

AddElementCmd::AddElementCmd(SceneDocument* doc,
                             int             gi,
                             json            elementJson,
                             QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_gi(gi)
    , m_json(std::move(elementJson))
    , m_id(m_json.value("id", ""))
{
    setText("Add element");
}

void AddElementCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size())) {
            Graphic& g = s.graphics[m_gi];
            auto it = std::find_if(g.elements.begin(), g.elements.end(),
                                   [&](const Element& el) { return el.id == m_id; });
            if (it != g.elements.end())
                g.elements.erase(it);
        }
    });
}

void AddElementCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size()))
            insertElementFromJson(s.graphics[m_gi], m_json);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// RemoveElementCmd
// ─────────────────────────────────────────────────────────────────────────────

RemoveElementCmd::RemoveElementCmd(SceneDocument* doc,
                                   int             gi,
                                   int             ei,
                                   QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_gi(gi)
    , m_ei(ei)
{
    setText("Remove element");

    const Scene& s = m_doc->scene();
    if (m_gi >= 0 && m_gi < static_cast<int>(s.graphics.size())) {
        const Graphic& g = s.graphics[m_gi];
        m_graphicId = g.id;
        if (m_ei >= 0 && m_ei < static_cast<int>(g.elements.size())) {
            // Serialize the element via full scene JSON
            json fullScene = SceneDocument::sceneToJson(s);
            const auto& garr = fullScene["graphics"];
            if (m_gi < static_cast<int>(garr.size())) {
                const auto& earr = garr[m_gi]["elements"];
                if (m_ei < static_cast<int>(earr.size()))
                    m_snapshot = earr[m_ei];
            }
        }
    }
}

void RemoveElementCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size())) {
            Graphic& g = s.graphics[m_gi];
            insertElementFromJson(g, m_snapshot);
            // Restore original position
            int last = static_cast<int>(g.elements.size()) - 1;
            if (last > m_ei) {
                std::rotate(g.elements.begin() + m_ei,
                            g.elements.end()   - 1,
                            g.elements.end());
            }
        }
    });
}

void RemoveElementCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < static_cast<int>(s.graphics.size())) {
            Graphic& g = s.graphics[m_gi];
            if (m_ei >= 0 && m_ei < static_cast<int>(g.elements.size()))
                g.elements.erase(g.elements.begin() + m_ei);
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveGraphicCmd
// ─────────────────────────────────────────────────────────────────────────────

MoveGraphicCmd::MoveGraphicCmd(SceneDocument* doc,
                               int             fromIndex,
                               int             toIndex,
                               QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_from(fromIndex)
    , m_to(toIndex)
{
    setText("Move graphic");
}

void MoveGraphicCmd::doMove(int from, int to)
{
    m_doc->applyMutation([&](Scene& s) {
        int n = static_cast<int>(s.graphics.size());
        if (from < 0 || from >= n || to < 0 || to >= n || from == to)
            return;
        if (from < to)
            std::rotate(s.graphics.begin() + from,
                        s.graphics.begin() + from + 1,
                        s.graphics.begin() + to + 1);
        else
            std::rotate(s.graphics.begin() + to,
                        s.graphics.begin() + from,
                        s.graphics.begin() + from + 1);
    });
}

void MoveGraphicCmd::undo() { doMove(m_to, m_from); }
void MoveGraphicCmd::redo() { doMove(m_from, m_to); }

// ─────────────────────────────────────────────────────────────────────────────
// MoveElementCmd
// ─────────────────────────────────────────────────────────────────────────────

MoveElementCmd::MoveElementCmd(SceneDocument* doc,
                               int             gi,
                               int             fromIndex,
                               int             toIndex,
                               QUndoCommand*  parent)
    : QUndoCommand(parent)
    , m_doc(doc)
    , m_gi(gi)
    , m_from(fromIndex)
    , m_to(toIndex)
{
    setText("Move element");
}

void MoveElementCmd::doMove(int from, int to)
{
    m_doc->applyMutation([&](Scene& s) {
        if (m_gi < 0 || m_gi >= static_cast<int>(s.graphics.size()))
            return;
        Graphic& g = s.graphics[m_gi];
        int n = static_cast<int>(g.elements.size());
        if (from < 0 || from >= n || to < 0 || to >= n || from == to)
            return;
        if (from < to)
            std::rotate(g.elements.begin() + from,
                        g.elements.begin() + from + 1,
                        g.elements.begin() + to + 1);
        else
            std::rotate(g.elements.begin() + to,
                        g.elements.begin() + from,
                        g.elements.begin() + from + 1);
    });
}

void MoveElementCmd::undo() { doMove(m_to, m_from); }
void MoveElementCmd::redo() { doMove(m_from, m_to); }
