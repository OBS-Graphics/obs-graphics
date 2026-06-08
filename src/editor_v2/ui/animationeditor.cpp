#include "animationeditor.h"
#include "engine/animation.h"
#include <QGridLayout>
#include <QLabel>

AnimationEditor::AnimationEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QGridLayout(this);
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 1);
    layout->setColumnStretch(3, 1);

    auto *typeLabel = new QLabel("Type");
    typeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    typeComboBox = new QComboBox;
    typeComboBox->addItems({"None", "Fade", "Scale In",
                            "Slide Up", "Slide Down", "Slide Left", "Slide Right",
                            "Wipe Up", "Wipe Down", "Wipe Left", "Wipe Right"});
    layout->addWidget(typeLabel, 0, 0);
    layout->addWidget(typeComboBox, 0, 1, 1, 3);

    auto *easingLabel = new QLabel("Easing");
    easingLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    easingComboBox = new QComboBox;
    easingComboBox->addItems({"Linear", "Ease In", "Ease Out", "Ease In Out"});
    layout->addWidget(easingLabel, 1, 0);
    layout->addWidget(easingComboBox, 1, 1, 1, 3);

    auto *durationLabel = new QLabel("Duration");
    durationLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    durationDoubleSpinBox = new QDoubleSpinBox;
    durationDoubleSpinBox->setMaximum(100.0);
    durationDoubleSpinBox->setSingleStep(0.05);
    durationDoubleSpinBox->setSuffix(" s");
    durationDoubleSpinBox->setValue(0.5);

    auto *delayLabel = new QLabel("Delay");
    delayLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    delayDoubleSpinBox = new QDoubleSpinBox;
    delayDoubleSpinBox->setMaximum(100.0);
    delayDoubleSpinBox->setSingleStep(0.05);
    delayDoubleSpinBox->setSuffix(" s");

    layout->addWidget(durationLabel, 2, 0);
    layout->addWidget(durationDoubleSpinBox, 2, 1);
    layout->addWidget(delayLabel, 2, 2);
    layout->addWidget(delayDoubleSpinBox, 2, 3);

    connect(typeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnimationEditor::onTypeChanged);
    connect(easingComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnimationEditor::onEasingChanged);
    connect(durationDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnimationEditor::onDurationChanged);
    connect(delayDoubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnimationEditor::onDelayChanged);
}

void AnimationEditor::load(const AnimationDef& anim)
{
    typeComboBox->blockSignals(true);
    easingComboBox->blockSignals(true);
    durationDoubleSpinBox->blockSignals(true);
    delayDoubleSpinBox->blockSignals(true);

    typeComboBox->setCurrentIndex(static_cast<int>(anim.type));
    easingComboBox->setCurrentIndex(static_cast<int>(anim.easing));
    durationDoubleSpinBox->setValue(static_cast<double>(anim.duration));
    delayDoubleSpinBox->setValue(static_cast<double>(anim.delay));

    typeComboBox->blockSignals(false);
    easingComboBox->blockSignals(false);
    durationDoubleSpinBox->blockSignals(false);
    delayDoubleSpinBox->blockSignals(false);
}

AnimationDef AnimationEditor::getAnimationDef() const
{
    AnimationDef anim;
    anim.type     = static_cast<AnimationType>(typeComboBox->currentIndex());
    anim.easing   = static_cast<Easing>(easingComboBox->currentIndex());
    anim.duration = static_cast<float>(durationDoubleSpinBox->value());
    anim.delay    = static_cast<float>(delayDoubleSpinBox->value());
    return anim;
}

void AnimationEditor::onTypeChanged(int)    { emit animationChanged(); }
void AnimationEditor::onEasingChanged(int)  { emit animationChanged(); }
void AnimationEditor::onDurationChanged(double) { emit animationChanged(); }
void AnimationEditor::onDelayChanged(double)    { emit animationChanged(); }
