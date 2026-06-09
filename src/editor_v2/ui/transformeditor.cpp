#include "transformeditor.h"
#include <QGridLayout>
#include <QLabel>

static QLabel* sectionLabel(const QString& text)
{
    auto* lbl = new QLabel(text);
    lbl->setStyleSheet("QLabel { font-weight: bold; color: #aaa; }");
    return lbl;
}

static QDoubleSpinBox* makeSpinBox()
{
    auto* sb = new QDoubleSpinBox;
    sb->setRange(-9999, 9999);
    sb->setDecimals(1);
    return sb;
}

TransformEditor::TransformEditor(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QGridLayout(this);

    layout->addWidget(sectionLabel("Position (X/Y)"), 0, 0, 1, 2);
    spinX = makeSpinBox();
    spinY = makeSpinBox();
    layout->addWidget(spinX, 1, 0);
    layout->addWidget(spinY, 1, 1);

    layout->addWidget(sectionLabel("Size (Width/Height)"), 2, 0, 1, 2);
    spinW = makeSpinBox();
    spinH = makeSpinBox();
    layout->addWidget(spinW, 3, 0);
    layout->addWidget(spinH, 3, 1);

    layout->addWidget(sectionLabel("Rotation"), 4, 0, 1, 2);
    spinRot = makeSpinBox();
    spinRot->setSuffix("°");
    layout->addWidget(spinRot, 5, 0, 1, 2);

    connect(spinX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &TransformEditor::onSpinXChanged);
    connect(spinY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &TransformEditor::onSpinYChanged);
    connect(spinW, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &TransformEditor::onSpinWChanged);
    connect(spinH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &TransformEditor::onSpinHChanged);
    connect(spinRot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &TransformEditor::onSpinRotChanged);
}

void TransformEditor::load(const Rectangle& bounds, float rotation)
{
    spinX->blockSignals(true);
    spinY->blockSignals(true);
    spinW->blockSignals(true);
    spinH->blockSignals(true);
    spinRot->blockSignals(true);

    spinX->setValue(bounds.x);
    spinY->setValue(bounds.y);
    spinW->setValue(bounds.width);
    spinH->setValue(bounds.height);
    spinRot->setValue(static_cast<double>(rotation));

    spinX->blockSignals(false);
    spinY->blockSignals(false);
    spinW->blockSignals(false);
    spinH->blockSignals(false);
    spinRot->blockSignals(false);
}

void TransformEditor::onSpinXChanged(double value)
{
    emit xChanged(value);
}
void TransformEditor::onSpinYChanged(double value)
{
    emit yChanged(value);
}
void TransformEditor::onSpinWChanged(double value)
{
    emit wChanged(value);
}
void TransformEditor::onSpinHChanged(double value)
{
    emit hChanged(value);
}
void TransformEditor::onSpinRotChanged(double value)
{
    emit rotChanged(value);
}
