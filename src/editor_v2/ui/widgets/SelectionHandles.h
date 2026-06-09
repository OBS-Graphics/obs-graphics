#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>

class QPainter;

// Lightweight helper (no QObject) for drawing and hit-testing selection handles.
// The 8 handles are indexed as follows:
//   0=TL, 1=TC, 2=TR, 3=ML, 4=MR, 5=BL, 6=BC, 7=BR
struct SelectionHandles {
    static constexpr int kHandleSize = 8;
    static constexpr int kHandleCount = 8;

    // Return the rect (in widget coordinates) for a given handle index,
    // computed from the element's bounding rect already in widget coords.
    static QRectF handleRect(int handleIndex, const QRectF& elementInWidget);

    // Return handle index (0-7) if widgetPt hits a handle, -1 otherwise.
    static int hitTest(QPointF widgetPt, const QRectF& elementInWidget);

    // Draw all 8 handles and the selection border rectangle on the painter.
    // activeHandle: index of the handle currently being dragged (-1 = none).
    // highlight: accent color for the active handle (pass palette().highlight().color()).
    static void draw(QPainter& p, const QRectF& elementInWidget, int activeHandle = -1,
                     QColor highlight = QColor(0, 120, 215));
};
