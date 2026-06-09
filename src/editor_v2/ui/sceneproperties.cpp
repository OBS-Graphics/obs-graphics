#include "sceneproperties.h"
#include "engine/scene.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

const SceneProperties::Preset SceneProperties::kPresets[] = {
    {"1920 × 1080  (16:9 FHD)", 1920, 1080}, {"1280 × 720   (16:9 HD)", 1280, 720},
    {"3840 × 2160  (4K UHD)", 3840, 2160},   {"1080 × 1920  (9:16 Vert)", 1080, 1920},
    {"2560 × 1440  (16:9 QHD)", 2560, 1440}, {"Custom", 0, 0},
};

SceneProperties::SceneProperties(SceneDocument* doc, QWidget* parent) : QWidget(parent), m_doc(doc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);

    m_nameEdit = new QLineEdit(this);
    form->addRow("Name:", m_nameEdit);

    m_presetCombo = new QComboBox(this);
    for (const auto& p : kPresets)
        m_presetCombo->addItem(p.label);
    form->addRow("Preset:", m_presetCombo);

    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(1, 7680);
    m_widthSpin->setSuffix(" px");
    form->addRow("Width:", m_widthSpin);

    m_heightSpin = new QSpinBox(this);
    m_heightSpin->setRange(1, 7680);
    m_heightSpin->setSuffix(" px");
    form->addRow("Height:", m_heightSpin);

    layout->addLayout(form);
    layout->addStretch();

    connect(m_nameEdit, &QLineEdit::editingFinished, this, &SceneProperties::onNameEditingFinished);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &SceneProperties::onPresetChanged);
    connect(m_widthSpin, &QSpinBox::editingFinished, this, &SceneProperties::onWidthFinished);
    connect(m_heightSpin, &QSpinBox::editingFinished, this, &SceneProperties::onHeightFinished);
    connect(m_doc, &SceneDocument::documentChanged, this, &SceneProperties::onDocumentChanged);

    onDocumentChanged();
}

int SceneProperties::findPresetIndex(int w, int h) const
{
    for (int i = 0; i < kCustomIndex; ++i) {
        if (kPresets[i].w == w && kPresets[i].h == h)
            return i;
    }
    return kCustomIndex;
}

void SceneProperties::onDocumentChanged()
{
    if (m_updating)
        return;
    m_updating = true;

    const Scene& s = m_doc->scene();
    m_nameEdit->setText(m_doc->sceneName());
    m_widthSpin->setValue(s.width);
    m_heightSpin->setValue(s.height);
    m_presetCombo->setCurrentIndex(findPresetIndex(s.width, s.height));

    m_updating = false;
}

void SceneProperties::onNameEditingFinished()
{
    if (m_updating)
        return;
    QString newName = m_nameEdit->text().trimmed();
    if (newName == m_doc->sceneName())
        return;
    m_doc->undoStack()->push(new SetSceneNameCmd(m_doc, newName));
}

void SceneProperties::onPresetChanged(int index)
{
    if (m_updating || index < 0 || index >= kCustomIndex)
        return;
    int w = kPresets[index].w;
    int h = kPresets[index].h;
    if (w == m_doc->scene().width && h == m_doc->scene().height)
        return;

    m_updating = true;
    m_widthSpin->setValue(w);
    m_heightSpin->setValue(h);
    m_updating = false;

    m_doc->undoStack()->push(new SetSceneDimensionsCmd(m_doc, w, h));
}

void SceneProperties::onWidthFinished()
{
    if (m_updating)
        return;
    int w = m_widthSpin->value();
    int h = m_heightSpin->value();
    if (w == m_doc->scene().width)
        return;
    m_doc->undoStack()->push(new SetSceneDimensionsCmd(m_doc, w, h));
}

void SceneProperties::onHeightFinished()
{
    if (m_updating)
        return;
    int w = m_widthSpin->value();
    int h = m_heightSpin->value();
    if (h == m_doc->scene().height)
        return;
    m_doc->undoStack()->push(new SetSceneDimensionsCmd(m_doc, w, h));
}
