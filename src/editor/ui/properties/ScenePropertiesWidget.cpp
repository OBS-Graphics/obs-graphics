#include "ScenePropertiesWidget.h"

#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"

#include <QLineEdit>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>

ScenePropertiesWidget::ScenePropertiesWidget(SceneDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_doc(doc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* form = new QFormLayout;

    m_nameEdit = new QLineEdit(this);
    form->addRow("Name:", m_nameEdit);

    m_graphicsCountLabel = new QLabel("Graphics: 0", this);
    form->addRow("", m_graphicsCountLabel);

    layout->addLayout(form);
    layout->addStretch();

    connect(m_nameEdit, &QLineEdit::editingFinished,
            this, &ScenePropertiesWidget::onNameEditingFinished);

    connect(m_doc, &SceneDocument::documentChanged,
            this, &ScenePropertiesWidget::onDocumentChanged);
}

void ScenePropertiesWidget::setSelection()
{
    onDocumentChanged();
}

void ScenePropertiesWidget::onDocumentChanged()
{
    if (m_updating) return;
    m_updating = true;

    const Scene& s = m_doc->scene();
    m_nameEdit->setText(m_doc->sceneName());
    m_graphicsCountLabel->setText(QString("Graphics: %1").arg(s.graphics.size()));

    m_updating = false;
}

void ScenePropertiesWidget::onNameEditingFinished()
{
    if (m_updating) return;
    QString newName = m_nameEdit->text();
    if (newName == m_doc->sceneName()) return;

    m_doc->undoStack()->push(new SetSceneNameCmd(m_doc, newName));
}
