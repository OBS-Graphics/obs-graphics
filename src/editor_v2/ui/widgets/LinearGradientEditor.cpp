#include "LinearGradientEditor.h"
#include "GradientHandlePainter.h"
#include "ColorUtils.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QColorDialog>
#include <QMenu>
#include <QResizeEvent>
#include <algorithm>

static QGradientStops toQGradientStops(const QVector<GradientStop>& stops)
{
    QGradientStops r;
    r.reserve(stops.size());
    for (const auto& s : stops)
        r.append({s.position, s.color});
    return r;
}

LinearGradientEditor::LinearGradientEditor(QWidget* parent)
    : QWidget(parent)
{
    setStops({{0.0, Qt::black}, {1.0, Qt::white}});
    setMinimumSize(100, 100);
}

QVector<GradientStop> LinearGradientEditor::stops() const { return m_stops; }

void LinearGradientEditor::setStops(const QVector<GradientStop>& stops)
{
    m_stops = stops.size() >= 2 ? stops : QVector<GradientStop>{{0.0, Qt::black}, {1.0, Qt::white}};
    sortStops();
    int newSel = qBound(0, m_selected, m_stops.size() - 1);
    if (m_selected != newSel) {
        m_selected = newSel;
        emit stopSelected(m_selected);
    }
    update();
}

// p1/p2 are normalized (0-1); setP1/setP2 accept the same
void LinearGradientEditor::setP1(QPointF p) { m_p1 = p; update(); }
void LinearGradientEditor::setP2(QPointF p) { m_p2 = p; update(); }

void LinearGradientEditor::updateStops()
{
    sortStops();
    emit stopsChanged(m_stops);
    update();
}

QSize LinearGradientEditor::sizeHint() const { return {300, 200}; }

void LinearGradientEditor::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    update();
}

// Returns pixel coordinates of stop i
QPointF LinearGradientEditor::stopPos(int i) const
{
    QPointF norm = m_p1 + m_stops[i].position * (m_p2 - m_p1);
    return toPixel(norm);
}

// p is in pixel coords; returns t along the gradient line (0-1)
qreal LinearGradientEditor::tFromPoint(QPointF p) const
{
    QPointF n = toNorm(p);
    QPointF ab = m_p2 - m_p1;
    qreal len2 = ab.x() * ab.x() + ab.y() * ab.y();
    if (len2 < 1e-10) return 0.0;
    qreal t = ((n.x() - m_p1.x()) * ab.x() + (n.y() - m_p1.y()) * ab.y()) / len2;
    return qBound(0.0, t, 1.0);
}

int LinearGradientEditor::hitTestHandle(QPointF p) const
{
    int n = m_stops.size();
    QPointF p1px = toPixel(m_p1), p2px = toPixel(m_p2);

    if (m_selected == 0        && GHP::hitCircle(p, p1px, kEndpointR)) return 0;
    if (m_selected == n - 1    && GHP::hitCircle(p, p2px, kEndpointR)) return n - 1;
    if (m_selected >= 1 && m_selected <= n - 2
        && GHP::hitDiamond(p, stopPos(m_selected), kDiamondD))         return m_selected;

    if (GHP::hitCircle(p, p2px, kEndpointR)) return n - 1;
    if (GHP::hitCircle(p, p1px, kEndpointR)) return 0;
    for (int i = n - 2; i >= 1; --i)
        if (GHP::hitDiamond(p, stopPos(i), kDiamondD)) return i;
    return -1;
}

void LinearGradientEditor::sortStops()
{
    std::stable_sort(m_stops.begin(), m_stops.end(),
        [](const GradientStop& a, const GradientStop& b){ return a.position < b.position; });
    if (!m_stops.isEmpty()) {
        m_stops.first().position = 0.0;
        m_stops.last().position  = 1.0;
    }
}

void LinearGradientEditor::editStopColor(int idx)
{
    QColor chosen = QColorDialog::getColor(
        m_stops[idx].color, this, "Stop Color", QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        m_stops[idx].color = chosen;
        emit stopsChanged(m_stops);
        update();
    }
}

void LinearGradientEditor::addStopAt(qreal t)
{
    QColor col = sampleGradient(toQGradientStops(m_stops), t);
    m_stops.append({t, col});
    sortStops();
    for (int i = 0; i < m_stops.size(); ++i) {
        if (qAbs(m_stops[i].position - t) < 1e-9 && m_stops[i].color == col) {
            m_selected = i;
            emit stopSelected(i);
            break;
        }
    }
    emit stopsChanged(m_stops);
    update();
}

