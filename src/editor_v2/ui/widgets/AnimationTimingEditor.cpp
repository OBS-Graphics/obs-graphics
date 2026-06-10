#include "AnimationTimingEditor.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <qdrawutil.h>
#include <cmath>

static QString animationTypeName(AnimationType t)
{
    switch (t) {
    case AnimationType::None:       return "None";
    case AnimationType::Fade:       return "Fade";
    case AnimationType::SlideUp:    return "Slide Up";
    case AnimationType::SlideDown:  return "Slide Down";
    case AnimationType::SlideLeft:  return "Slide Left";
    case AnimationType::SlideRight: return "Slide Right";
    case AnimationType::ScaleIn:    return "Scale In";
    case AnimationType::WipeUp:     return "Wipe Up";
    case AnimationType::WipeDown:   return "Wipe Down";
    case AnimationType::WipeLeft:   return "Wipe Left";
    case AnimationType::WipeRight:  return "Wipe Right";
    }
    return {};
}

static constexpr int kHandleZoneWidth = 14;

static void qUtilsDrawButton(QPainter    *painter,
                           const QRect   &rect,
                           const QColor  &base,
                           bool           sunken,
                           const QString &text,
                           int            radius)
{
    const QColor textColor = base.darker(300);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QRect r = rect.adjusted(1, 1, -1, -1);

    // --- outer ring ---
    painter->setPen(QPen(base.darker(160), 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(r, radius + 1, radius + 1);

    // --- gradient fill ---
    QLinearGradient grad(r.topLeft(), r.bottomLeft());
    if (sunken) {
        grad.setColorAt(0.0, base.darker(115));
        grad.setColorAt(1.0, base.darker(105));
    } else {
        grad.setColorAt(0.0, base.lighter(120));
        grad.setColorAt(1.0, base.darker(115));
    }

    painter->setPen(QPen(base.darker(140), 1.0));
    painter->setBrush(grad);
    painter->drawRoundedRect(r, radius, radius);

    // --- text ---
    QRect textRect = r.adjusted(16, 2, -16, -2);
    if (!text.isEmpty()) {
        if (sunken) textRect.translate(0, 1);

        painter->save();
        painter->setClipRect(textRect);

        QFont font = painter->font();
        font.setBold(true);
        painter->setFont(font);

        QFontMetrics fm(font);
        QString elided = fm.elidedText(text, Qt::ElideRight, textRect.width());

        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft, elided);

        painter->restore();
    }

    // --- handles ---
    int lx = rect.left();
    int rx = textRect.right() + 16;
    painter->setPen(textColor);
    painter->drawLine(lx + 5, textRect.y()+1, lx + 5, textRect.bottom()-2);
    painter->drawLine(lx + 8, textRect.y()+1, lx + 8, textRect.bottom()-2);
    painter->drawLine(rx - 5, textRect.y()+1, rx - 5, textRect.bottom()-2);
    painter->drawLine(rx - 8, textRect.y()+1, rx - 8, textRect.bottom()-2);

    painter->restore();
}

AnimationTimingEditor::AnimationTimingEditor(QWidget* parent) : QWidget{parent} {
    setMinimumSize(50, 24);
    setMaximumHeight(24);
    setMouseTracking(true);
}

void AnimationTimingEditor::load(const AnimationDef &def)
{
    m_duration = def.duration;
    m_delay = def.delay;
    m_type = def.type;
    m_easing = def.easing;
    m_duration = def.duration;
    m_delay = def.delay;
    update();
}

void AnimationTimingEditor::setMaxDuration(float maxDuration)
{
    m_maxDuration = qMax(maxDuration, 0.01f);
    update();
}

void AnimationTimingEditor::paintEvent(QPaintEvent *)
{
    const QColor base = (m_type == AnimationType::None) ? QColor(100, 100, 100) : QColor(241, 162, 24);
    const int radius = 3;

    QPainter p(this);
    QPalette pal = this->palette();

    p.save();
    p.setBrush(QColor(30, 30, 30));
    p.drawRect(rect().adjusted(-1, -1, 2, 2));
    p.restore();

    p.save();
    p.setPen(QColor(80, 80, 80));
    p.drawLine(rect().bottomLeft(), rect().bottomRight());
    p.restore();

    QString fmtText = (animationTypeName(m_type) + " (%1s)").arg(m_duration, 2, 'f', 2);
    setToolTip(fmtText);

    qUtilsDrawButton(&p, buttonRect(), base, false, fmtText, radius);
}

QRect AnimationTimingEditor::buttonRect() const
{
    const float w = float(geometry().width());
    int xPos  = int(w * m_delay    / m_maxDuration);
    int bw    = int(w * m_duration / m_maxDuration);
    return QRect(QPoint{xPos, 0}, QSize{bw, geometry().height()});
}

AnimationTimingEditor::DragZone AnimationTimingEditor::hitTest(const QPoint& pos) const
{
    QRect br = buttonRect();
    if (!br.contains(pos))
        return DragZone::None;
    if (pos.x() <= br.left() + kHandleZoneWidth)
        return DragZone::LeftHandle;
    if (pos.x() >= br.right() - kHandleZoneWidth)
        return DragZone::RightHandle;
    return DragZone::Body;
}

void AnimationTimingEditor::updateCursor(const QPoint& pos)
{
    if (m_type == AnimationType::None) { unsetCursor(); return; }
    switch (hitTest(pos)) {
    case DragZone::LeftHandle:
    case DragZone::RightHandle:
        setCursor(Qt::SizeHorCursor);
        break;
    case DragZone::Body:
        setCursor(Qt::SizeAllCursor);
        break;
    default:
        unsetCursor();
        break;
    }
}

void AnimationTimingEditor::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_type == AnimationType::None)
        return;
    m_dragZone = hitTest(event->pos());
    if (m_dragZone == DragZone::None)
        return;
    m_dragStartX = event->pos().x();
    m_dragStartDelay = m_delay;
    m_dragStartDuration = m_duration;
    event->accept();
}

