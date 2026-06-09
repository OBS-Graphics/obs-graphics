#include "elementproperties.h"
#include "animationeditor.h"
#include "engine/element.h"
#include "engine/graphic.h"
#include "engine/scene.h"
#include "model/SceneDocument.h"
#include "model/UndoCommands.h"
#include "paddingeditor.h"
#include "transformeditor.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>

static QGroupBox* makeGroup(const QString& title)
{
    auto* box = new QGroupBox(title);
    box->setStyleSheet("QGroupBox { font-size: 16px; font-weight: bold; }");
    return box;
}

ElementProperties::ElementProperties(SceneDocument* doc, QWidget* parent)
    : QWidget(parent), m_doc(doc)
{
    auto* outerLayout = new QVBoxLayout(this);
    auto* innerLayout = new QVBoxLayout;
    outerLayout->addLayout(innerLayout);
    outerLayout->addStretch(1);

    // Identity
    auto* identityBox = makeGroup("Identity");
    auto* identityForm = new QFormLayout;
    identityForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    idLineEdit = new QLineEdit;
    idLineEdit->setPlaceholderText("Element name or identifier");
    zOrderSpinBox = new QSpinBox;
    zOrderSpinBox->setRange(-9999, 9999);
    identityForm->addRow("ID", idLineEdit);
    identityForm->addRow("Z Order", zOrderSpinBox);
    auto* identityBoxLayout = new QVBoxLayout(identityBox);
    identityBoxLayout->addLayout(identityForm);
    innerLayout->addWidget(identityBox);

    // Transform
    auto* transformBox = makeGroup("Transform");
    transformEditor = new TransformEditor;
    auto* transformBoxLayout = new QVBoxLayout(transformBox);
    transformBoxLayout->addWidget(transformEditor);
    innerLayout->addWidget(transformBox);

    // Appearance
    auto* appearanceBox = makeGroup("Appearance");
    auto* appearanceForm = new QFormLayout(appearanceBox);
    appearanceForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    opacitySpinBox = new QDoubleSpinBox;
    opacitySpinBox->setRange(0.0, 100.0);
    opacitySpinBox->setSuffix("%");
    opacitySpinBox->setDecimals(1);
    opacitySpinBox->setSingleStep(0.5);
    opacitySpinBox->setValue(100.0);
    appearanceForm->addRow("Opacity", opacitySpinBox);
    innerLayout->addWidget(appearanceBox);

    // References
    auto* referencesBox = makeGroup("References");
    auto* referencesForm = new QFormLayout(referencesBox);
    referencesForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    maskComboBox = new QComboBox;
    parentComboBox = new QComboBox;
    referencesForm->addRow("Mask", maskComboBox);
    referencesForm->addRow("Parent", parentComboBox);
    innerLayout->addWidget(referencesBox);

    // Animate In
    auto* animateInBox = makeGroup("Animate In");
    animateIn = new AnimationEditor;
    auto* animateInLayout = new QVBoxLayout(animateInBox);
    animateInLayout->addWidget(animateIn);
    innerLayout->addWidget(animateInBox);

    // Animate Out
    auto* animateOutBox = makeGroup("Animate Out");
    animateOut = new AnimationEditor;
    auto* animateOutLayout = new QVBoxLayout(animateOutBox);
    animateOutLayout->addWidget(animateOut);
    innerLayout->addWidget(animateOutBox);

    // Fit to Children
    m_fitChildrenBox = makeGroup("Fit to Children");
    auto* fitLayout = new QVBoxLayout(m_fitChildrenBox);
    m_fitChildrenCheck = new QCheckBox("Enabled");
    m_paddingEditor = new PaddingEditor;
    fitLayout->addWidget(m_fitChildrenCheck);
    fitLayout->addWidget(m_paddingEditor);
    innerLayout->addWidget(m_fitChildrenBox);

    setEnabled(false);

    // Connections
    connect(idLineEdit, &QLineEdit::editingFinished, this, &ElementProperties::onIdEditingFinished);
    connect(zOrderSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &ElementProperties::onZOrderChanged);
    connect(opacitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &ElementProperties::onOpacityChanged);

    connect(transformEditor, &TransformEditor::xChanged, this, &ElementProperties::onXChanged);
    connect(transformEditor, &TransformEditor::yChanged, this, &ElementProperties::onYChanged);
    connect(transformEditor, &TransformEditor::wChanged, this, &ElementProperties::onWChanged);
    connect(transformEditor, &TransformEditor::hChanged, this, &ElementProperties::onHChanged);
    connect(transformEditor, &TransformEditor::rotChanged, this, &ElementProperties::onRotChanged);

    connect(animateIn, &AnimationEditor::animationChanged, this,
            &ElementProperties::onAnimInChanged);
    connect(animateOut, &AnimationEditor::animationChanged, this,
            &ElementProperties::onAnimOutChanged);

    connect(maskComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ElementProperties::onMaskChanged);
    connect(parentComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ElementProperties::onParentChanged);

    connect(m_fitChildrenCheck, &QCheckBox::toggled, this,
            &ElementProperties::onFitToChildrenToggled);
    connect(m_paddingEditor, &PaddingEditor::paddingChanged, this,
            &ElementProperties::onChildrenPaddingChanged);

    connect(m_doc, &SceneDocument::documentChanged, this, &ElementProperties::onDocumentChanged);
}

