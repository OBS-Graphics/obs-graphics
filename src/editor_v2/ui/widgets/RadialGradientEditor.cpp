#include "RadialGradientEditor.h"
#include "GradientHandlePainter.h"
#include "ColorUtils.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QMenu>
#include <QResizeEvent>
#include <algorithm>
#include <cmath>

static QGradientStops toQGradientStops(const QVector<GradientStop>& stops)
{
    QGradientStops r;
    r.reserve(stops.size());
    for (const auto& s : stops)
        r.append({s.position, s.color});
    return r;
}

RadialGradientEditor::RadialGradientEditor(QWidget* parent)
    : QWidget(parent)
{
    setStops({{0.0, Qt::black}, {1.0, Qt::white}});
    setMinimumSize(120, 100);
}

QVector<GradientStop> RadialGradientEditor::stops() const { return m_stops; }

void RadialGradientEditor::setStops(const QVector<GradientStop>& stops)
{
    m_stops = stops.size() >= 2 ? stops : QVector<GradientStop>{{0.0, Qt::black}, {1.0, Qt::white}};
    sortStops();
    m_selected = qBound(0, m_selected, m_stops.size() - 1);
    emit stopSelected(m_selected);
    update();
}

// Returns normalized radius: hypot of the normalized delta, effectively fraction of width
qreal RadialGradientEditor::radius() const
{
    QPointF d = m_radiusEnd - m_center;
    return std::hypot(d.x(), d.y());
}

void RadialGradientEditor::setCenter(QPointF c)
{
    QPointF dir = m_radiusEnd - m_center;
    m_center    = c;
    m_radiusEnd = c + dir;
    update();
}

void RadialGradientEditor::setRadius(qreal r)
{
    r = qMax(r, 1e-4);
    QPointF d   = m_radiusEnd - m_center;
    qreal   len = std::hypot(d.x(), d.y());
    m_radiusEnd = (len < 1e-10)
        ? m_center + QPointF(r, 0)
        : m_center + d * (r / len);
    update();
}

void RadialGradientEditor::updateStops()
{
    sortStops();
    emit stopsChanged(m_stops);
    update();
}

QSize RadialGradientEditor::sizeHint() const { return {300, 200}; }

void RadialGradientEditor::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    update();
}

// Returns pixel coords for stop i (along the line from center to radiusEnd)
QPointF RadialGradientEditor::stopPos(int i) const
{
    QPointF normPt = m_center + m_stops[i].position * (m_radiusEnd - m_center);
    return toPixelPt(normPt);
}

qreal RadialGradientEditor::tFromPoint(QPointF p) const
{
    QPointF n   = toNormPt(p);
    QPointF ab  = m_radiusEnd - m_center;
    qreal   len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-10) return 0.0;
    qreal t = ((n.x() - m_center.x()) * ab.x() + (n.y() - m_center.y()) * ab.y()) / len2;
    return qBound(0.0, t, 1.0);
}

int RadialGradientEditor::hitTestHandle(QPointF p) const
{
    int     n         = m_stops.size();
    QPointF centerPx  = toPixelPt(m_center);
    QPointF radiusEndPx = toPixelPt(m_radiusEnd);

    if (m_selected == 0     && GHP::hitCircle(p, centerPx,    kEndpointR)) return 0;
    if (m_selected == n - 1 && GHP::hitCircle(p, radiusEndPx, kEndpointR)) return n - 1;
    if (m_selected >= 1 && m_selected <= n - 2
        && GHP::hitDiamond(p, stopPos(m_selected), kDiamondD))             return m_selected;

    if (GHP::hitCircle(p, radiusEndPx, kEndpointR)) return n - 1;
    if (GHP::hitCircle(p, centerPx,    kEndpointR)) return 0;
    for (int i = n - 2; i >= 1; --i)
        if (GHP::hitDiamond(p, stopPos(i), kDiamondD)) return i;
    return -1;
}

void RadialGradientEditor::sortStops()
{
    std::stable_sort(m_stops.begin(), m_stops.end(),
        [](const GradientStop& a, const GradientStop& b){ return a.position < b.position; });
    if (!m_stops.isEmpty()) {
        m_stops.first().position = 0.0;
        m_stops.last().position  = 1.0;
    }
}

void RadialGradientEditor::editStopColor(int idx)
{
    QColor chosen = QColorDialog::getColor(
        m_stops[idx].color, this, "Stop Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        m_stops[idx].color = chosen;
        emit stopsChanged(m_stops);
        update();
    }
}

void RadialGradientEditor::addStopAt(qreal t)
{
    t = qBound(1e-4, t, 1.0 - 1e-4);
    QColor col = sampleGradient(toQGradientStops(m_stops), t);
    m_stops.append({t, col});
    sortStops();
    for (int i = 0; i < m_stops.size(); ++i) {
        if (qAbs(m_stops[i].position - t) < 1e-9 && m_stops[i].color == col) {
            m_selected = i;
            emit stopSelected(m_selected);
            break;
        }
    }
    emit stopsChanged(m_stops);
    update();
}

void RadialGradientEditor::drawCheckerboard(QPainter& p) const
{
    int cell = kCheckerCell;
    QRect r = rect();
    for (int row = 0; row * cell < r.height(); ++row)
        for (int col = 0; col * cell < r.width(); ++col) {
            QColor c = ((row + col) % 2 == 0) ? QColor(180, 180, 180) : Qt::white;
            p.fillRect(r.x() + col * cell, r.y() + row * cell, cell, cell, c);
        }
}

void RadialGradientEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawCheckerboard(p);

    QPointF centerPx    = toPixelPt(m_center);
    QPointF radiusEndPx = toPixelPt(m_radiusEnd);
    // Draw the circle preview using normalized radius * widget width
    qreal pixelR = radius() * (width() > 0 ? width() : 1.0);

    if (m_stops.size() >= 2) {
        QRadialGradient grad(centerPx, pixelR, centerPx);
        for (const auto& s : m_stops)
            grad.setColorAt(s.position, s.color);
        p.fillRect(rect(), grad);
    }

    p.setPen(QPen(QColor(220, 220, 220, 160), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(centerPx, pixelR, pixelR);

    p.setPen(QPen(QColor(0, 0, 0, 80), 1.2));
    p.drawLine(centerPx, radiusEndPx);

    int n = m_stops.size();
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < n; ++i) {
            bool sel = (i == m_selected);
            if (pass == 0 && sel)  continue;
            if (pass == 1 && !sel) continue;

            if (i == 0)
                GHP::paintCircleHandle(p, centerPx,    kEndpointR, m_stops[0].color,   sel, palette());
            else if (i == n - 1)
                GHP::paintCircleHandle(p, radiusEndPx, kEndpointR, m_stops[n-1].color, sel, palette());
            else
                GHP::paintDiamondHandle(p, stopPos(i), kDiamondD, m_stops[i].color, sel, palette());
        }
    }
}

void RadialGradientEditor::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;

    QPointF pos = e->position();
    int h = hitTestHandle(pos);
    if (h >= 0) {
        m_selected = h;
        emit stopSelected(h);
        if (h == 0) {
            m_draggingCenter = true;
            m_dragOffset = pos - toPixelPt(m_center);
        } else if (h == m_stops.size() - 1) {
            m_draggingEdge = true;
            m_dragOffset = pos - toPixelPt(m_radiusEnd);
        } else {
            m_dragging = h;
        }
        update();
        return;
    }

    qreal t;
    if (GHP::hitSegment(pos, toPixelPt(m_center), toPixelPt(m_radiusEnd), kLineHitTol, &t))
        addStopAt(t);
}

void RadialGradientEditor::mouseMoveEvent(QMouseEvent* e)
{
    if (!(e->buttons() & Qt::LeftButton)) return;
    QPointF pos = e->position();

    if (m_draggingCenter) {
        QPointF newPx = pos - m_dragOffset;
        QPointF dir   = m_radiusEnd - m_center;
        m_center    = toNormPt(newPx);
        m_center.setX(qBound(0.0, m_center.x(), 1.0));
        m_center.setY(qBound(0.0, m_center.y(), 1.0));
        m_radiusEnd = m_center + dir;
        emit geometryChanged(m_center, radius());
        update();
    } else if (m_draggingEdge) {
        QPointF newPx = pos - m_dragOffset;
        m_radiusEnd   = toNormPt(newPx);
        m_radiusEnd.setX(qBound(0.0, m_radiusEnd.x(), 1.0));
        m_radiusEnd.setY(qBound(0.0, m_radiusEnd.y(), 1.0));
        emit geometryChanged(m_center, radius());
        update();
    } else if (m_dragging >= 1 && m_dragging <= m_stops.size() - 2) {
        qreal t = qBound(1e-4, tFromPoint(pos), 1.0 - 1e-4);
        m_stops[m_dragging].position = t;
        emit stopsChanged(m_stops);
        update();
    }
}

void RadialGradientEditor::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;

    if (m_draggingCenter) { m_draggingCenter = false; update(); return; }
    if (m_draggingEdge)   { m_draggingEdge   = false; update(); return; }

    if (m_dragging >= 0) {
        GradientStop dragged = m_stops[m_dragging];
        sortStops();
        for (int i = 0; i < m_stops.size(); ++i) {
            if (qAbs(m_stops[i].position - dragged.position) < 1e-9
                && m_stops[i].color == dragged.color) {
                m_selected = i;
                emit stopSelected(m_selected);
                break;
            }
        }
        m_dragging = -1;
        update();
    }
}

void RadialGradientEditor::mouseDoubleClickEvent(QMouseEvent* e)
{
    int h = hitTestHandle(e->position());
    if (h >= 0) editStopColor(h);
}

void RadialGradientEditor::contextMenuEvent(QContextMenuEvent* e)
{
    int h = hitTestHandle(e->pos());
    QMenu menu(this);

    if (h >= 0) {
        QAction* editAct   = menu.addAction("Edit Color...");
        QAction* deleteAct = nullptr;
        bool isEndpoint = (h == 0 || h == m_stops.size() - 1);
        if (!isEndpoint) {
            deleteAct = menu.addAction("Delete Stop");
            deleteAct->setEnabled(m_stops.size() > 2);
        }
        QAction* chosen = menu.exec(e->globalPos());
        if (chosen == editAct) {
            editStopColor(h);
        } else if (deleteAct && chosen == deleteAct) {
            m_stops.removeAt(h);
            m_selected = qBound(0, m_selected, m_stops.size() - 1);
            emit stopSelected(m_selected);
            emit stopsChanged(m_stops);
            update();
        }
    } else {
        qreal t;
        if (GHP::hitSegment(e->pos(), toPixelPt(m_center), toPixelPt(m_radiusEnd), kLineHitTol, &t)) {
            QAction* addAct = menu.addAction("Add Stop Here");
            if (menu.exec(e->globalPos()) == addAct)
                addStopAt(t);
        }
    }
}
