#include "ElementPropertiesWidget.h"
#include "AnimationWidget.h"

#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "engine/scene.h"
#include "engine/graphic.h"
#include "engine/element.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QString>

static QDoubleSpinBox* makeBoundsSpin(QWidget* parent)
{
    auto* sb = new QDoubleSpinBox(parent);
    sb->setRange(-4000.0, 4000.0);
    sb->setDecimals(1);
    sb->setSingleStep(1.0);
    return sb;
}

ElementPropertiesWidget::ElementPropertiesWidget(SceneDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_doc(doc)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // ── Identity ──────────────────────────────────────────────────────────────
    auto* identityBox = new QGroupBox("Identity", this);
    auto* identForm   = new QFormLayout(identityBox);

    m_idEdit = new QLineEdit(this);
    identForm->addRow("ID:", m_idEdit);

    m_zOrderSpin = new QSpinBox(this);
    m_zOrderSpin->setRange(-9999, 9999);
    identForm->addRow("Z Order:", m_zOrderSpin);

    layout->addWidget(identityBox);

    // ── Transform ────────────────────────────────────────────────────────────
    auto* transformBox  = new QGroupBox("Transform", this);
    auto* transformForm = new QFormLayout(transformBox);

    m_xSpin = makeBoundsSpin(this);
    transformForm->addRow("X:", m_xSpin);

    m_ySpin = makeBoundsSpin(this);
    transformForm->addRow("Y:", m_ySpin);

    m_wSpin = makeBoundsSpin(this);
    transformForm->addRow("W:", m_wSpin);

    m_hSpin = makeBoundsSpin(this);
    transformForm->addRow("H:", m_hSpin);

    m_rotationSpin = new QDoubleSpinBox(this);
    m_rotationSpin->setRange(-360.0, 360.0);
    m_rotationSpin->setDecimals(1);
    m_rotationSpin->setSingleStep(0.5);
    m_rotationSpin->setSuffix("°");
    transformForm->addRow("Rotation:", m_rotationSpin);

    layout->addWidget(transformBox);

    // ── Appearance ────────────────────────────────────────────────────────────
    auto* appearBox  = new QGroupBox("Appearance", this);
    auto* appearForm = new QFormLayout(appearBox);

    m_opacitySpin = new QDoubleSpinBox(this);
    m_opacitySpin->setRange(0.0, 1.0);
    m_opacitySpin->setDecimals(3);
    m_opacitySpin->setSingleStep(0.01);
    appearForm->addRow("Opacity:", m_opacitySpin);

    m_strokeWidthSpin = new QDoubleSpinBox(this);
    m_strokeWidthSpin->setRange(0.0, 100.0);
    m_strokeWidthSpin->setDecimals(1);
    m_strokeWidthSpin->setSingleStep(0.5);
    m_strokeWidthSpin->setSuffix(" px");
    appearForm->addRow("Stroke Width:", m_strokeWidthSpin);

    layout->addWidget(appearBox);

    // ── References ────────────────────────────────────────────────────────────
    auto* refBox  = new QGroupBox("References", this);
    auto* refForm = new QFormLayout(refBox);

    m_maskCombo = new QComboBox(this);
    refForm->addRow("Mask:", m_maskCombo);

    m_parentCombo = new QComboBox(this);
    refForm->addRow("Parent:", m_parentCombo);

    layout->addWidget(refBox);

    // ── Animations ────────────────────────────────────────────────────────────
    m_animIn  = new AnimationWidget("Animate In",  this);
    m_animOut = new AnimationWidget("Animate Out", this);
    layout->addWidget(m_animIn);
    layout->addWidget(m_animOut);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_idEdit, &QLineEdit::editingFinished,
            this, &ElementPropertiesWidget::onIdEditingFinished);

    connect(m_zOrderSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onZOrderChanged);

    connect(m_xSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onXChanged);
    connect(m_ySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onYChanged);
    connect(m_wSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onWChanged);
    connect(m_hSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onHChanged);
    connect(m_rotationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onRotationChanged);
    connect(m_opacitySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onOpacityChanged);
    connect(m_strokeWidthSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ElementPropertiesWidget::onStrokeWidthChanged);

    connect(m_maskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ElementPropertiesWidget::onMaskChanged);
    connect(m_parentCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ElementPropertiesWidget::onParentChanged);

    connect(m_animIn,  &AnimationWidget::animationDefChanged,
            this, &ElementPropertiesWidget::onAnimInChanged);
    connect(m_animOut, &AnimationWidget::animationDefChanged,
            this, &ElementPropertiesWidget::onAnimOutChanged);

    connect(m_doc, &SceneDocument::documentChanged,
            this, &ElementPropertiesWidget::onDocumentChanged);
}

void ElementPropertiesWidget::setSelection(int gi, int ei)
{
    m_gi = gi;
    m_ei = ei;
    onDocumentChanged();
}

void ElementPropertiesWidget::refresh()
{
    onDocumentChanged();
}

void ElementPropertiesWidget::onDocumentChanged()
{
    if (m_updating) return;
    if (m_gi < 0 || m_ei < 0) return;

    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    const Graphic& g = s.graphics[m_gi];
    if (m_ei >= static_cast<int>(g.elements.size())) return;

    m_updating = true;
    const Element& el = g.elements[m_ei];

    m_idEdit->setText(QString::fromStdString(el.id));
    m_zOrderSpin->setValue(el.zOrder);
    m_xSpin->setValue(el.bounds.x);
    m_ySpin->setValue(el.bounds.y);
    m_wSpin->setValue(el.bounds.width);
    m_hSpin->setValue(el.bounds.height);
    m_rotationSpin->setValue(static_cast<double>(el.rotation));
    m_opacitySpin->setValue(static_cast<double>(el.opacity));
    m_strokeWidthSpin->setValue(static_cast<double>(el.strokeWidth));

    m_animIn->setAnimationDef(el.inAnimation);
    m_animOut->setAnimationDef(el.outAnimation);

    populateRefCombos();

    m_updating = false;
}

