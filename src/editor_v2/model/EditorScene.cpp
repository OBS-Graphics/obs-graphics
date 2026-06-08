#include "EditorScene.h"
#include "SceneDocument.h"
#include "engine/scene.h"

EditorScene::EditorScene(SceneDocument* doc, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
{
    connect(m_doc, &SceneDocument::documentChanged,
            this,  &EditorScene::validateSelection);
}

void EditorScene::validateSelection()
{
    const Scene& s = m_doc->scene();
    const SelectionId& cur = m_selection;

    if (cur.level == SelectionId::Level::Graphic) {
        if (cur.graphicIndex < 0 || cur.graphicIndex >= (int)s.graphics.size())
            setSelection(SelectionId{});
    } else if (cur.level == SelectionId::Level::Element) {
        if (cur.graphicIndex < 0 || cur.graphicIndex >= (int)s.graphics.size() ||
            cur.elementIndex < 0 || cur.elementIndex >= (int)s.graphics[cur.graphicIndex].elements.size())
            setSelection(SelectionId{});
    }
}

void EditorScene::setSelection(const SelectionId& id)
{
    if (m_selection == id)
        return;
    m_selection = id;
    emit selectionChanged(m_selection);
}
