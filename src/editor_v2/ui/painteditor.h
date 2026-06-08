#ifndef PAINTEDITOR_H
#define PAINTEDITOR_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QWidget>

#include "engine/types.hpp"
#include "ui/adaptivestack.h"
#include "ui/widgets/ColorPicker.h"
#include "ui/widgets/LinearGradientEditor.h"
#include "ui/widgets/RadialGradientEditor.h"

class PaintEditor : public QWidget
{
    Q_OBJECT

public:
    explicit PaintEditor(QWidget *parent = nullptr);

    Paint getPaint() const;
    void setPaint(const Paint& paint);

signals:
    void paintChanged(Paint paint);

private slots:
    void onTypeChanged(int index);
    void onColorChanged(const QColor& color);
    void onStopPosChanged(double value);
    void onStopsChanged(const QVector<GradientStop>& stops);
    void onStopSelected(int index);

private:
    QComboBox *typeComboBox;
    AdaptiveStack *mainSelector;

    // solid page
    ColorPicker* m_cpSolid;

    // linear page
    ColorPicker* m_cpLinear;
    QDoubleSpinBox* m_lnStopPosition;
    LinearGradientEditor* m_linearEditor;

    // radial page
    ColorPicker* m_cpRadial;
    QDoubleSpinBox* m_radStopPosition;
    RadialGradientEditor* m_radialEditor;

    bool m_updating{false}; // prevent recursion

    void emitPaint();
    bool filterScroll(QObject *obj, QEvent *event);
};

#endif // PAINTEDITOR_H
