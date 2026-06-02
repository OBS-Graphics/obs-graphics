#pragma once

#include <QTreeView>

#include "model/EditorScene.h"

class SceneDocument;
class EditorScene;
class SceneTreeModel;

class SceneTreeView : public QTreeView {
    Q_OBJECT
public:
    explicit SceneTreeView(SceneDocument* doc,
                           EditorScene*   editorScene,
                           QWidget*       parent = nullptr);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onViewSelectionChanged(const QItemSelection& selected,
                                const QItemSelection& deselected);
    void onEditorSelectionChanged(SelectionId id);

private:
    SceneDocument*  m_doc;
    EditorScene*    m_editorScene;
    SceneTreeModel* m_model;
    bool            m_syncingSelection{false};
};
