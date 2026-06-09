#ifndef CORNERRADIUSEDITOR_H
#define CORNERRADIUSEDITOR_H

#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

class CornerRadiusEditor : public QWidget {
    Q_OBJECT
public:
    explicit CornerRadiusEditor(QWidget* parent = nullptr);

    void setValues(float tl, float tr, float br, float bl);
    void getValues(float& tl, float& tr, float& br, float& bl) const;

signals:
    void valuesChanged(float tl, float tr, float br, float bl);

private slots:
    void onSpinChange(double value);
    void onLinkClick(bool value);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QDoubleSpinBox* m_tl;
    QDoubleSpinBox* m_tr;
    QDoubleSpinBox* m_bl;
    QDoubleSpinBox* m_br;
    QPushButton* m_link;
    bool m_updating{false}; // prevent recursion
};

#endif // CORNERRADIUSEDITOR_H
