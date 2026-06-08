#include "styleproperties.h"
#include "ui/painteditor.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"
#include "engine/graphic.h"
#include "engine/element.h"
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

StyleProperties::StyleProperties(SceneDocument* doc, QWidget *parent)
    : QWidget{parent}
    , m_doc(doc)
{
    auto* root = new QVBoxLayout(this);
    {
        auto* panel = new QGroupBox("Stroke");
        panel->setStyleSheet("QGroupBox { font-weight: bold; color: #aaa; }");
        auto* layout = new QGridLayout(panel);

        auto* label = new QLabel("Width");
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(label, 0, 0);

        m_strokeWidth = new QDoubleSpinBox();
        m_strokeWidth->setMaximum(999.0);
        m_strokeWidth->setSingleStep(0.5);
        layout->addWidget(m_strokeWidth, 0, 1);

        m_strokePaint = new PaintEditor();
        layout->addWidget(m_strokePaint, 1, 0, 1, 2);

        root->addWidget(panel);
    }

    {
        auto* panel = new QGroupBox("Fill", this);
        panel->setStyleSheet("QGroupBox { font-weight: bold; color: #aaa; }");
        auto* layout = new QVBoxLayout(panel);

        m_fillPaint = new PaintEditor();
        layout->addWidget(m_fillPaint);

        root->addWidget(panel);
    }

    {
        auto* panel = new QGroupBox("Corner Radius", this);
        auto* layout = new QVBoxLayout(panel);
        panel->setStyleSheet("QGroupBox { font-weight: bold; color: #aaa; }");
        m_cornerRadius = new CornerRadiusEditor();
        layout->addWidget(m_cornerRadius);

        root->addWidget(panel);
    }

    root->addStretch(1);

    setEnabled(false);

    connect(m_strokeWidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &StyleProperties::onStrokeWidthChanged);
    connect(m_fillPaint,   &PaintEditor::paintChanged,
            this, &StyleProperties::onFillPaintChanged);
    connect(m_strokePaint, &PaintEditor::paintChanged,
            this, &StyleProperties::onStrokePaintChanged);
    connect(m_cornerRadius, &CornerRadiusEditor::valuesChanged,
            this, &StyleProperties::onCornerRadiusChanged);

    connect(m_doc, &SceneDocument::documentChanged,
            this, &StyleProperties::onDocumentChanged);
}

void StyleProperties::setSelection(const std::string& graphicId, const std::string& elementId)
{
    m_graphicId = graphicId;
    m_elementId = elementId;
    onDocumentChanged();
}

void StyleProperties::onDocumentChanged()
{
    if (m_updating) return;
    if (m_graphicId.empty() || m_elementId.empty()) {
        setEnabled(false);
        return;
    }

    Scene& s = m_doc->scene();
    Element* el = nullptr;
    try {
        el = &s.GetById(m_graphicId).GetById(m_elementId);
    } catch (const std::runtime_error&) {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_updating = true;

    m_strokeWidth->setValue(static_cast<double>(el->strokeWidth));
    m_fillPaint->setPaint(el->fill);
    m_strokePaint->setPaint(el->stroke);
    m_cornerRadius->setValues(
        static_cast<float>(el->cornerRadius[0]),
        static_cast<float>(el->cornerRadius[1]),
        static_cast<float>(el->cornerRadius[2]),
        static_cast<float>(el->cornerRadius[3])
    );

    m_updating = false;
}

void StyleProperties::onStrokeWidthChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty()) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(value),
        [](Element& e) -> float& { return e.strokeWidth; },
        "strokeWidth", ElemMergeTag::StrokeW
    ));
}

void StyleProperties::onFillPaintChanged(const Paint& paint)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty()) return;
    m_doc->undoStack()->push(new SetElementPaintCmd(
        m_doc, m_graphicId, m_elementId, SetElementPaintCmd::Target::Fill, paint
    ));
}

void StyleProperties::onStrokePaintChanged(const Paint& paint)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty()) return;
    m_doc->undoStack()->push(new SetElementPaintCmd(
        m_doc, m_graphicId, m_elementId, SetElementPaintCmd::Target::Stroke, paint
    ));
}

void StyleProperties::onCornerRadiusChanged(float tl, float tr, float br, float bl)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty()) return;
    m_doc->undoStack()->push(new SetCornerRadiusCmd(
        m_doc, m_graphicId, m_elementId, tl, tr, br, bl
    ));
}
