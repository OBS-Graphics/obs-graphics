#ifndef COLORPICKER_H
#define COLORPICKER_H

#include "ui/widgets/ColorLineEdit.h"
#include "ui/widgets/ColorWheel.h"
#include "ui/widgets/GradientSlider.h"
#include <QWidget>

class ColorPicker : public QWidget
{
    Q_OBJECT
public:
    explicit ColorPicker(QWidget *parent = nullptr);

    QColor color() const;
    void setColor(const QColor& color);

    static void showPopup(QWidget* at);

signals:
    void colorChanged(const QColor& color);

private slots:
    void onColorWheelChanged(const QColor& color);
    void onAlphaSliderChanged(int value);
    void onColorEditChanged(const QColor& color);

private:
    ColorWheel* m_colorWheel;
    GradientSlider* m_alphaSlider;
    ColorLineEdit* m_colorEdit;

    QColor computeColor() const;
};

#endif // COLORPICKER_H
