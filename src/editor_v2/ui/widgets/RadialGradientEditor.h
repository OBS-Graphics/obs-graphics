#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include "GradientEditor.h"

class RadialGradientEditor : public QWidget {
    Q_OBJECT
public:
    explicit RadialGradientEditor(QWidget* parent = nullptr);

    QVector<GradientStop> stops() const;
    void setStops(const QVector<GradientStop>& stops);

    // center and radius are in normalized (0–1) coordinates
    QPointF center() const { return m_center; }
    qreal   radius() const;
    void setCenter(QPointF c);
    void setRadius(qreal r);

    int selectedStop() const { return m_selected; }
    GradientStop& stop(int index) { return m_stops[index]; }

    void updateStops();

    QSize sizeHint() const override;

signals:
    void stopsChanged(const QVector<GradientStop>&);
    void stopSelected(int index);
    void geometryChanged(QPointF center, qreal radius);

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
    static constexpr qreal kMinRadius   = 4.0;

    // Both stored in normalized (0–1) space
    QVector<GradientStop> m_stops;
    QPointF m_center{0.5, 0.5};
    QPointF m_radiusEnd{0.85, 0.5};

    int     m_selected{0};
    int     m_dragging{-1};
    bool    m_draggingCenter{false};
    bool    m_draggingEdge{false};
    QPointF m_dragOffset;

    QPointF toPixelPt(QPointF norm) const { return {norm.x() * width(), norm.y() * height()}; }
    QPointF toNormPt(QPointF px) const {
        return {width() > 0 ? px.x() / width() : 0.0,
                height() > 0 ? px.y() / height() : 0.0};
    }

    QPointF stopPos(int i) const;     // pixel coords
    int     hitTestHandle(QPointF p) const;
    qreal   tFromPoint(QPointF p) const;
    void    sortStops();
    void    editStopColor(int idx);
    void    addStopAt(qreal t);
    void    drawCheckerboard(QPainter& p) const;
};
