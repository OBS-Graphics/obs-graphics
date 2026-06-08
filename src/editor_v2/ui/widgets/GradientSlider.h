#pragma once
#include <QSlider>

class GradientSlider : public QSlider {
    Q_OBJECT
public:
    explicit GradientSlider(QWidget* parent = nullptr);
    void setStops(const QGradientStops& stops);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QGradientStops m_stops;
};
