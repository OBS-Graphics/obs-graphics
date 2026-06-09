#include "ColorPicker.h"
#include <QGridLayout>
#include <QScreen>

ColorPicker::ColorPicker(QWidget* parent) : QWidget{parent}
{
    QGridLayout* layout = new QGridLayout(this);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 0);

    m_colorWheel = new ColorWheel();
    layout->addWidget(m_colorWheel, 0, 0);

    m_alphaSlider = new GradientSlider();
    m_alphaSlider->setMinimum(0);
    m_alphaSlider->setMaximum(1000);
    m_alphaSlider->setValue(1000);
    m_alphaSlider->setOrientation(Qt::Vertical);
    layout->addWidget(m_alphaSlider, 0, 1);

    m_colorEdit = new ColorLineEdit();
    layout->addWidget(m_colorEdit, 1, 0, 1, 2);

    connect(m_colorWheel, &ColorWheel::colorChanged, this, &ColorPicker::onColorWheelChanged);
    connect(m_alphaSlider, &GradientSlider::valueChanged, this, &ColorPicker::onAlphaSliderChanged);
    connect(m_colorEdit, &ColorLineEdit::colorChanged, this, &ColorPicker::onColorEditChanged);

    onColorWheelChanged(m_colorWheel->color());
    onAlphaSliderChanged(1000);
    onColorEditChanged(m_colorWheel->color());
}

QColor ColorPicker::color() const
{
    return computeColor();
}

void ColorPicker::setColor(const QColor& color)
{
    onColorEditChanged(color);
}

void ColorPicker::showPopup(QWidget* at)
{
    auto* popup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);

    auto* picker = new ColorPicker(popup);
    picker->setGeometry(0, 0, 120, 120);

    // Calculate popup position
    QPoint pos = at->mapToGlobal(QPoint(0, at->height()));

    const QSize popupSize = popup->sizeHint();
    const QRect screen = at->screen()->availableGeometry();

    // Flip horizontally if it bleeds off the right edge
    if (pos.x() + popupSize.width() > screen.right())
        pos.setX(at->mapToGlobal(QPoint(at->width(), at->height())).x() - popupSize.width());

    // Flip vertically if it bleeds off the bottom edge
    if (pos.y() + popupSize.height() > screen.bottom())
        pos.setY(at->mapToGlobal(QPoint(0, 0)).y() - popupSize.height());

    // Clamp to screen edges as a last resort
    pos.setX(qBound(screen.left(), pos.x(), screen.right() - popupSize.width()));
    pos.setY(qBound(screen.top(), pos.y(), screen.bottom() - popupSize.height()));

    popup->move(pos);
    popup->show();
}

void ColorPicker::onColorWheelChanged(const QColor& color)
{
    m_alphaSlider->setStops({
        {0.0, QColor::fromRgb(color.red(), color.green(), color.blue(), 0)},
        {1.0, color},
    });
    emit m_alphaSlider->valueChanged(m_alphaSlider->value());
    emit colorChanged(computeColor());
}

void ColorPicker::onAlphaSliderChanged(int value)
{
    qreal alpha = (qreal)value / 1000;
    QColor col = m_colorWheel->color();
    col.setAlphaF(alpha);
    m_colorEdit->setColor(col);
    emit colorChanged(computeColor());
}

void ColorPicker::onColorEditChanged(const QColor& color)
{
    QColor colorNoAlpha = color;
    colorNoAlpha.setAlphaF(1.0f);

    m_colorWheel->setColor(colorNoAlpha);
    m_alphaSlider->setValue((int)(color.alphaF() * 1000.0f));

    emit colorChanged(computeColor());
}

QColor ColorPicker::computeColor() const
{
    QColor color = m_colorWheel->color();
    qreal alpha = (qreal)m_alphaSlider->value() / 1000;
    color.setAlphaF(alpha);
    return color;
}
