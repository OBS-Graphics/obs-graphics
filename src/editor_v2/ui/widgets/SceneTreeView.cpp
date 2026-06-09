#include "SceneTreeView.h"
#include "SceneTreeModel.h"

#include "model/EditorScene.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"

#include "engine/json.hpp"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>

SceneTreeView::SceneTreeView(SceneDocument* doc, EditorScene* editorScene, QWidget* parent)
    : QTreeView(parent), m_doc(doc), m_editorScene(editorScene),
      m_model(new SceneTreeModel(doc, this))
{
    setModel(m_model);
    setHeaderHidden(true);
    setDragEnabled(true);
    setAcceptDrops(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
    setSelectionMode(QAbstractItemView::SingleSelection);

    // Expand scene node by default
    expandAll();

    // Keep tree expanded after model resets
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { expandAll(); });

    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            &SceneTreeView::onViewSelectionChanged);

    connect(m_editorScene, &EditorScene::selectionChanged, this,
            &SceneTreeView::onEditorSelectionChanged);
}

void SceneTreeView::onViewSelectionChanged(const QItemSelection& selected,
                                           const QItemSelection& /*deselected*/)
{
    if (m_syncingSelection)
        return;

    m_syncingSelection = true;

    QModelIndexList indexes = selected.indexes();
    if (indexes.isEmpty()) {
        m_editorScene->setSelection(SelectionId{});
    } else {
        SelectionId sid = m_model->selectionIdFor(indexes.first());
        m_editorScene->setSelection(sid);
    }

    m_syncingSelection = false;
}

void SceneTreeView::onEditorSelectionChanged(SelectionId id)
{
    if (m_syncingSelection)
        return;

    m_syncingSelection = true;

    QModelIndex idx = m_model->indexForSelection(id);
    if (idx.isValid()) {
        selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
        scrollTo(idx);
    } else {
        selectionModel()->clearSelection();
    }

    m_syncingSelection = false;
}

void SceneTreeView::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    // Determine what is currently selected
    QModelIndex current = currentIndex();
    SelectionId sel = m_model->selectionIdFor(current);

    bool graphicSelected = (sel.level == SelectionId::Level::Graphic);
    bool elementSelected = (sel.level == SelectionId::Level::Element);
    bool hasGraphic = graphicSelected || elementSelected;

    // ── Add Graphic ──────────────────────────────────────────────────────────
    QAction* addGraphicAction = menu.addAction("Add Graphic");
    connect(addGraphicAction, &QAction::triggered, this, [this]() {
        using json = nlohmann::json;
        json j = json::parse(R"({"id":"graphic_1","z_order":0,"elements":[]})");
        m_doc->undoStack()->push(new AddGraphicCmd(m_doc, std::move(j)));

        // Expand scene node so new graphic is visible
        QModelIndex sceneIdx = m_model->index(0, 0);
        expand(sceneIdx);
    });

    // ── Add Rectangle Element ────────────────────────────────────────────────
    QAction* addRectAction = menu.addAction("Add Rectangle Element");
    addRectAction->setEnabled(hasGraphic);
    connect(addRectAction, &QAction::triggered, this, [this, sel]() {
        int gi = sel.graphicIndex;
        const std::string gid = m_doc->scene().graphics[gi].id;
        using json = nlohmann::json;
        json j = json::parse(
            R"({"id":"element_1","type":"rectangle","x":100,"y":100,"w":200,"h":80,"fill":[0.5,0.5,0.9,1.0],"z_order":0})");
        m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));

        // Expand the parent graphic node
        QModelIndex graphicIdx =
            m_model->indexForSelection(SelectionId{SelectionId::Level::Graphic, gi, -1});
        expand(graphicIdx);
    });

    // ── Add Text Element ─────────────────────────────────────────────────────
    QAction* addTextAction = menu.addAction("Add Text Element");
    addTextAction->setEnabled(hasGraphic);
    connect(addTextAction, &QAction::triggered, this, [this, sel]() {
        int gi = sel.graphicIndex;
        const std::string gid = m_doc->scene().graphics[gi].id;
        using json = nlohmann::json;
        json j = json::parse(
            R"({"id":"text_1","type":"text","x":100,"y":100,"w":300,"h":60,"text":"New Text","font_family":"Sans","font_size":36,"color":[1.0,1.0,1.0,1.0],"z_order":0})");
        m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));

        // Expand the parent graphic node
        QModelIndex graphicIdx =
            m_model->indexForSelection(SelectionId{SelectionId::Level::Graphic, gi, -1});
        expand(graphicIdx);
    });

    menu.addSeparator();

    // ── Remove ───────────────────────────────────────────────────────────────
    QAction* removeAction = menu.addAction("Remove");
    removeAction->setEnabled(graphicSelected || elementSelected);
    connect(removeAction, &QAction::triggered, this, [this, sel]() {
        if (sel.level == SelectionId::Level::Graphic) {
            const std::string gid = m_doc->scene().graphics[sel.graphicIndex].id;
            m_doc->undoStack()->push(new RemoveGraphicCmd(m_doc, gid));
        } else if (sel.level == SelectionId::Level::Element) {
            const auto& g = m_doc->scene().graphics[sel.graphicIndex];
            const std::string gid = g.id;
            const std::string eid = g.elements[sel.elementIndex].id;
            m_doc->undoStack()->push(new RemoveElementCmd(m_doc, gid, eid));
        }
    });

    menu.exec(event->globalPos());
}
