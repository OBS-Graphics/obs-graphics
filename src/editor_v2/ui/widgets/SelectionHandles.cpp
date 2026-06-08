#include "SelectionHandles.h"

#include <QPainter>
#include <QPen>
#include <QBrush>

// Handle order: TL=0, TC=1, TR=2, ML=3, MR=4, BL=5, BC=6, BR=7

static QPointF handleCenter(int idx, const QRectF& r)
{
    const double l  = r.left();
    const double t  = r.top();
    const double cx = r.center().x();
    const double cy = r.center().y();
    const double ri = r.right();
    const double bo = r.bottom();

    switch (idx) {
    case 0: return {l,  t};   // TL
    case 1: return {cx, t};   // TC
    case 2: return {ri, t};   // TR
    case 3: return {l,  cy};  // ML
    case 4: return {ri, cy};  // MR
    case 5: return {l,  bo};  // BL
    case 6: return {cx, bo};  // BC
    case 7: return {ri, bo};  // BR
    default: return {};
    }
}

QRectF SelectionHandles::handleRect(int handleIndex, const QRectF& elementInWidget)
{
    const double half = kHandleSize / 2.0;
    QPointF c = handleCenter(handleIndex, elementInWidget);
    return {c.x() - half, c.y() - half,
            static_cast<double>(kHandleSize),
            static_cast<double>(kHandleSize)};
}

int SelectionHandles::hitTest(QPointF widgetPt, const QRectF& elementInWidget)
{
    for (int i = 0; i < kHandleCount; ++i) {
        if (handleRect(i, elementInWidget).contains(widgetPt))
            return i;
    }
    return -1;
}

void SelectionHandles::draw(QPainter& p, const QRectF& elementInWidget,
                             int activeHandle, QColor highlight)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);

    // Selection border — shadow pass then white pass
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 120), 3.0));
    p.drawRect(elementInWidget);
    p.setPen(QPen(Qt::white, 1.0));
    p.drawRect(elementInWidget);

    // Handles — two passes so the active handle is drawn on top
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < kHandleCount; ++i) {
            const bool isActive = (i == activeHandle);
            if (pass == 0 && isActive)  continue;
            if (pass == 1 && !isActive) continue;

            const QRectF r = handleRect(i, elementInWidget);

            // Shadow: wide dark stroke so handles pop on light backgrounds
            p.setPen(QPen(QColor(0, 0, 0, 100), 5.0));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);

            // Fill and border
            p.setBrush(isActive ? highlight : Qt::white);
            p.setPen(QPen(Qt::white, 1.5));
            p.drawRect(r);
        }
    }

    p.restore();
}
