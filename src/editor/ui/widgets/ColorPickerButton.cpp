#include "ColorPickerButton.h"

#include <QColorDialog>
#include <QColor>
#include <QPainter>
#include <QPaintEvent>

ColorPickerButton::ColorPickerButton(QWidget* parent)
    : QPushButton(parent)
{
    setFixedHeight(28);
    connect(this, &QPushButton::clicked, this, &ColorPickerButton::onClicked);
}

void ColorPickerButton::setColor(double r, double g, double b, double a)
{
    m_r = r;
    m_g = g;
    m_b = b;
    m_a = a;
    update();
}

void ColorPickerButton::getColor(double& r, double& g, double& b, double& a) const
{
    r = m_r;
    g = m_g;
    b = m_b;
    a = m_a;
}

void ColorPickerButton::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(2, 2, -2, -2);

    // Draw checkerboard background for transparency indication
    int cellSize = 6;
    for (int row = 0; row * cellSize < r.height(); ++row) {
        for (int col = 0; col * cellSize < r.width(); ++col) {
            QColor c = ((row + col) % 2 == 0) ? Qt::lightGray : Qt::white;
            p.fillRect(r.x() + col * cellSize, r.y() + row * cellSize,
                       cellSize, cellSize, c);
        }
    }

    // Draw color swatch
    QColor color(
        static_cast<int>(m_r * 255.0 + 0.5),
        static_cast<int>(m_g * 255.0 + 0.5),
        static_cast<int>(m_b * 255.0 + 0.5),
        static_cast<int>(m_a * 255.0 + 0.5)
    );
    p.fillRect(r, color);

    // Draw border
    p.setPen(palette().dark().color());
    p.drawRect(r);

    // Draw "..." text indicator
    p.setPen(m_a > 0.5 ? Qt::white : Qt::black);
    p.drawText(rect(), Qt::AlignCenter, "...");
}

void ColorPickerButton::onClicked()
{
    QColor initial(
        static_cast<int>(m_r * 255.0 + 0.5),
        static_cast<int>(m_g * 255.0 + 0.5),
        static_cast<int>(m_b * 255.0 + 0.5),
        static_cast<int>(m_a * 255.0 + 0.5)
    );

    QColor chosen = QColorDialog::getColor(
        initial, this, "Pick Color",
        QColorDialog::ShowAlphaChannel
    );

    if (chosen.isValid()) {
        m_r = chosen.red()   / 255.0;
        m_g = chosen.green() / 255.0;
        m_b = chosen.blue()  / 255.0;
        m_a = chosen.alpha() / 255.0;
        update();
        emit colorChanged(m_r, m_g, m_b, m_a);
    }
}
