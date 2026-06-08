#pragma once
#include <QWidget>
#include <QColor>

class ColorPreview : public QWidget {
    Q_OBJECT
public:
    explicit ColorPreview(QWidget* parent = nullptr);
    QColor color() const;
    void setColor(const QColor& c);
    void setClickable(bool clickable);
signals:
    void clicked();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    QColor m_color{Qt::red};
    bool m_clickable{false};
};