void ElementProperties::setSelection(const std::string& graphicId, const std::string& elementId)
{
    m_graphicId = graphicId;
    m_elementId = elementId;
    onDocumentChanged();
}

void ElementProperties::onDocumentChanged()
{
    if (m_updating)
        return;
    if (m_graphicId.empty() || m_elementId.empty()) {
        setEnabled(false);
        return;
    }

    Scene& s = m_doc->scene();
    Graphic* g = nullptr;
    Element* el = nullptr;
    try {
        g = &s.GetById(m_graphicId);
        el = &g->GetById(m_elementId);
    } catch (const std::runtime_error&) {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_updating = true;

    bool usedAsParent = false;
    for (const auto& other : g->elements)
        if (other.parent == el) {
            usedAsParent = true;
            break;
        }
    idLineEdit->setReadOnly(usedAsParent);
    idLineEdit->setStyleSheet(usedAsParent ? "QLineEdit { color: #888888; font-style: italic; }"
                                           : "");
    idLineEdit->setToolTip(
        usedAsParent
            ? "ID is locked \xe2\x80\x94 this element is used as a parent by another element"
            : QString());

    idLineEdit->setText(QString::fromStdString(el->id));
    zOrderSpinBox->setValue(el->zOrder);
    opacitySpinBox->setValue(static_cast<double>(el->opacity) * 100.0);
    transformEditor->load(el->bounds, el->rotation);
    animateIn->load(el->inAnimation);
    animateOut->load(el->outAnimation);
    populateRefCombos();

    // Fit to Children — show only when this element is used as parent
    bool hasChildren = false;
    for (const auto& other : g->elements)
        if (other.parent == el) {
            hasChildren = true;
            break;
        }
    m_fitChildrenBox->setVisible(hasChildren);
    m_fitChildrenCheck->setChecked(el->fitToChildren);
    m_paddingEditor->setEnabled(el->fitToChildren);
    m_paddingEditor->setValues(el->childrenPadding[0], el->childrenPadding[1],
                               el->childrenPadding[2], el->childrenPadding[3]);

    m_updating = false;
}

void ElementProperties::populateRefCombos()
{
    Graphic& g = m_doc->scene().GetById(m_graphicId);
    Element& el = g.GetById(m_elementId);

    maskComboBox->blockSignals(true);
    maskComboBox->clear();
    maskComboBox->addItem("None", QString());
    int maskIdx = 0;
    for (const auto& other : g.elements) {
        if (other.id == m_elementId)
            continue;
        maskComboBox->addItem(QString::fromStdString(other.id), QString::fromStdString(other.id));
        if (el.mask && el.mask->id == other.id)
            maskIdx = maskComboBox->count() - 1;
    }
    maskComboBox->setCurrentIndex(maskIdx);
    maskComboBox->blockSignals(false);

    parentComboBox->blockSignals(true);
    parentComboBox->clear();
    parentComboBox->addItem("None", QString());
    int parentIdx = 0;
    for (const auto& other : g.elements) {
        if (other.id == m_elementId)
            continue;
        parentComboBox->addItem(QString::fromStdString(other.id), QString::fromStdString(other.id));
        if (el.parent && el.parent->id == other.id)
            parentIdx = parentComboBox->count() - 1;
    }
    parentComboBox->setCurrentIndex(parentIdx);
    parentComboBox->blockSignals(false);
}

void ElementProperties::onIdEditingFinished()
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    std::string newId = idLineEdit->text().toStdString();
    if (m_elementId == newId)
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<std::string>(
        m_doc, m_graphicId, m_elementId, newId, [](Element& e) -> std::string& { return e.id; },
        "id"));
    m_elementId = newId;
    emit elementIdChanged(m_graphicId, m_elementId);
}

