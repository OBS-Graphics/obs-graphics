#pragma once

#include <functional>
#include <string>
#include <vector>

#include <QUndoCommand>
#include <QString>

#include "engine/graphic.h"
#include "engine/element.h"
#include "engine/scene.h"
#include "engine/json.hpp"

#include "SceneDocument.h"

// ── SetSceneNameCmd ───────────────────────────────────────────────────────────

class SetSceneNameCmd : public QUndoCommand {
public:
    SetSceneNameCmd(SceneDocument* doc,
                    const QString& after,
                    QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    QString        m_before;
    QString        m_after;
};

// ── SetGraphicFieldCmd ────────────────────────────────────────────────────────
// Generic command for a single scalar field on a Graphic.
// T must be copyable.

template<typename T>
class SetGraphicFieldCmd : public QUndoCommand {
public:
    SetGraphicFieldCmd(SceneDocument*           doc,
                       int                       gi,
                       T                         after,
                       std::function<T&(Graphic&)> accessor,
                       const QString&            fieldName,
                       QUndoCommand*             parent = nullptr)
        : QUndoCommand(parent)
        , m_doc(doc)
        , m_gi(gi)
        , m_after(std::move(after))
        , m_accessor(std::move(accessor))
    {
        setText(QString("Set graphic %1").arg(fieldName));

        // Capture before value
        const Scene& s = m_doc->scene();
        if (m_gi >= 0 && m_gi < static_cast<int>(s.graphics.size()))
            m_before = m_accessor(const_cast<Graphic&>(s.graphics[m_gi]));
    }

    void undo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            if (m_gi < static_cast<int>(s.graphics.size()))
                m_accessor(s.graphics[m_gi]) = m_before;
        });
    }

    void redo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            if (m_gi < static_cast<int>(s.graphics.size()))
                m_accessor(s.graphics[m_gi]) = m_after;
        });
    }

private:
    SceneDocument*           m_doc;
    int                      m_gi;
    T                        m_before;
    T                        m_after;
    std::function<T&(Graphic&)> m_accessor;
};

// ── SetElementFieldCmd ────────────────────────────────────────────────────────
// Generic command for a single scalar field on an Element.
// Supports mergeWith for numeric types (collapsing rapid edits).

template<typename T>
class SetElementFieldCmd : public QUndoCommand {
public:
    SetElementFieldCmd(SceneDocument*            doc,
                       int                        gi,
                       int                        ei,
                       T                          after,
                       std::function<T&(Element&)> accessor,
                       const QString&             fieldName,
                       int                        mergeTag = -1,
                       QUndoCommand*              parent   = nullptr)
        : QUndoCommand(parent)
        , m_doc(doc)
        , m_gi(gi)
        , m_ei(ei)
        , m_after(std::move(after))
        , m_accessor(std::move(accessor))
        , m_mergeTag(mergeTag)
    {
        setText(QString("Set element %1").arg(fieldName));

        const Scene& s = m_doc->scene();
        if (m_gi >= 0 && m_gi < static_cast<int>(s.graphics.size())) {
            const Graphic& g = s.graphics[m_gi];
            if (m_ei >= 0 && m_ei < static_cast<int>(g.elements.size()))
                m_before = m_accessor(const_cast<Element&>(g.elements[m_ei]));
        }
    }

    void undo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            if (m_gi < static_cast<int>(s.graphics.size())) {
                Graphic& g = s.graphics[m_gi];
                if (m_ei < static_cast<int>(g.elements.size()))
                    m_accessor(g.elements[m_ei]) = m_before;
            }
        });
    }

    void redo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            if (m_gi < static_cast<int>(s.graphics.size())) {
                Graphic& g = s.graphics[m_gi];
                if (m_ei < static_cast<int>(g.elements.size()))
                    m_accessor(g.elements[m_ei]) = m_after;
            }
        });
    }

    // Merge consecutive edits to the same field so the undo stack stays clean
    int id() const override { return m_mergeTag; }

    bool mergeWith(const QUndoCommand* other) override
    {
        if (other->id() != id() || id() == -1)
            return false;
        const auto* o = static_cast<const SetElementFieldCmd<T>*>(other);
        if (o->m_gi != m_gi || o->m_ei != m_ei)
            return false;
        m_after = o->m_after;
        return true;
    }

