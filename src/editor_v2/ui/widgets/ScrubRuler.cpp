#include "ScrubRuler.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>

ScrubRuler::ScrubRuler(QWidget* parent) : QWidget{parent}
{
    setFixedHeight(20);
    setCursor(Qt::SizeHorCursor);
}

void ScrubRuler::setMaxDuration(float maxDuration)
{
    m_maxDuration = qMax(maxDuration, 0.01f);
    update();
}

void ScrubRuler::setScrubTime(float t)
{
    m_scrubTime = qBound(0.0f, t, m_maxDuration);
    update();
}

float ScrubRuler::timeAtX(int x) const
{
    return qBound(0.0f, float(x) / float(qMax(width(), 1)) * m_maxDuration, m_maxDuration);
}

void ScrubRuler::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const int w = width();
    const int h = height();

    p.fillRect(rect(), QColor(22, 22, 22));

    // --- ticks ---
    // Choose a minor step so major ticks (every 5th) land ~50px apart.
    static const float kSteps[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f};
    const float pps = float(w) / m_maxDuration;
    float minorStep = 10.0f;
    for (float s : kSteps) {
        if (pps * s * 5.0f >= 50.0f) { minorStep = s; break; }
    }

    const int nSteps = int(m_maxDuration / minorStep) + 2;
    for (int k = 0; k <= nSteps; ++k) {
        const float t = k * minorStep;
        if (t > m_maxDuration + 1e-4f) break;
        const int x = int(t / m_maxDuration * w);
        const bool isMajor = (k % 5 == 0);

        if (isMajor) {
            p.setPen(QColor(130, 130, 130));
            p.drawLine(x, h - 9, x, h);
            QFont f = p.font();
            f.setPointSize(7);
            p.setFont(f);
            p.drawText(QRect(x + 2, 0, 40, h - 7),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString("%1s").arg(double(t), 0, 'f', t < 1.0f ? 2 : 1));
        } else {
            p.setPen(QColor(65, 65, 65));
            p.drawLine(x, h - 4, x, h);
        }
    }

    // --- playhead ---
    const int cx = int(m_scrubTime / m_maxDuration * w);
    p.setPen(QPen(QColor(255, 200, 50), 2.5));
    p.drawLine(cx, 0, cx, h);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 200, 50));
    const QPoint tri[3] = { {cx - 6, 0}, {cx + 6, 0}, {cx, 10} };
    p.drawPolygon(tri, 3);
}

void ScrubRuler::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    m_scrubTime = timeAtX(event->pos().x());
    update();
    emit scrubChanged(m_scrubTime);
    event->accept();
}

void ScrubRuler::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) return;
    m_scrubTime = timeAtX(event->pos().x());
    update();
    emit scrubChanged(m_scrubTime);
    event->accept();
}