void ElementProperties::onZOrderChanged(int value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<int>(
        m_doc, m_graphicId, m_elementId, value, [](Element& e) -> int& { return e.zOrder; },
        "zOrder", ElemMergeTag::ZOrder));
}

void ElementProperties::onOpacityChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(value / 100.0),
        [](Element& e) -> float& { return e.opacity; }, "opacity", ElemMergeTag::Opacity));
}

void ElementProperties::onXChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, value, [](Element& e) -> double& { return e.bounds.x; },
        "x", ElemMergeTag::X));
}

void ElementProperties::onYChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, value, [](Element& e) -> double& { return e.bounds.y; },
        "y", ElemMergeTag::Y));
}

void ElementProperties::onWChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, value,
        [](Element& e) -> double& { return e.bounds.width; }, "width", ElemMergeTag::W));
}

void ElementProperties::onHChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<double>(
        m_doc, m_graphicId, m_elementId, value,
        [](Element& e) -> double& { return e.bounds.height; }, "height", ElemMergeTag::H));
}

void ElementProperties::onRotChanged(double value)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementFieldCmd<float>(
        m_doc, m_graphicId, m_elementId, static_cast<float>(value),
        [](Element& e) -> float& { return e.rotation; }, "rotation", ElemMergeTag::Rotation));
}

void ElementProperties::onAnimInChanged()
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementAnimCmd(m_doc, m_graphicId, m_elementId,
                                                   SetElementAnimCmd::Target::AnimIn,
                                                   animateIn->getAnimationDef()));
}

void ElementProperties::onAnimOutChanged()
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(new SetElementAnimCmd(m_doc, m_graphicId, m_elementId,
                                                   SetElementAnimCmd::Target::AnimOut,
                                                   animateOut->getAnimationDef()));
}

void ElementProperties::onMaskChanged(int)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    try {
        const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
        std::string maskId = maskComboBox->currentData().toString().toStdString();
        std::string parentId = el.parent ? el.parent->id : "";
        m_doc->setElementRef(m_graphicId, m_elementId, maskId, parentId);
    } catch (const std::runtime_error&) {
    }
}

void ElementProperties::onParentChanged(int)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    try {
        const Element& el = m_doc->scene().GetById(m_graphicId).GetById(m_elementId);
        std::string maskId = el.mask ? el.mask->id : "";
        std::string newParentId = parentComboBox->currentData().toString().toStdString();
        std::string oldParentId = el.parent ? el.parent->id : "";

        if (newParentId == oldParentId)
            return;

        // Capture the element's screen position before changing the parent
        Point globalPos = el.GetGlobalPosition();

        m_doc->setElementRef(m_graphicId, m_elementId, maskId, newParentId);

        // Recompute bounds to keep the element at the same on-screen position
        double newX = globalPos.x;
        double newY = globalPos.y;
        if (!newParentId.empty()) {
            try {
                Point parentGlobal =
                    m_doc->scene().GetById(m_graphicId).GetById(newParentId).GetGlobalPosition();
                newX = globalPos.x - parentGlobal.x;
                newY = globalPos.y - parentGlobal.y;
            } catch (const std::runtime_error&) {
            }
        }

        const std::string gi = m_graphicId, ei = m_elementId;
        m_doc->applyMutation([gi, ei, newX, newY](Scene& s) {
            Element& e = s.GetById(gi).GetById(ei);
            e.bounds.x = newX;
            e.bounds.y = newY;
        });
    } catch (const std::runtime_error&) {
    }
}

void ElementProperties::onFitToChildrenToggled(bool checked)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_paddingEditor->setEnabled(checked);
    m_doc->undoStack()->push(new SetElementFieldCmd<bool>(
        m_doc, m_graphicId, m_elementId, checked,
        [](Element& e) -> bool& { return e.fitToChildren; }, "fitToChildren"));
}

void ElementProperties::onChildrenPaddingChanged(float top, float right, float bottom, float left)
{
    if (m_updating || m_graphicId.empty() || m_elementId.empty())
        return;
    m_doc->undoStack()->push(
        new SetChildrenPaddingCmd(m_doc, m_graphicId, m_elementId, top, right, bottom, left));
}
