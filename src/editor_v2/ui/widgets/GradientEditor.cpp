#include "GradientEditor.h"
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <algorithm>

static QGradientStops toQGradientStops(const QVector<GradientStop>& stops)
{
    QGradientStops result;
    result.reserve(stops.size());
    for (const auto& s : stops)
        result.append({s.position, s.color});
    return result;
}

GradientEditor::GradientEditor(QWidget* parent) : QWidget(parent)
{
    m_stops = {{0.0, Qt::black}, {1.0, Qt::white}};
    setMinimumHeight(kBarH);
}

QVector<GradientStop> GradientEditor::stops() const
{
    return m_stops;
}

void GradientEditor::setStops(const QVector<GradientStop>& stops)
{
    m_stops = stops;
    sortStops();
    m_selected = qBound(0, m_selected, m_stops.size() - 1);
    update();
}

QSize GradientEditor::sizeHint() const
{
    return {200, kBarH};
}

QRect GradientEditor::barRect() const
{
    return QRect(kHandleR, 0, width() - 2 * kHandleR, kBarH);
}

QRect GradientEditor::handleRect(int idx) const
{
    if (idx < 0 || idx >= m_stops.size())
        return {};
    QRect bar = barRect();
    int cx = bar.left() + qRound(m_stops[idx].position * bar.width());
    int cy = bar.center().y();
    return QRect(cx - kHandleR, cy - kHandleR, 2 * kHandleR, 2 * kHandleR);
}

int GradientEditor::hitTestHandle(QPoint p) const
{
    for (int i = 0; i < m_stops.size(); ++i) {
        if (handleRect(i).adjusted(-3, -3, 3, 3).contains(p))
            return i;
    }
    return -1;
}

qreal GradientEditor::positionFromX(int x) const
{
    QRect bar = barRect();
    if (bar.width() <= 0)
        return 0.0;
    return qBound(0.0, (qreal)(x - bar.left()) / bar.width(), 1.0);
}

void GradientEditor::sortStops()
{
    std::stable_sort(
        m_stops.begin(), m_stops.end(),
        [](const GradientStop& a, const GradientStop& b) { return a.position < b.position; });
}

void GradientEditor::editStopColor(int idx)
{
    QColor chosen = QColorDialog::getColor(m_stops[idx].color, this, "Stop Color",
                                           QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        m_stops[idx].color = chosen;
        emit stopsChanged(m_stops);
        update();
    }
}

void GradientEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRect bar = barRect();

    // Checkerboard
    int cell = 5;
    for (int row = 0; row * cell < bar.height(); ++row) {
        for (int col = 0; col * cell < bar.width(); ++col) {
            QColor c = ((row + col) % 2 == 0) ? QColor(180, 180, 180) : Qt::white;
            p.fillRect(bar.x() + col * cell, bar.y() + row * cell, cell, cell, c);
        }
    }

    // Gradient fill
    if (m_stops.size() >= 2) {
        QLinearGradient grad(bar.left(), 0, bar.right(), 0);
        QVector<GradientStop> sorted = m_stops;
        std::sort(sorted.begin(), sorted.end(), [](const GradientStop& a, const GradientStop& b) {
            return a.position < b.position;
        });
        for (const auto& s : sorted)
            grad.setColorAt(s.position, s.color);
        p.fillRect(bar, grad);
    } else if (!m_stops.isEmpty()) {
        p.fillRect(bar, m_stops[0].color);
    }

    p.setPen(palette().dark().color());
    p.drawRect(bar.adjusted(0, 0, -1, -1));

    // Stop handles — two passes so selected renders on top
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < m_stops.size(); ++i) {
            bool isSelected = (i == m_selected);
            if (pass == 0 && isSelected)
                continue;
            if (pass == 1 && !isSelected)
                continue;

            QPoint center = handleRect(i).center();

            p.save();
            p.setPen(Qt::NoPen);
            p.setBrush(m_stops[i].color);
            p.drawEllipse(center, kHandleR, kHandleR);
            p.restore();

            p.setPen(QPen(QColor(0, 0, 0, 80), 5));
            p.drawEllipse(center, kHandleR, kHandleR);

            if (isSelected)
                p.setPen(QPen(palette().highlight().color(), 2.5));
            else
                p.setPen(QPen(Qt::white, 2));
            p.drawEllipse(center, kHandleR, kHandleR);
        }
    }
}

void GradientEditor::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;

    int h = hitTestHandle(e->pos());
    if (h >= 0) {
        m_selected = h;
        m_dragging = h;
        update();
        return;
    }

    if (barRect().contains(e->pos())) {
        qreal pos = positionFromX(e->pos().x());
        m_stops.append({pos, sampleGradient(toQGradientStops(m_stops), pos)});
        m_dragging = m_stops.size() - 1;
        m_selected = m_dragging;
        emit stopsChanged(m_stops);
        update();
    }
}

void GradientEditor::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging < 0 || !(e->buttons() & Qt::LeftButton))
        return;
    m_stops[m_dragging].position = positionFromX(e->pos().x());
    emit stopsChanged(m_stops);
    update();
}

void GradientEditor::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || m_dragging < 0)
        return;

    GradientStop dragged = m_stops[m_dragging];
    sortStops();
    for (int i = 0; i < m_stops.size(); ++i) {
        if (qAbs(m_stops[i].position - dragged.position) < 1e-9 &&
            m_stops[i].color == dragged.color) {
            m_selected = i;
            break;
        }
    }
    m_dragging = -1;
    update();
}

void GradientEditor::mouseDoubleClickEvent(QMouseEvent* e)
{
    int h = hitTestHandle(e->pos());
    if (h >= 0)
        editStopColor(h);
}

void GradientEditor::contextMenuEvent(QContextMenuEvent* e)
{
    int h = hitTestHandle(e->pos());
    QMenu menu(this);

    if (h >= 0) {
        QAction* editAct = menu.addAction("Edit Color...");
        QAction* deleteAct = menu.addAction("Delete Stop");
        deleteAct->setEnabled(m_stops.size() > 2);

        QAction* chosen = menu.exec(e->globalPos());
        if (chosen == editAct) {
            editStopColor(h);
        } else if (chosen == deleteAct) {
            m_stops.removeAt(h);
            m_selected = qBound(0, m_selected, m_stops.size() - 1);
            emit stopsChanged(m_stops);
            update();
        }
    } else if (barRect().contains(e->pos())) {
        QAction* addAct = menu.addAction("Add Stop Here");
        if (menu.exec(e->globalPos()) == addAct) {
            qreal pos = positionFromX(e->pos().x());
            m_stops.append({pos, sampleGradient(toQGradientStops(m_stops), pos)});
            sortStops();
            emit stopsChanged(m_stops);
            update();
        }
    }
}