void AnimationTimingEditor::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragZone == DragZone::None) {
        updateCursor(event->pos());
        return;
    }

    const float w = float(width());
    if (w <= 0.0f)
        return;

    const float deltaSec    = float(event->pos().x() - m_dragStartX) / w * m_maxDuration;
    const float minDuration = float(kHandleZoneWidth * 2) / w * m_maxDuration;

    auto snap = [](float v) { return std::round(v * 100.0f) / 100.0f; };

    switch (m_dragZone) {
    case DragZone::LeftHandle: {
        // Right edge stays fixed; left edge moves.
        const float maxDelay = m_dragStartDelay + m_dragStartDuration - minDuration;
        m_delay    = snap(qBound(0.0f, m_dragStartDelay + deltaSec, maxDelay));
        m_duration = (m_dragStartDelay + m_dragStartDuration) - m_delay;
        break;
    }
    case DragZone::Body: {
        // Whole button shifts; duration unchanged.
        m_delay = snap(qBound(0.0f, m_dragStartDelay + deltaSec, m_maxDuration - m_dragStartDuration));
        break;
    }
    case DragZone::RightHandle: {
        // Left edge stays fixed; right edge moves.
        m_duration = snap(qBound(minDuration, m_dragStartDuration + deltaSec, m_maxDuration - m_dragStartDelay));
        break;
    }
    default:
        break;
    }

    update();
    emit animationChanged(m_type, m_easing, m_delay, m_duration);
    event->accept();
}

void AnimationTimingEditor::contextMenuEvent(QContextMenuEvent* event)
{
    static constexpr AnimationType kAllTypes[] = {
        AnimationType::None,
        AnimationType::Fade,
        AnimationType::SlideUp,   AnimationType::SlideDown,
        AnimationType::SlideLeft, AnimationType::SlideRight,
        AnimationType::ScaleIn,
        AnimationType::WipeUp,    AnimationType::WipeDown,
        AnimationType::WipeLeft,  AnimationType::WipeRight,
    };

    static constexpr struct { Easing easing; const char* name; } kAllEasings[] = {
        { Easing::Linear,   "Linear"    },
        { Easing::EaseIn,   "Ease In"   },
        { Easing::EaseOut,  "Ease Out"  },
        { Easing::EaseInOut,"Ease In/Out"},
    };

    QMenu menu(this);

    QMenu* typeMenu = menu.addMenu("Set Animation Type");
    for (AnimationType t : kAllTypes) {
        QAction* a = typeMenu->addAction(animationTypeName(t));
        a->setCheckable(true);
        a->setChecked(t == m_type);
        connect(a, &QAction::triggered, this, [this, t] {
            m_type = t;
            update();
            emit animationChanged(m_type, m_easing, m_delay, m_duration);
        });
    }

    QMenu* easingMenu = menu.addMenu("Set Easing");
    for (const auto& e : kAllEasings) {
        QAction* a = easingMenu->addAction(e.name);
        a->setCheckable(true);
        a->setChecked(e.easing == m_easing);
        connect(a, &QAction::triggered, this, [this, e] {
            m_easing = e.easing;
            update();
            emit animationChanged(m_type, m_easing, m_delay, m_duration);
        });
    }

    menu.exec(event->globalPos());
    event->accept();
}

void AnimationTimingEditor::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_dragZone == DragZone::None)
        return;
    m_dragZone = DragZone::None;
    updateCursor(event->pos());
    event->accept();
}
