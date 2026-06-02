#pragma once

#include <QWidget>

class QDoubleSpinBox;
class QCheckBox;

class CornerRadiusWidget : public QWidget {
    Q_OBJECT
public:
    explicit CornerRadiusWidget(QWidget* parent = nullptr);

    void setValues(float tl, float tr, float br, float bl);
    void getValues(float& tl, float& tr, float& br, float& bl) const;

signals:
    void valuesChanged(float tl, float tr, float br, float bl);

private slots:
    void onSpinChanged(double value);
    void onLinkToggled(bool checked);

private:
    QDoubleSpinBox* m_tl;
    QDoubleSpinBox* m_tr;
    QDoubleSpinBox* m_br;
    QDoubleSpinBox* m_bl;
    QCheckBox*      m_link;
    bool            m_updating{false};
};
