#pragma once

#include <QWidget>
#include "engine/types.hpp"

class QComboBox;
class QStackedWidget;
class QDoubleSpinBox;
class ColorPickerButton;
class GradientStopListWidget;

class PaintEditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit PaintEditorWidget(QWidget* parent = nullptr);

    void setPaint(const Paint& paint);
    Paint getPaint() const;

signals:
    void paintChanged(Paint paint);

private slots:
    void onTypeChanged(int index);
    void onSolidColorChanged(double r, double g, double b, double a);
    void onLinearParamChanged();
    void onRadialParamChanged();
    void onLinearStopsChanged(std::vector<Paint::Stop> stops);
    void onRadialStopsChanged(std::vector<Paint::Stop> stops);

private:
    QComboBox*              m_typeCombo;
    QStackedWidget*         m_stack;

    // Page 1: Solid
    ColorPickerButton*      m_solidColor;

    // Page 2: Linear
    QDoubleSpinBox*         m_linX0, *m_linY0, *m_linX1, *m_linY1;
    GradientStopListWidget* m_linearStops;

    // Page 3: Radial
    QDoubleSpinBox*         m_radCx0, *m_radCy0, *m_radR0;
    QDoubleSpinBox*         m_radCx1, *m_radCy1, *m_radR1;
    GradientStopListWidget* m_radialStops;

    bool m_updating{false};

    QDoubleSpinBox* makeCoordSpinBox();
    void emitPaint();
};
