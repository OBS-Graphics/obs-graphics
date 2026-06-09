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
    gWrapper["id"] = g.id;
    gWrapper["z_order"] = g.zOrder;
    gWrapper["elements"] = json::array({ej});

    json wrapper;
    wrapper["graphics"] = json::array({gWrapper});
    Scene tmp = Scene::LoadString(wrapper.dump());
    if (!tmp.graphics.empty() && !tmp.graphics[0].elements.empty()) {
        // Clear the pointer fixup — the real graphic already has existing
        // elements so raw pointers from the temp scene are invalid.
        Element newEl = std::move(tmp.graphics[0].elements[0]);
        newEl.mask = nullptr;
        newEl.parent = nullptr;
        g.elements.push_back(std::move(newEl));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SetSceneDimensionsCmd
// ─────────────────────────────────────────────────────────────────────────────

SetSceneDimensionsCmd::SetSceneDimensionsCmd(SceneDocument* doc, int width, int height,
                                             QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_beforeW(doc->scene().width),
      m_beforeH(doc->scene().height), m_afterW(width), m_afterH(height)
{
    setText("Set scene dimensions");
}

void SetSceneDimensionsCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        s.width = m_beforeW;
        s.height = m_beforeH;
    });
}

void SetSceneDimensionsCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        s.width = m_afterW;
        s.height = m_afterH;
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetSceneNameCmd
// ─────────────────────────────────────────────────────────────────────────────

SetSceneNameCmd::SetSceneNameCmd(SceneDocument* doc, const QString& after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_before(doc->sceneName()), m_after(after)
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

SetElementPaintCmd::SetElementPaintCmd(SceneDocument* doc, std::string gi, std::string ei,
                                       Target target, const Paint& after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_ei(std::move(ei)), m_target(target),
      m_after(after)
{
    setText(target == Target::Fill ? "Set fill" : "Set stroke");

    try {
        m_before = paintRef(m_doc->scene().GetById(m_gi).GetById(m_ei));
    } catch (const std::runtime_error&) {
    }
}

Paint& SetElementPaintCmd::paintRef(Element& el) const
{
    return (m_target == Target::Fill) ? el.fill : el.stroke;
}

void SetElementPaintCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            paintRef(s.GetById(m_gi).GetById(m_ei)) = m_before;
        } catch (const std::runtime_error&) {
        }
    });
}

void SetElementPaintCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            paintRef(s.GetById(m_gi).GetById(m_ei)) = m_after;
        } catch (const std::runtime_error&) {
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetElementAnimCmd
// ─────────────────────────────────────────────────────────────────────────────

SetElementAnimCmd::SetElementAnimCmd(SceneDocument* doc, std::string gi, std::string ei,
                                     Target target, const AnimationDef& after, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_ei(std::move(ei)), m_target(target),
      m_after(after)
{
    setText(target == Target::AnimIn ? "Set anim in" : "Set anim out");

    try {
        m_before = animRef(m_doc->scene().GetById(m_gi).GetById(m_ei));
    } catch (const std::runtime_error&) {
    }
}

AnimationDef& SetElementAnimCmd::animRef(Element& el) const
{
    return (m_target == Target::AnimIn) ? el.inAnimation : el.outAnimation;
}

void SetElementAnimCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            animRef(s.GetById(m_gi).GetById(m_ei)) = m_before;
        } catch (const std::runtime_error&) {
        }
    });
}

void SetElementAnimCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            animRef(s.GetById(m_gi).GetById(m_ei)) = m_after;
        } catch (const std::runtime_error&) {
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetChildrenPaddingCmd
// ─────────────────────────────────────────────────────────────────────────────

SetChildrenPaddingCmd::SetChildrenPaddingCmd(SceneDocument* doc, std::string gi, std::string ei,
                                             float top, float right, float bottom, float left,
                                             QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_ei(std::move(ei))
{
    setText("Set children padding");
    m_after[0] = top;
    m_after[1] = right;
    m_after[2] = bottom;
    m_after[3] = left;

    try {
        const Element& el = m_doc->scene().GetById(m_gi).GetById(m_ei);
        for (int i = 0; i < 4; ++i)
            m_before[i] = el.childrenPadding[i];
    } catch (const std::runtime_error&) {
        for (int i = 0; i < 4; ++i)
            m_before[i] = 0.f;
    }
}

void SetChildrenPaddingCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Element& el = s.GetById(m_gi).GetById(m_ei);
            for (int i = 0; i < 4; ++i)
                el.childrenPadding[i] = m_before[i];
        } catch (const std::runtime_error&) {
        }
    });
}

void SetChildrenPaddingCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Element& el = s.GetById(m_gi).GetById(m_ei);
            for (int i = 0; i < 4; ++i)
                el.childrenPadding[i] = m_after[i];
        } catch (const std::runtime_error&) {
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// SetCornerRadiusCmd
// ─────────────────────────────────────────────────────────────────────────────

SetCornerRadiusCmd::SetCornerRadiusCmd(SceneDocument* doc, std::string gi, std::string ei, float tl,
                                       float tr, float br, float bl, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_ei(std::move(ei))
{
    setText("Set corner radius");
    m_after[0] = tl;
    m_after[1] = tr;
    m_after[2] = br;
    m_after[3] = bl;

    try {
        const Element& el = m_doc->scene().GetById(m_gi).GetById(m_ei);
        for (int i = 0; i < 4; ++i)
            m_before[i] = static_cast<float>(el.cornerRadius[i]);
    } catch (const std::runtime_error&) {
        for (int i = 0; i < 4; ++i)
            m_before[i] = 0.f;
    }
}

void SetCornerRadiusCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Element& el = s.GetById(m_gi).GetById(m_ei);
            for (int i = 0; i < 4; ++i)
                el.cornerRadius[i] = m_before[i];
        } catch (const std::runtime_error&) {
        }
    });
}

void SetCornerRadiusCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Element& el = s.GetById(m_gi).GetById(m_ei);
            for (int i = 0; i < 4; ++i)
                el.cornerRadius[i] = m_after[i];
        } catch (const std::runtime_error&) {
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// RenameGraphicCmd
// ─────────────────────────────────────────────────────────────────────────────

RenameGraphicCmd::RenameGraphicCmd(SceneDocument* doc, std::string before, std::string after,
                                   QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_before(std::move(before)), m_after(std::move(after))
{
    setText("Rename graphic");
}

void RenameGraphicCmd::applyRename(const std::string& from, const std::string& to)
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            s.GetById(from).id = to;
        } catch (const std::runtime_error&) {
        }
    });
    m_doc->renameGraphicRef(from, to);
}

void RenameGraphicCmd::undo()
{
    applyRename(m_after, m_before);
}
void RenameGraphicCmd::redo()
{
    applyRename(m_before, m_after);
}

// ─────────────────────────────────────────────────────────────────────────────
// AddGraphicCmd
// ─────────────────────────────────────────────────────────────────────────────

AddGraphicCmd::AddGraphicCmd(SceneDocument* doc, json graphicJson, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_json(std::move(graphicJson)), m_id(m_json.value("id", ""))
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
    m_doc->applyMutation([&](Scene& s) { insertGraphicFromJson(s, m_json); });
}

// ─────────────────────────────────────────────────────────────────────────────
// RemoveGraphicCmd
// ─────────────────────────────────────────────────────────────────────────────

RemoveGraphicCmd::RemoveGraphicCmd(SceneDocument* doc, std::string gi, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_savedZOrder(0), m_savedPosition(-1)
{
    setText("Remove graphic");

    Scene& s = m_doc->scene();
    try {
        const Graphic& g = s.GetById(m_gi);
        m_savedZOrder = g.zOrder;

        auto it = std::find_if(s.graphics.begin(), s.graphics.end(),
                               [&](const Graphic& gr) { return gr.id == m_gi; });
        if (it != s.graphics.end()) {
            m_savedPosition = static_cast<int>(it - s.graphics.begin());
            json fullScene = SceneDocument::sceneToJson(s);
            const auto& garr = fullScene["graphics"];
            if (m_savedPosition < static_cast<int>(garr.size()))
                m_snapshot = garr[m_savedPosition];
        }
    } catch (const std::runtime_error&) {
    }
}

void RemoveGraphicCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        insertGraphicFromJson(s, m_snapshot);
        if (m_savedPosition >= 0) {
            int last = static_cast<int>(s.graphics.size()) - 1;
            if (last > m_savedPosition) {
                std::rotate(s.graphics.begin() + m_savedPosition, s.graphics.end() - 1,
                            s.graphics.end());
            }
        }
    });
    // Restore mask/parent logical refs for all elements in the snapshot
    if (m_snapshot.contains("elements")) {
        for (const auto& ej : m_snapshot["elements"]) {
            std::string eid = ej.value("id", "");
            std::string maskId = ej.value("mask", "");
            std::string parentId = ej.value("parent", "");
            if (!eid.empty() && (!maskId.empty() || !parentId.empty()))
                m_doc->setElementRef(m_gi, eid, maskId, parentId);
        }
    }
}

void RemoveGraphicCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        auto it = std::find_if(s.graphics.begin(), s.graphics.end(),
                               [&](const Graphic& g) { return g.id == m_gi; });
        if (it != s.graphics.end())
            s.graphics.erase(it);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// AddElementCmd
// ─────────────────────────────────────────────────────────────────────────────

