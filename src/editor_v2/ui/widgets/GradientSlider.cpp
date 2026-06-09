#include "GradientSlider.h"
#include "ColorUtils.h"
#include <QPainter>

GradientSlider::GradientSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent)
{
    setMinimumWidth(24);
    setMinimumHeight(24);
    setStops({{0.0, Qt::transparent}, {1.0, Qt::black}});
}

void GradientSlider::setStops(const QGradientStops& stops)
{
    m_stops = stops;
    update();
}

void GradientSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const int kHandleR = 8;
    const int hPad = kHandleR + 2;

    const bool horiz = (orientation() == Qt::Horizontal);
    QRect groove = horiz ? QRect(hPad, 1, width() - 2 * hPad, height() - 2)
                         : QRect(1, hPad, width() - 2, height() - 2 * hPad);

    // Checkerboard for transparency indication
    p.save();
    p.setClipRegion(groove);
    const int cell = 4;
    for (int row = 0; row * cell < groove.height(); ++row) {
        for (int col = 0; col * cell < groove.width(); ++col) {
            QColor c = ((row + col) % 2 == 0) ? QColor(180, 180, 180) : Qt::white;
            p.fillRect(groove.x() + col * cell, groove.y() + row * cell, cell, cell, c);
        }
    }
    p.restore();

    QLinearGradient grad = horiz ? QLinearGradient(groove.topLeft(), groove.topRight())
                                 : QLinearGradient(groove.bottomLeft(), groove.topLeft());
    for (const auto& stop : m_stops)
        grad.setColorAt(stop.first, stop.second);
    p.fillRect(groove, grad);

    p.setPen(palette().dark().color());
    p.drawRect(groove);

    qreal t = (qreal)(value() - minimum()) / (maximum() - minimum());
    QPoint handleCenter =
        horiz ? QPoint(groove.left() + qRound(t * groove.width()), groove.center().y() + 2)
              : QPoint(groove.center().x() + 2, groove.bottom() - qRound(t * groove.height()));

    p.save();
    p.setRenderHint(QPainter::RenderHint::Antialiasing);

    p.save();
    p.setBrush(QBrush(sampleGradient(m_stops, t)));
    p.drawEllipse(handleCenter, kHandleR, kHandleR);
    p.restore();

    p.setPen(QPen(QColor(0, 0, 0, 80), 5));
    p.drawEllipse(handleCenter, kHandleR, kHandleR);
    p.setPen(QPen(Qt::white, 2));
    p.drawEllipse(handleCenter, kHandleR, kHandleR);
    p.restore();
}