private:
    SceneDocument*            m_doc;
    int                       m_gi;
    int                       m_ei;
    T                         m_before;
    T                         m_after;
    std::function<T&(Element&)> m_accessor;
    int                       m_mergeTag;
};

// ── SetElementPaintCmd ────────────────────────────────────────────────────────

class SetElementPaintCmd : public QUndoCommand {
public:
    enum class Target { Fill, Stroke };

    SetElementPaintCmd(SceneDocument* doc,
                       int             gi,
                       int             ei,
                       Target          target,
                       const Paint&    after,
                       QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_gi;
    int            m_ei;
    Target         m_target;
    Paint          m_before;
    Paint          m_after;

    Paint& paintRef(Element& el) const;
};

// ── SetElementAnimCmd ─────────────────────────────────────────────────────────

class SetElementAnimCmd : public QUndoCommand {
public:
    enum class Target { AnimIn, AnimOut };

    SetElementAnimCmd(SceneDocument*    doc,
                      int                gi,
                      int                ei,
                      Target             target,
                      const AnimationDef& after,
                      QUndoCommand*     parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_gi;
    int            m_ei;
    Target         m_target;
    AnimationDef   m_before;
    AnimationDef   m_after;

    AnimationDef& animRef(Element& el) const;
};

// ── Structural: Add / Remove Graphic ─────────────────────────────────────────

class AddGraphicCmd : public QUndoCommand {
public:
    // Takes ownership of the graphic definition (as JSON for safe storage)
    AddGraphicCmd(SceneDocument*     doc,
                  nlohmann::json     graphicJson,
                  QUndoCommand*     parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    nlohmann::json m_json;
    std::string    m_id; // extracted for O(1) lookup on undo
};

class RemoveGraphicCmd : public QUndoCommand {
public:
    RemoveGraphicCmd(SceneDocument* doc,
                     int             gi,
                     QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_gi;
    nlohmann::json m_snapshot; // full JSON backup captured in ctor
    int            m_savedZOrder;
};

// ── Structural: Add / Remove Element ─────────────────────────────────────────

class AddElementCmd : public QUndoCommand {
public:
    AddElementCmd(SceneDocument*  doc,
                  int              gi,
                  nlohmann::json   elementJson,
                  QUndoCommand*   parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_gi;
    nlohmann::json m_json;
    std::string    m_id;
};

class RemoveElementCmd : public QUndoCommand {
public:
    RemoveElementCmd(SceneDocument* doc,
                     int             gi,
                     int             ei,
                     QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_gi;
    int            m_ei;
    nlohmann::json m_snapshot;
    std::string    m_graphicId; // stored for re-insertion reference mapping
};

// ── Structural: Move Graphic / Element ───────────────────────────────────────

// MoveGraphicCmd swaps the vector positions of two graphics (by swapping them
// in the graphics vector, which is the simplest reorder primitive; callers
// can build multi-step moves by pushing multiple commands).
class MoveGraphicCmd : public QUndoCommand {
public:
    MoveGraphicCmd(SceneDocument* doc,
                   int             fromIndex,
                   int             toIndex,
                   QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_from;
    int            m_to;

    void doMove(int from, int to);
};

class MoveElementCmd : public QUndoCommand {
public:
    MoveElementCmd(SceneDocument* doc,
                   int             gi,
                   int             fromIndex,
                   int             toIndex,
                   QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_gi;
    int            m_from;
    int            m_to;

    void doMove(int from, int to);
};
