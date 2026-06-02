#pragma once

#include <QPushButton>

class ColorPickerButton : public QPushButton {
    Q_OBJECT
public:
    explicit ColorPickerButton(QWidget* parent = nullptr);

    void setColor(double r, double g, double b, double a);
    void getColor(double& r, double& g, double& b, double& a) const;

signals:
    void colorChanged(double r, double g, double b, double a);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onClicked();

private:
    double m_r{0.0}, m_g{0.0}, m_b{0.0}, m_a{1.0};
};
