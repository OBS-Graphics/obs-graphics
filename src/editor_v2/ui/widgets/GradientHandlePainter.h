#pragma once
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <cmath>

namespace GHP {

inline void paintCircleHandle(QPainter& p, QPointF center, int r, QColor color, bool selected,
                              const QPalette& pal)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(center, r, r);
    p.restore();

    p.setPen(QPen(QColor(0, 0, 0, 80), 5));
    p.drawEllipse(center, r, r);

    p.setPen(selected ? QPen(pal.highlight().color(), 2.5) : QPen(Qt::white, 2));
    p.drawEllipse(center, r, r);
}

inline void paintDiamondHandle(QPainter& p, QPointF center, int d, QColor color, bool selected,
                               const QPalette& pal)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(center);
    p.rotate(45.0);

    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRect(-d, -d, 2 * d, 2 * d);

    p.setPen(QPen(QColor(0, 0, 0, 80), 5));
    p.setBrush(Qt::NoBrush);
    p.drawRect(-d, -d, 2 * d, 2 * d);

    p.setPen(selected ? QPen(pal.highlight().color(), 2.5) : QPen(Qt::white, 2));
    p.drawRect(-d, -d, 2 * d, 2 * d);

    p.restore();
}

inline void paintNubHandle(QPainter& p, QPointF center, int halfSize, bool selected,
                           const QPalette& pal)
{
    QRectF r(center.x() - halfSize, center.y() - halfSize, 2 * halfSize, 2 * halfSize);

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(200, 200, 200));
    p.drawEllipse(r);

    p.setPen(QPen(QColor(0, 0, 0, 80), 3));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r);

    p.setPen(selected ? QPen(pal.highlight().color(), 2) : QPen(Qt::white, 1.5));
    p.drawEllipse(r);
    p.restore();
}

inline bool hitCircle(QPointF p, QPointF center, int r, int slack = 3)
{
    qreal dx = p.x() - center.x(), dy = p.y() - center.y();
    qreal lim = r + slack;
    return dx * dx + dy * dy <= lim * lim;
}

inline bool hitDiamond(QPointF p, QPointF center, int d, int slack = 3)
{
    return qAbs(p.x() - center.x()) + qAbs(p.y() - center.y()) <= d + slack;
}

inline bool hitSegment(QPointF p, QPointF a, QPointF b, qreal tol, qreal* outT = nullptr)
{
    QPointF ab = b - a;
    qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
    qreal t = 0.0;
    if (len2 > 1e-10) {
        t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
        t = qBound(0.0, t, 1.0);
    }
    QPointF closest = a + t * ab;
    qreal dx = p.x() - closest.x(), dy = p.y() - closest.y();
    bool hit = (dx * dx + dy * dy) <= tol * tol;
    if (hit && outT)
        *outT = t;
    return hit;
}

} // namespace GHP
