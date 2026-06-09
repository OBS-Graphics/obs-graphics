#include "graphicproperties.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"

#include <QFormLayout>
#include <QVBoxLayout>

GraphicProperties::GraphicProperties(SceneDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_doc(doc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_idEdit = new QLineEdit(this);
    form->addRow("ID:", m_idEdit);

    m_zOrderSpin = new QSpinBox(this);
    m_zOrderSpin->setRange(-9999, 9999);
    form->addRow("Z Order:", m_zOrderSpin);

    layout->addLayout(form);
    layout->addStretch(1);

    connect(m_idEdit,    &QLineEdit::editingFinished, this, &GraphicProperties::onIdEditingFinished);
    connect(m_zOrderSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &GraphicProperties::onZOrderChanged);
    connect(m_doc, &SceneDocument::documentChanged,
            this, &GraphicProperties::onDocumentChanged);
}

void GraphicProperties::setSelection(const std::string& graphicId)
{
    m_graphicId = graphicId;
    onDocumentChanged();
}

void GraphicProperties::onDocumentChanged()
{
    if (m_updating) return;

    const Scene& s = m_doc->scene();
    const Graphic* g = nullptr;
    for (const auto& gr : s.graphics)
        if (gr.id == m_graphicId) { g = &gr; break; }

    if (!g) return;

    m_updating = true;
    m_idEdit->setText(QString::fromStdString(g->id));
    m_zOrderSpin->setValue(g->zOrder);
    m_updating = false;
}

void GraphicProperties::onIdEditingFinished()
{
    if (m_updating || m_graphicId.empty()) return;
    const std::string newId = m_idEdit->text().trimmed().toStdString();
    if (newId.empty() || newId == m_graphicId) return;

    // Check uniqueness
    for (const auto& g : m_doc->scene().graphics)
        if (g.id != m_graphicId && g.id == newId) return;

    const std::string oldId = m_graphicId;
    m_graphicId = newId;
    m_doc->undoStack()->push(new RenameGraphicCmd(m_doc, oldId, newId));
}

void GraphicProperties::onZOrderChanged(int value)
{
    if (m_updating || m_graphicId.empty()) return;
    m_doc->undoStack()->push(new SetGraphicFieldCmd<int>(
        m_doc, m_graphicId, value,
        [](Graphic& g) -> int& { return g.zOrder; }, "z_order"
    ));
}
