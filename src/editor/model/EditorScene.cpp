#include "EditorScene.h"
#include "SceneDocument.h"

EditorScene::EditorScene(SceneDocument* doc, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
{
}

void EditorScene::setSelection(const SelectionId& id)
{
    if (m_selection == id)
        return;
    m_selection = id;
    emit selectionChanged(m_selection);
}
