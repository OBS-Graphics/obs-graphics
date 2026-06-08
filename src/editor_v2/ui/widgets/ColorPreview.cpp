#include "ColorPreview.h"
#include <QPainter>
#include <QMouseEvent>

ColorPreview::ColorPreview(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(40, 20);
}

QColor ColorPreview::color() const { return m_color; }

void ColorPreview::setColor(const QColor& c)
{
    m_color = c;
    update();
}

void ColorPreview::setClickable(bool clickable)
{
    m_clickable = clickable;
    setCursor(clickable ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void ColorPreview::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QRect r = rect();
    int half = r.width() / 2;
    QRect leftHalf(r.left(), r.top(), half, r.height());
    QRect rightHalf(r.left() + half, r.top(), r.width() - half, r.height());

    // Left: solid color (fully opaque, alpha stripped)
    QColor solid = m_color;
    solid.setAlpha(255);
    p.fillRect(leftHalf, solid);

    // Right: checkerboard + color with alpha
    int cell = 5;
    for (int row = 0; row * cell < rightHalf.height(); ++row) {
        for (int col = 0; col * cell < rightHalf.width(); ++col) {
            QColor c = ((row + col) % 2 == 0) ? QColor(180, 180, 180) : Qt::white;
            p.fillRect(rightHalf.x() + col * cell, rightHalf.y() + row * cell, cell, cell, c);
        }
    }
    p.fillRect(rightHalf, m_color);

    // Border and divider
    p.setPen(palette().dark().color());
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.drawLine(leftHalf.right(), r.top(), leftHalf.right(), r.bottom());
}

void ColorPreview::mousePressEvent(QMouseEvent* e)
{
    if (m_clickable && e->button() == Qt::LeftButton)
        emit clicked();
}