void ElementPropertiesWidget::populateRefCombos()
{
    if (m_gi < 0 || m_ei < 0) return;
    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    const Graphic& g = s.graphics[m_gi];
    const Element& el = g.elements[m_ei];

    // Populate mask combo: "None" + all element IDs in same graphic (except self)
    m_maskCombo->blockSignals(true);
    m_maskCombo->clear();
    m_maskCombo->addItem("None", QVariant(""));
    int maskIdx = 0;
    for (int i = 0; i < static_cast<int>(g.elements.size()); ++i) {
        if (i == m_ei) continue;
        const std::string& elemId = g.elements[i].id;
        m_maskCombo->addItem(QString::fromStdString(elemId), QVariant(QString::fromStdString(elemId)));
        // Check if this is the current mask
        if (el.mask && el.mask->id == elemId) {
            maskIdx = m_maskCombo->count() - 1;
        }
    }
    m_maskCombo->setCurrentIndex(maskIdx);
    m_maskCombo->blockSignals(false);

    // Populate parent combo similarly
    m_parentCombo->blockSignals(true);
    m_parentCombo->clear();
    m_parentCombo->addItem("None", QVariant(""));
    int parentIdx = 0;
    for (int i = 0; i < static_cast<int>(g.elements.size()); ++i) {
        if (i == m_ei) continue;
        const std::string& elemId = g.elements[i].id;
        m_parentCombo->addItem(QString::fromStdString(elemId), QVariant(QString::fromStdString(elemId)));
        if (el.parent && el.parent->id == elemId) {
            parentIdx = m_parentCombo->count() - 1;
        }
    }
    m_parentCombo->setCurrentIndex(parentIdx);
    m_parentCombo->blockSignals(false);
}

void ElementPropertiesWidget::onIdEditingFinished()
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    std::string newId = m_idEdit->text().toStdString();
    const Scene& s = m_doc->scene();
    if (m_gi >= static_cast<int>(s.graphics.size())) return;
    if (m_ei >= static_cast<int>(s.graphics[m_gi].elements.size())) return;
    if (s.graphics[m_gi].elements[m_ei].id == newId) return;

    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_gi, m_ei, newId,
        [](Element& e) -> std::string& { return e.id; },
        "id"
    ));
}

void ElementPropertiesWidget::onZOrderChanged(int value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<int>(
        m_doc, m_gi, m_ei, value,
        [](Element& e) -> int& { return e.zOrder; },
        "zOrder", ElemMergeTag::ZOrder
    ));
}

void ElementPropertiesWidget::onXChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_gi, m_ei, value,
        [](Element& e) -> double& { return e.bounds.x; },
        "x", ElemMergeTag::X
    ));
}

void ElementPropertiesWidget::onYChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_gi, m_ei, value,
        [](Element& e) -> double& { return e.bounds.y; },
        "y", ElemMergeTag::Y
    ));
}

void ElementPropertiesWidget::onWChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_gi, m_ei, value,
        [](Element& e) -> double& { return e.bounds.width; },
        "width", ElemMergeTag::W
    ));
}

void ElementPropertiesWidget::onHChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_gi, m_ei, value,
        [](Element& e) -> double& { return e.bounds.height; },
        "height", ElemMergeTag::H
    ));
}

void ElementPropertiesWidget::onRotationChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_gi, m_ei, static_cast<float>(value),
        [](Element& e) -> float& { return e.rotation; },
        "rotation", ElemMergeTag::Rotation
    ));
}

void ElementPropertiesWidget::onOpacityChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_gi, m_ei, static_cast<float>(value),
        [](Element& e) -> float& { return e.opacity; },
        "opacity", ElemMergeTag::Opacity
    ));
}

void ElementPropertiesWidget::onStrokeWidthChanged(double value)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_gi, m_ei, static_cast<float>(value),
        [](Element& e) -> float& { return e.strokeWidth; },
        "strokeWidth", ElemMergeTag::StrokeW
    ));
}

void ElementPropertiesWidget::onMaskChanged(int /*index*/)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    const Scene& s = m_doc->scene();
    if (m_gi >= (int)s.graphics.size()) return;
    const Element& el = s.graphics[m_gi].elements[m_ei];

    std::string maskId   = m_maskCombo->currentData().toString().toStdString();
    std::string parentId = el.parent ? el.parent->id : "";
    m_doc->setElementRef(m_gi, m_ei, maskId, parentId);
}

void ElementPropertiesWidget::onParentChanged(int /*index*/)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    const Scene& s = m_doc->scene();
    if (m_gi >= (int)s.graphics.size()) return;
    const Element& el = s.graphics[m_gi].elements[m_ei];

    std::string maskId   = el.mask ? el.mask->id : "";
    std::string parentId = m_parentCombo->currentData().toString().toStdString();
    m_doc->setElementRef(m_gi, m_ei, maskId, parentId);
}

void ElementPropertiesWidget::onAnimInChanged(AnimationDef def)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementAnimCmd(
        m_doc, m_gi, m_ei,
        SetElementAnimCmd::Target::AnimIn,
        def
    ));
}

void ElementPropertiesWidget::onAnimOutChanged(AnimationDef def)
{
    if (m_updating || m_gi < 0 || m_ei < 0) return;
    m_doc->undoStack()->push(new SetElementAnimCmd(
        m_doc, m_gi, m_ei,
        SetElementAnimCmd::Target::AnimOut,
        def
    ));
}
