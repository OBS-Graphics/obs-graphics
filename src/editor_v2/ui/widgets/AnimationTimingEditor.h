#pragma once

#include "engine/animation.h"
#include <QWidget>

class AnimationTimingEditor : public QWidget {
    Q_OBJECT
public:
    explicit AnimationTimingEditor(QWidget* parent = nullptr);

    void load(const AnimationDef& def);
    void setMaxDuration(float maxDuration);

signals:
    void animationChanged(AnimationType type, Easing easing, float delay, float duration);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    enum class DragZone { None, LeftHandle, Body, RightHandle };

    float m_maxDuration{1.0f};
    AnimationType m_type{AnimationType::None};
    Easing m_easing{Easing::Linear};
    float m_duration{0.5f};
    float m_delay{0.0f};

    DragZone m_dragZone{DragZone::None};
    int m_dragStartX{0};
    float m_dragStartDelay{0.0f};
    float m_dragStartDuration{0.0f};

    QRect buttonRect() const;
    DragZone hitTest(const QPoint& pos) const;
    void updateCursor(const QPoint& pos);
};
