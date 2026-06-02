#include "PropertiesPanel.h"

#include "properties/ScenePropertiesWidget.h"
#include "properties/GraphicPropertiesWidget.h"
#include "properties/RectanglePropertiesWidget.h"
#include "properties/TextPropertiesWidget.h"

#include "model/SceneDocument.h"
#include "model/EditorScene.h"
#include "engine/scene.h"
#include "engine/graphic.h"
#include "engine/element.h"

#include <QLabel>
#include <QVBoxLayout>

PropertiesPanel::PropertiesPanel(SceneDocument* doc, EditorScene* editorState,
                                 QWidget* parent)
    : QScrollArea(parent)
    , m_doc(doc)
    , m_editorState(editorState)
{
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_stack = new AdaptiveStack;

    // Page 0: Empty (nothing selected)
    m_emptyPage = new QWidget;
    auto* emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->addWidget(new QLabel("Nothing selected", m_emptyPage));
    emptyLayout->addStretch();
    m_stack->addWidget(m_emptyPage);

    // Page 1: Scene properties
    m_sceneProps = new ScenePropertiesWidget(doc, m_stack);
    m_stack->addWidget(m_sceneProps);

    // Page 2: Graphic properties
    m_graphicProps = new GraphicPropertiesWidget(doc, m_stack);
    m_stack->addWidget(m_graphicProps);

    // Page 3: Rectangle element properties
    m_rectProps = new RectanglePropertiesWidget(doc, m_stack);
    m_stack->addWidget(m_rectProps);

    // Page 4: Text element properties
    m_textProps = new TextPropertiesWidget(doc, m_stack);
    m_stack->addWidget(m_textProps);

    m_stack->setCurrentWidget(m_emptyPage);
    setWidget(m_stack);

    connect(editorState, &EditorScene::selectionChanged,
            this, &PropertiesPanel::onSelectionChanged);
}

void PropertiesPanel::onSelectionChanged(SelectionId id)
{
    switch (id.level) {
    case SelectionId::Level::None:
        m_stack->setCurrentWidget(m_emptyPage);
        break;

    case SelectionId::Level::Scene:
        m_sceneProps->setSelection();
        m_stack->setCurrentWidget(m_sceneProps);
        break;

    case SelectionId::Level::Graphic:
        m_graphicProps->setSelection(id.graphicIndex);
        m_stack->setCurrentWidget(m_graphicProps);
        break;

    case SelectionId::Level::Element:
    {
        int gi = id.graphicIndex;
        int ei = id.elementIndex;
        const Scene& s = m_doc->scene();
        if (gi < 0 || gi >= static_cast<int>(s.graphics.size())) {
            m_stack->setCurrentWidget(m_emptyPage);
            break;
        }
        const Graphic& g = s.graphics[gi];
        if (ei < 0 || ei >= static_cast<int>(g.elements.size())) {
            m_stack->setCurrentWidget(m_emptyPage);
            break;
        }
        ElementType type = g.elements[ei].type;
        if (type == ElementType::Rectangle) {
            m_rectProps->setSelection(gi, ei);
            m_stack->setCurrentWidget(m_rectProps);
        } else {
            m_textProps->setSelection(gi, ei);
            m_stack->setCurrentWidget(m_textProps);
        }
        break;
    }
    }
}
