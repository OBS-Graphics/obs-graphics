#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include "GradientEditor.h"

class LinearGradientEditor : public QWidget {
    Q_OBJECT
public:
    explicit LinearGradientEditor(QWidget* parent = nullptr);

    QVector<GradientStop> stops() const;
    void setStops(const QVector<GradientStop>& stops);

    // p1/p2 are in normalized (0–1) coordinates relative to the widget
    QPointF p1() const { return m_p1; }
    QPointF p2() const { return m_p2; }
    void setP1(QPointF p);
    void setP2(QPointF p);

    int selectedStop() const { return m_selected; }
    GradientStop& stop(int index) { return m_stops[index]; }

    void updateStops();

    QSize sizeHint() const override;

signals:
    void stopsChanged(const QVector<GradientStop>&);
    void stopSelected(int index);
    void p1Changed(QPointF);
    void p2Changed(QPointF);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    static constexpr int   kEndpointR   = 10;
    static constexpr int   kDiamondD    = 7;
    static constexpr int   kLineHitTol  = 8;
    static constexpr int   kCheckerCell = 5;

    QVector<GradientStop> m_stops;
    // Stored in normalized (0–1) space; rendered by multiplying by width()/height()
    QPointF               m_p1{0.2, 0.5};
    QPointF               m_p2{0.8, 0.5};
    int                   m_selected{0};
    int                   m_dragging{-1};
    bool                  m_draggingP1{false};
    bool                  m_draggingP2{false};
    QPointF               m_dragOffset;

    QPointF toPixel(QPointF norm) const { return {norm.x() * width(), norm.y() * height()}; }
    QPointF toNorm(QPointF px) const {
        return {width() > 0 ? px.x() / width() : 0.0,
                height() > 0 ? px.y() / height() : 0.0};
    }

    QPointF stopPos(int i) const;     // returns pixel coords
    int     hitTestHandle(QPointF p) const;
    qreal   tFromPoint(QPointF p) const;
    void    sortStops();
    void    editStopColor(int idx);
    void    addStopAt(qreal t);
    void    drawCheckerboard(QPainter& p) const;
};
