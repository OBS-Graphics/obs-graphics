#ifndef TRANSFORMEDITOR_H
#define TRANSFORMEDITOR_H

#include <QDoubleSpinBox>
#include <QWidget>

#include "engine/types.hpp"

class TransformEditor : public QWidget
{
    Q_OBJECT

public:
    explicit TransformEditor(QWidget *parent = nullptr);

    void load(const Rectangle& bounds, float rotation);

signals:
    void xChanged(double);
    void yChanged(double);
    void wChanged(double);
    void hChanged(double);
    void rotChanged(double);

private slots:
    void onSpinXChanged(double value);
    void onSpinYChanged(double value);
    void onSpinWChanged(double value);
    void onSpinHChanged(double value);
    void onSpinRotChanged(double value);

private:
    QDoubleSpinBox *spinX, *spinY, *spinW, *spinH, *spinRot;
};

#endif // TRANSFORMEDITOR_H
