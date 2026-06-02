#include "RectanglePropertiesWidget.h"
#include "ElementPropertiesWidget.h"
#include "ui/widgets/PaintEditorWidget.h"
#include "ui/widgets/CornerRadiusWidget.h"

#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"
#include "engine/graphic.h"
#include "engine/element.h"

#include <QGroupBox>
#include <QVBoxLayout>

RectanglePropertiesWidget::RectanglePropertiesWidget(SceneDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_doc(doc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // 1) Common element properties
    m_elementProps = new ElementPropertiesWidget(m_doc, this);
    layout->addWidget(m_elementProps);

    // 2) Fill
    auto* fillBox    = new QGroupBox("Fill", this);
    auto* fillLayout = new QVBoxLayout(fillBox);
    m_fillEditor = new PaintEditorWidget(fillBox);
    fillLayout->addWidget(m_fillEditor);
    layout->addWidget(fillBox);

    // 3) Stroke
    auto* strokeBox    = new QGroupBox("Stroke", this);
    auto* strokeLayout = new QVBoxLayout(strokeBox);
    m_strokeEditor = new PaintEditorWidget(strokeBox);
    strokeLayout->addWidget(m_strokeEditor);
    layout->addWidget(strokeBox);

    // 4) Shape (corner radius)
    auto* shapeBox    = new QGroupBox("Shape", this);
    auto* shapeLayout = new QVBoxLayout(shapeBox);
    m_cornerRadius = new CornerRadiusWidget(shapeBox);
    shapeLayout->addWidget(m_cornerRadius);
    layout->addWidget(shapeBox);

    layout->addStretch();

    // Connections
    connect(m_fillEditor,   &PaintEditorWidget::paintChanged,
            this, &RectanglePropertiesWidget::onFillChanged);
    connect(m_strokeEditor, &PaintEditorWidget::paintChanged,
            this, &RectanglePropertiesWidget::onStrokeChanged);
    connect(m_cornerRadius, &CornerRadiusWidget::valuesChanged,
            this, &RectanglePropertiesWidget::onCornerRadiusChanged);

    connect(m_doc, &SceneDocument::documentChanged,
            this, &RectanglePropertiesWidget::onDocumentChanged);
}

void RectanglePropertiesWidget::setSelection(int gi, int ei)
{
    m_gi = gi;
    m_ei = ei;
    m_elementProps->setSelection(gi, ei);
    onDocumentChanged();
}

void RectanglePropertiesWidget::onDocumentChanged()
{
    if (m_updating) return;
    if (m_gi < 0 || m_ei < 0) return;

    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    const Graphic& g = s.graphics[m_gi];
    if (m_ei >= static_cast<int>(g.elements.size())) return;

    m_updating = true;
    const Element& el = g.elements[m_ei];
    m_fillEditor->setPaint(el.fill);
    m_strokeEditor->setPaint(el.stroke);
    m_cornerRadius->setValues(
        el.cornerRadius[0], el.cornerRadius[1],
        el.cornerRadius[2], el.cornerRadius[3]
    );
    m_updating = false;
}

void RectanglePropertiesWidget::onFillChanged(Paint paint)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementPaintCmd(
        m_doc, m_gi, m_ei,
        SetElementPaintCmd::Target::Fill,
        paint
    ));
}

void RectanglePropertiesWidget::onStrokeChanged(Paint paint)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementPaintCmd(
        m_doc, m_gi, m_ei,
        SetElementPaintCmd::Target::Stroke,
        paint
    ));
}

void RectanglePropertiesWidget::onCornerRadiusChanged(float tl, float tr, float br, float bl)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    int gi = m_gi, ei = m_ei;
    float c0 = tl, c1 = tr, c2 = br, c3 = bl;
    m_doc->applyMutation([gi, ei, c0, c1, c2, c3](Scene& s) {
        if (gi >= static_cast<int>(s.graphics.size())) return;
        Graphic& g = s.graphics[gi];
        if (ei >= static_cast<int>(g.elements.size())) return;
        Element& el = g.elements[ei];
        el.cornerRadius[0] = c0;
        el.cornerRadius[1] = c1;
        el.cornerRadius[2] = c2;
        el.cornerRadius[3] = c3;
    });
}
