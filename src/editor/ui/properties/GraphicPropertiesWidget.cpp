#include "GraphicPropertiesWidget.h"

#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"
#include "engine/graphic.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>

GraphicPropertiesWidget::GraphicPropertiesWidget(SceneDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_doc(doc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* form = new QFormLayout;

    m_idEdit = new QLineEdit(this);
    form->addRow("ID:", m_idEdit);

    m_zOrderSpin = new QSpinBox(this);
    m_zOrderSpin->setRange(-9999, 9999);
    form->addRow("Z Order:", m_zOrderSpin);

    m_elementCountLabel = new QLabel("Elements: 0", this);
    form->addRow("", m_elementCountLabel);

    layout->addLayout(form);
    layout->addStretch();

    connect(m_idEdit, &QLineEdit::editingFinished,
            this, &GraphicPropertiesWidget::onIdEditingFinished);

    connect(m_zOrderSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &GraphicPropertiesWidget::onZOrderChanged);

    connect(m_doc, &SceneDocument::documentChanged,
            this, &GraphicPropertiesWidget::onDocumentChanged);
}

void GraphicPropertiesWidget::setSelection(int graphicIndex)
{
    m_gi = graphicIndex;
    onDocumentChanged();
}

void GraphicPropertiesWidget::onDocumentChanged()
{
    if (m_updating) return;
    if (m_gi < 0) return;

    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;

    m_updating = true;
    const Graphic& g = s.graphics[m_gi];
    m_idEdit->setText(QString::fromStdString(g.id));
    m_zOrderSpin->setValue(g.zOrder);
    m_elementCountLabel->setText(QString("Elements: %1").arg(g.elements.size()));
    m_updating = false;
}

void GraphicPropertiesWidget::onIdEditingFinished()
{
    if (m_updating || m_gi < 0) return;
    std::string newId = m_idEdit->text().toStdString();
    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    if (s.graphics[m_gi].id == newId) return;

    m_doc->undoStack()->push(new SetGraphicFieldCmd<std::string>(
        m_doc, m_gi, newId,
        [](Graphic& g) -> std::string& { return g.id; },
        "id"
    ));
}

void GraphicPropertiesWidget::onZOrderChanged(int value)
{
    if (m_updating || m_gi < 0) return;
    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    if (s.graphics[m_gi].zOrder == value) return;

    m_doc->undoStack()->push(new SetGraphicFieldCmd<int>(
        m_doc, m_gi, value,
        [](Graphic& g) -> int& { return g.zOrder; },
        "zOrder"
    ));
}
