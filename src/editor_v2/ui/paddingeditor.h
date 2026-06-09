#pragma once

#include <QDoubleSpinBox>
#include <QPushButton>
#include <QWidget>

class PaddingEditor : public QWidget {
    Q_OBJECT
public:
    explicit PaddingEditor(QWidget* parent = nullptr);

    void setValues(float top, float right, float bottom, float left);
    void getValues(float& top, float& right, float& bottom, float& left) const;

signals:
    void paddingChanged(float top, float right, float bottom, float left);

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    void onSpinChange(double);
    void onLinkClick(bool linked);

private:
    QDoubleSpinBox* m_top;
    QDoubleSpinBox* m_right;
    QDoubleSpinBox* m_bottom;
    QDoubleSpinBox* m_left;
    QPushButton* m_link;
    bool m_updating{false};
};
