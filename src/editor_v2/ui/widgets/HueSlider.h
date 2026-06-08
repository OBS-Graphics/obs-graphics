#pragma once
#include <QSlider>

class HueSlider : public QSlider {
    Q_OBJECT
public:
    explicit HueSlider(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
};
