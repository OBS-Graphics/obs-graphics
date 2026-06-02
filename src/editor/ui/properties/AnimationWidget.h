#pragma once

#include <QGroupBox>
#include "engine/animation.h"

class QComboBox;
class QDoubleSpinBox;

class AnimationWidget : public QGroupBox {
    Q_OBJECT
public:
    explicit AnimationWidget(const QString& title, QWidget* parent = nullptr);

    void setAnimationDef(const AnimationDef& def);
    AnimationDef getAnimationDef() const;

signals:
    void animationDefChanged(AnimationDef def);

private slots:
    void onChanged();

private:
    QComboBox*      m_typeCombo;
    QComboBox*      m_easingCombo;
    QDoubleSpinBox* m_duration;
    QDoubleSpinBox* m_delay;
    bool            m_updating{false};
};
