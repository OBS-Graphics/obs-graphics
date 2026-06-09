#include "HueSlider.h"
#include <QPainter>
#include <QStyleOptionSlider>

HueSlider::HueSlider(QWidget* parent) : QSlider(Qt::Horizontal, parent)
{
    setRange(0, 360);
}

void HueSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
    const int handleRadius = 5;

    const bool horiz = (orientation() == Qt::Horizontal);
    if (horiz)
        groove.adjust(handleRadius + 2, 0, -(handleRadius + 3), 0);
    else
        groove.adjust(0, handleRadius + 2, 0, -(handleRadius + 3));

    QLinearGradient grad = horiz ? QLinearGradient(groove.left(), 0, groove.right(), 0)
                                 : QLinearGradient(0, groove.bottom(), 0, groove.top());
    for (int h = 0; h <= 360; h += 6)
        grad.setColorAt(h / 360.0, QColor::fromHsv(h % 360, 255, 255));

    p.fillRect(groove, grad);
    p.setPen(palette().dark().color());
    p.drawRect(groove.adjusted(0, 0, -1, -1));

    p.save();
    p.setRenderHint(QPainter::RenderHint::Antialiasing);

    p.save();
    p.setBrush(QBrush(QColor::fromHsv(value() % 360, 255, 255)));
    p.drawEllipse(handle.center(), handleRadius, handleRadius);
    p.restore();

    p.setPen(QPen(QColor(0, 0, 0, 80), 5));
    p.drawEllipse(handle.center(), handleRadius, handleRadius);
    p.setPen(QPen(Qt::white, 2));
    p.drawEllipse(handle.center(), handleRadius, handleRadius);
    p.restore();
}
