#ifndef ANIMATIONEDITOR_H
#define ANIMATIONEDITOR_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QWidget>

struct AnimationDef;

class AnimationEditor : public QWidget
{
    Q_OBJECT

public:
    explicit AnimationEditor(QWidget *parent = nullptr);

    void load(const AnimationDef& anim);
    AnimationDef getAnimationDef() const;

signals:
    void animationChanged();

private slots:
    void onTypeChanged(int index);
    void onEasingChanged(int index);
    void onDurationChanged(double value);
    void onDelayChanged(double value);

private:
    QComboBox *typeComboBox, *easingComboBox;
    QDoubleSpinBox *durationDoubleSpinBox, *delayDoubleSpinBox;
};

#endif // ANIMATIONEDITOR_H
