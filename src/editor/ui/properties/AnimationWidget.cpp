#include "AnimationWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

AnimationWidget::AnimationWidget(const QString& title, QWidget* parent)
    : QGroupBox(title, parent)
{
    auto* form = new QFormLayout(this);

    // Animation Type combo
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("None",       QVariant::fromValue(static_cast<int>(AnimationType::None)));
    m_typeCombo->addItem("Fade",       QVariant::fromValue(static_cast<int>(AnimationType::Fade)));
    m_typeCombo->addItem("Scale In",   QVariant::fromValue(static_cast<int>(AnimationType::ScaleIn)));
    m_typeCombo->addItem("Slide Up",   QVariant::fromValue(static_cast<int>(AnimationType::SlideUp)));
    m_typeCombo->addItem("Slide Down", QVariant::fromValue(static_cast<int>(AnimationType::SlideDown)));
    m_typeCombo->addItem("Slide Left", QVariant::fromValue(static_cast<int>(AnimationType::SlideLeft)));
    m_typeCombo->addItem("Slide Right",QVariant::fromValue(static_cast<int>(AnimationType::SlideRight)));
    m_typeCombo->addItem("Wipe Up",    QVariant::fromValue(static_cast<int>(AnimationType::WipeUp)));
    m_typeCombo->addItem("Wipe Down",  QVariant::fromValue(static_cast<int>(AnimationType::WipeDown)));
    m_typeCombo->addItem("Wipe Left",  QVariant::fromValue(static_cast<int>(AnimationType::WipeLeft)));
    m_typeCombo->addItem("Wipe Right", QVariant::fromValue(static_cast<int>(AnimationType::WipeRight)));
    form->addRow("Type:", m_typeCombo);

    // Easing combo
    m_easingCombo = new QComboBox(this);
    m_easingCombo->addItem("Linear",      QVariant::fromValue(static_cast<int>(Easing::Linear)));
    m_easingCombo->addItem("Ease In",     QVariant::fromValue(static_cast<int>(Easing::EaseIn)));
    m_easingCombo->addItem("Ease Out",    QVariant::fromValue(static_cast<int>(Easing::EaseOut)));
    m_easingCombo->addItem("Ease In Out", QVariant::fromValue(static_cast<int>(Easing::EaseInOut)));
    form->addRow("Easing:", m_easingCombo);

    // Duration spinbox
    m_duration = new QDoubleSpinBox(this);
    m_duration->setRange(0.0, 30.0);
    m_duration->setDecimals(2);
    m_duration->setSingleStep(0.05);
    m_duration->setSuffix(" s");
    form->addRow("Duration:", m_duration);

    // Delay spinbox
    m_delay = new QDoubleSpinBox(this);
    m_delay->setRange(0.0, 30.0);
    m_delay->setDecimals(2);
    m_delay->setSingleStep(0.05);
    m_delay->setSuffix(" s");
    form->addRow("Delay:", m_delay);

    connect(m_typeCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnimationWidget::onChanged);
    connect(m_easingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnimationWidget::onChanged);
    connect(m_duration, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnimationWidget::onChanged);
    connect(m_delay, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnimationWidget::onChanged);
}

void AnimationWidget::setAnimationDef(const AnimationDef& def)
{
    m_updating = true;

    // Set type combo
    int typeVal = static_cast<int>(def.type);
    for (int i = 0; i < m_typeCombo->count(); ++i) {
        if (m_typeCombo->itemData(i).toInt() == typeVal) {
            m_typeCombo->setCurrentIndex(i);
            break;
        }
    }

    // Set easing combo
    int easingVal = static_cast<int>(def.easing);
    for (int i = 0; i < m_easingCombo->count(); ++i) {
        if (m_easingCombo->itemData(i).toInt() == easingVal) {
            m_easingCombo->setCurrentIndex(i);
            break;
        }
    }

    m_duration->setValue(static_cast<double>(def.duration));
    m_delay->setValue(static_cast<double>(def.delay));

    m_updating = false;
}

AnimationDef AnimationWidget::getAnimationDef() const
{
    AnimationDef def;
    def.type     = static_cast<AnimationType>(m_typeCombo->currentData().toInt());
    def.easing   = static_cast<Easing>(m_easingCombo->currentData().toInt());
    def.duration = static_cast<float>(m_duration->value());
    def.delay    = static_cast<float>(m_delay->value());
    return def;
}

void AnimationWidget::onChanged()
{
    if (m_updating) return;
    emit animationDefChanged(getAnimationDef());
}
