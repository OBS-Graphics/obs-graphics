#pragma once
#include "ColorUtils.h"
#include <QColor>
#include <QVector>
#include <QWidget>

struct GradientStop {
    qreal position; // 0.0 – 1.0
    QColor color;
};

class GradientEditor : public QWidget {
    Q_OBJECT
public:
    explicit GradientEditor(QWidget* parent = nullptr);
    QVector<GradientStop> stops() const;
    void setStops(const QVector<GradientStop>& stops);
    int selectedStop() const
    {
        return m_selected;
    }
    QSize sizeHint() const override;
signals:
    void stopsChanged(const QVector<GradientStop>&);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    static constexpr int kBarH = 24;
    static constexpr int kHandleR = 7;

    QVector<GradientStop> m_stops;
    int m_selected{0};
    int m_dragging{-1};

    QRect barRect() const;
    QRect handleRect(int idx) const;
    int hitTestHandle(QPoint p) const;
    qreal positionFromX(int x) const;
    void sortStops();
    void editStopColor(int idx);
};