void LinearGradientEditor::drawCheckerboard(QPainter& p) const
{
    int cell = kCheckerCell;
    QRect r = rect();
    for (int row = 0; row * cell < r.height(); ++row)
        for (int col = 0; col * cell < r.width(); ++col) {
            QColor c = ((row + col) % 2 == 0) ? QColor(180, 180, 180) : Qt::white;
            p.fillRect(r.x() + col * cell, r.y() + row * cell, cell, cell, c);
        }
}

void LinearGradientEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    drawCheckerboard(p);

    QPointF p1px = toPixel(m_p1), p2px = toPixel(m_p2);

    if (m_stops.size() >= 2) {
        QLinearGradient grad(p1px, p2px);
        for (const auto& s : m_stops)
            grad.setColorAt(s.position, s.color);
        p.fillRect(rect(), grad);
    }

    p.setPen(QPen(QColor(0, 0, 0, 100), 1.5));
    p.drawLine(p1px, p2px);

    int n = m_stops.size();
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < n; ++i) {
            bool sel = (i == m_selected);
            if (pass == 0 && sel)  continue;
            if (pass == 1 && !sel) continue;

            if (i == 0)
                GHP::paintCircleHandle(p, p1px, kEndpointR, m_stops[0].color, sel, palette());
            else if (i == n - 1)
                GHP::paintCircleHandle(p, p2px, kEndpointR, m_stops[n-1].color, sel, palette());
            else
                GHP::paintDiamondHandle(p, stopPos(i), kDiamondD, m_stops[i].color, sel, palette());
        }
    }
}

void LinearGradientEditor::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;

    QPointF pos = e->position();
    int h = hitTestHandle(pos);
    if (h >= 0) {
        m_selected = h;
        emit stopSelected(h);
        if (h == 0) {
            m_draggingP1 = true;
            m_dragOffset = pos - toPixel(m_p1);
        } else if (h == m_stops.size() - 1) {
            m_draggingP2 = true;
            m_dragOffset = pos - toPixel(m_p2);
        } else {
            m_dragging   = h;
            m_dragOffset = pos - stopPos(h);
        }
        update();
        return;
    }

    qreal t;
    if (GHP::hitSegment(pos, toPixel(m_p1), toPixel(m_p2), kLineHitTol, &t))
        addStopAt(t);
}

void LinearGradientEditor::mouseMoveEvent(QMouseEvent* e)
{
    if (!(e->buttons() & Qt::LeftButton)) return;
    QPointF pos = e->position();

    if (m_draggingP1) {
        QPointF newPx = pos - m_dragOffset;
        m_p1 = {qBound(0.0, width()  > 0 ? newPx.x() / width()  : 0.0, 1.0),
                qBound(0.0, height() > 0 ? newPx.y() / height() : 0.0, 1.0)};
        emit p1Changed(m_p1);
        update();
    } else if (m_draggingP2) {
        QPointF newPx = pos - m_dragOffset;
        m_p2 = {qBound(0.0, width()  > 0 ? newPx.x() / width()  : 0.0, 1.0),
                qBound(0.0, height() > 0 ? newPx.y() / height() : 0.0, 1.0)};
        emit p2Changed(m_p2);
        update();
    } else if (m_dragging >= 1 && m_dragging <= m_stops.size() - 2) {
        qreal t = qBound(1e-4, tFromPoint(pos), 1.0 - 1e-4);
        m_stops[m_dragging].position = t;
        emit stopsChanged(m_stops);
        update();
    }
}

void LinearGradientEditor::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;

    if (m_draggingP1) { m_draggingP1 = false; update(); return; }
    if (m_draggingP2) { m_draggingP2 = false; update(); return; }

    if (m_dragging >= 0) {
        GradientStop dragged = m_stops[m_dragging];
        sortStops();
        for (int i = 0; i < m_stops.size(); ++i) {
            if (qAbs(m_stops[i].position - dragged.position) < 1e-9
                && m_stops[i].color == dragged.color) {
                m_selected = i;
                emit stopSelected(i);
                break;
            }
        }
        m_dragging = -1;
        update();
    }
}

void LinearGradientEditor::mouseDoubleClickEvent(QMouseEvent* e)
{
    int h = hitTestHandle(e->position());
    if (h >= 0) editStopColor(h);
}

void LinearGradientEditor::contextMenuEvent(QContextMenuEvent* e)
{
    int h = hitTestHandle(e->pos());
    QMenu menu(this);

    if (h >= 0) {
        QAction* editAct = menu.addAction("Edit Color...");
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
        if (GHP::hitSegment(e->pos(), toPixel(m_p1), toPixel(m_p2), kLineHitTol, &t)) {
            QAction* addAct = menu.addAction("Add Stop Here");
            if (menu.exec(e->globalPos()) == addAct)
                addStopAt(t);
        }
    }
}