AddElementCmd::AddElementCmd(SceneDocument* doc, std::string gi, json elementJson,
                             QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_json(std::move(elementJson)),
      m_id(m_json.value("id", ""))
{
    setText("Add element");
}

void AddElementCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Graphic& g = s.GetById(m_gi);
            auto it = std::find_if(g.elements.begin(), g.elements.end(),
                                   [&](const Element& el) { return el.id == m_id; });
            if (it != g.elements.end())
                g.elements.erase(it);
        } catch (const std::runtime_error&) {
        }
    });
}

void AddElementCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            insertElementFromJson(s.GetById(m_gi), m_json);
        } catch (const std::runtime_error&) {
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// RemoveElementCmd
// ─────────────────────────────────────────────────────────────────────────────

RemoveElementCmd::RemoveElementCmd(SceneDocument* doc, std::string gi, std::string ei,
                                   QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_ei(std::move(ei)),
      m_savedPosition(-1)
{
    setText("Remove element");

    Scene& s = m_doc->scene();
    try {
        Graphic& g = s.GetById(m_gi);
        auto eit = std::find_if(g.elements.begin(), g.elements.end(),
                                [&](const Element& el) { return el.id == m_ei; });
        if (eit != g.elements.end()) {
            m_savedPosition = static_cast<int>(eit - g.elements.begin());

            auto git = std::find_if(s.graphics.begin(), s.graphics.end(),
                                    [&](const Graphic& gr) { return gr.id == m_gi; });
            if (git != s.graphics.end()) {
                int gi_idx = static_cast<int>(git - s.graphics.begin());
                json fullScene = SceneDocument::sceneToJson(s);
                const auto& garr = fullScene["graphics"];
                if (gi_idx < static_cast<int>(garr.size())) {
                    const auto& earr = garr[gi_idx]["elements"];
                    if (m_savedPosition < static_cast<int>(earr.size()))
                        m_snapshot = earr[m_savedPosition];
                }
            }
        }
    } catch (const std::runtime_error&) {
    }
}

void RemoveElementCmd::undo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Graphic& g = s.GetById(m_gi);
            insertElementFromJson(g, m_snapshot);
            if (m_savedPosition >= 0) {
                int last = static_cast<int>(g.elements.size()) - 1;
                if (last > m_savedPosition) {
                    std::rotate(g.elements.begin() + m_savedPosition, g.elements.end() - 1,
                                g.elements.end());
                }
            }
        } catch (const std::runtime_error&) {
        }
    });
    // Restore mask/parent logical refs that insertElementFromJson cleared
    const std::string maskId = m_snapshot.value("mask", "");
    const std::string parentId = m_snapshot.value("parent", "");
    if (!maskId.empty() || !parentId.empty())
        m_doc->setElementRef(m_gi, m_ei, maskId, parentId);
}

void RemoveElementCmd::redo()
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Graphic& g = s.GetById(m_gi);
            auto it = std::find_if(g.elements.begin(), g.elements.end(),
                                   [&](const Element& el) { return el.id == m_ei; });
            if (it != g.elements.end())
                g.elements.erase(it);
        } catch (const std::runtime_error&) {
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveGraphicCmd
// ─────────────────────────────────────────────────────────────────────────────

MoveGraphicCmd::MoveGraphicCmd(SceneDocument* doc, int fromIndex, int toIndex, QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_from(fromIndex), m_to(toIndex)
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
            std::rotate(s.graphics.begin() + from, s.graphics.begin() + from + 1,
                        s.graphics.begin() + to + 1);
        else
            std::rotate(s.graphics.begin() + to, s.graphics.begin() + from,
                        s.graphics.begin() + from + 1);
    });
}

void MoveGraphicCmd::undo()
{
    doMove(m_to, m_from);
}
void MoveGraphicCmd::redo()
{
    doMove(m_from, m_to);
}

// ─────────────────────────────────────────────────────────────────────────────
// MoveElementCmd
// ─────────────────────────────────────────────────────────────────────────────

MoveElementCmd::MoveElementCmd(SceneDocument* doc, std::string gi, int fromIndex, int toIndex,
                               QUndoCommand* parent)
    : QUndoCommand(parent), m_doc(doc), m_gi(std::move(gi)), m_from(fromIndex), m_to(toIndex)
{
    setText("Move element");
}

void MoveElementCmd::doMove(int from, int to)
{
    m_doc->applyMutation([&](Scene& s) {
        try {
            Graphic& g = s.GetById(m_gi);
            int n = static_cast<int>(g.elements.size());
            if (from < 0 || from >= n || to < 0 || to >= n || from == to)
                return;
            if (from < to)
                std::rotate(g.elements.begin() + from, g.elements.begin() + from + 1,
                            g.elements.begin() + to + 1);
            else
                std::rotate(g.elements.begin() + to, g.elements.begin() + from,
                            g.elements.begin() + from + 1);
        } catch (const std::runtime_error&) {
        }
    });
}

void MoveElementCmd::undo()
{
    doMove(m_to, m_from);
}
void MoveElementCmd::redo()
{
    doMove(m_from, m_to);
}
