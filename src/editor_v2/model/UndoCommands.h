#pragma once

#include <functional>
#include <string>

#include <QUndoCommand>
#include <QString>

#include "engine/graphic.h"
#include "engine/element.h"
#include "engine/scene.h"
#include "engine/json.hpp"

#include "SceneDocument.h"

// ── SetSceneDimensionsCmd ─────────────────────────────────────────────────────

class SetSceneDimensionsCmd : public QUndoCommand {
public:
    SetSceneDimensionsCmd(SceneDocument* doc,
                          int             width,
                          int             height,
                          QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    int            m_beforeW, m_beforeH;
    int            m_afterW,  m_afterH;
};

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
    SetGraphicFieldCmd(SceneDocument*             doc,
                       std::string                 gi,
                       T                           after,
                       std::function<T&(Graphic&)> accessor,
                       const QString&              fieldName,
                       QUndoCommand*               parent = nullptr)
        : QUndoCommand(parent)
        , m_doc(doc)
        , m_gi(std::move(gi))
        , m_after(std::move(after))
        , m_accessor(std::move(accessor))
    {
        setText(QString("Set graphic %1").arg(fieldName));

        try {
            m_before = m_accessor(m_doc->scene().GetById(m_gi));
        } catch (const std::runtime_error&) {}
    }

    void undo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            try { m_accessor(s.GetById(m_gi)) = m_before; }
            catch (const std::runtime_error&) {}
        });
    }

    void redo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            try { m_accessor(s.GetById(m_gi)) = m_after; }
            catch (const std::runtime_error&) {}
        });
    }

private:
    SceneDocument*             m_doc;
    std::string                m_gi;
    T                          m_before;
    T                          m_after;
    std::function<T&(Graphic&)> m_accessor;
};

// ── ElemMergeTag — unique ids for collapsing rapid spinbox edits ──────────────

namespace ElemMergeTag {
    enum : int {
        ZOrder   = 1000,
        X        = 1001,
        Y        = 1002,
        W        = 1003,
        H        = 1004,
        Rotation = 1005,
        Opacity  = 1006,
        StrokeW  = 1007,
    };
}

// ── SetElementFieldCmd ────────────────────────────────────────────────────────
// Generic command for a single scalar field on an Element.
// Uses (doc, gi, ei) string ids so undo/redo survives structural moves.
// Supports mergeWith for numeric types (collapsing rapid spinbox edits).

template<typename T>
class SetElementFieldCmd : public QUndoCommand {
public:
    SetElementFieldCmd(SceneDocument*              doc,
                       std::string                  gi,
                       std::string                  ei,
                       T                            after,
                       std::function<T&(Element&)>  accessor,
                       const QString&               fieldName,
                       int                          mergeTag = -1,
                       QUndoCommand*                parent = nullptr)
        : QUndoCommand(parent)
        , m_doc(doc)
        , m_gi(std::move(gi))
        , m_ei(std::move(ei))
        , m_after(std::move(after))
        , m_accessor(std::move(accessor))
        , m_mergeTag(mergeTag)
    {
        setText(QString("Set element %1").arg(fieldName));
        try {
            m_before = m_accessor(m_doc->scene().GetById(m_gi).GetById(m_ei));
        } catch (const std::runtime_error&) {}
    }

    void undo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            try { m_accessor(s.GetById(m_gi).GetById(m_ei)) = m_before; }
            catch (const std::runtime_error&) {}
        });
    }

    void redo() override
    {
        m_doc->applyMutation([&](Scene& s) {
            try { m_accessor(s.GetById(m_gi).GetById(m_ei)) = m_after; }
            catch (const std::runtime_error&) {}
        });
    }

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
    SceneDocument*             m_doc;
    std::string                m_gi;
    std::string                m_ei;
    T                          m_before;
    T                          m_after;
    std::function<T&(Element&)> m_accessor;
    int                        m_mergeTag;
};

// ── SetElementPaintCmd ────────────────────────────────────────────────────────

class SetElementPaintCmd : public QUndoCommand {
public:
    enum class Target { Fill, Stroke };

    SetElementPaintCmd(SceneDocument*  doc,
                       std::string      gi,
                       std::string      ei,
                       Target           target,
                       const Paint&     after,
                       QUndoCommand*   parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    std::string    m_ei;
    Target         m_target;
    Paint          m_before;
    Paint          m_after;

    Paint& paintRef(Element& el) const;
};

// ── SetElementAnimCmd ─────────────────────────────────────────────────────────

class SetElementAnimCmd : public QUndoCommand {
public:
    enum class Target { AnimIn, AnimOut };

    SetElementAnimCmd(SceneDocument*     doc,
                      std::string         gi,
                      std::string         ei,
                      Target              target,
                      const AnimationDef& after,
                      QUndoCommand*      parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    std::string    m_ei;
    Target         m_target;
    AnimationDef   m_before;
    AnimationDef   m_after;

    AnimationDef& animRef(Element& el) const;
};

// ── SetChildrenPaddingCmd ─────────────────────────────────────────────────────

class SetChildrenPaddingCmd : public QUndoCommand {
public:
    SetChildrenPaddingCmd(SceneDocument*  doc,
                          std::string      gi,
                          std::string      ei,
                          float            top, float right, float bottom, float left,
                          QUndoCommand*   parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    std::string    m_ei;
    float          m_before[4];
    float          m_after[4];
};

// ── SetCornerRadiusCmd ────────────────────────────────────────────────────────

class SetCornerRadiusCmd : public QUndoCommand {
public:
    SetCornerRadiusCmd(SceneDocument*  doc,
                       std::string      gi,
                       std::string      ei,
                       float            tl, float tr, float br, float bl,
                       QUndoCommand*   parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    std::string    m_ei;
    float          m_before[4];
    float          m_after[4];
};

// ── RenameGraphicCmd ──────────────────────────────────────────────────────────
// Renames a graphic's ID string and keeps m_elementRefs in sync.

class RenameGraphicCmd : public QUndoCommand {
public:
    RenameGraphicCmd(SceneDocument* doc,
                     std::string     before,
                     std::string     after,
                     QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_before;
    std::string    m_after;

    void applyRename(const std::string& from, const std::string& to);
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
                     std::string     gi,
                     QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    nlohmann::json m_snapshot;    // full JSON backup captured in ctor
    int            m_savedZOrder;
    int            m_savedPosition; // original vector index, needed for undo restore
};

// ── Structural: Add / Remove Element ─────────────────────────────────────────

class AddElementCmd : public QUndoCommand {
public:
    AddElementCmd(SceneDocument*  doc,
                  std::string      gi,
                  nlohmann::json   elementJson,
                  QUndoCommand*   parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    nlohmann::json m_json;
    std::string    m_id;
};

class RemoveElementCmd : public QUndoCommand {
public:
    RemoveElementCmd(SceneDocument* doc,
                     std::string     gi,
                     std::string     ei,
                     QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    std::string    m_ei;
    nlohmann::json m_snapshot;
    int            m_savedPosition; // original element index, needed for undo restore
};

// ── Structural: Move Graphic / Element ───────────────────────────────────────

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
                   std::string     gi,
                   int             fromIndex,
                   int             toIndex,
                   QUndoCommand*  parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SceneDocument* m_doc;
    std::string    m_gi;
    int            m_from;
    int            m_to;

    void doMove(int from, int to);
};
