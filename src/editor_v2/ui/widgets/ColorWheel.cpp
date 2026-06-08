#include "ColorWheel.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QtMath>
#include <array>
#include <cmath>

// Angle convention throughout:
//   0° = east (right), increases clockwise on screen (Qt y-down).
//   To convert degrees → screen offset: dx = r*cos(rad), dy = r*sin(rad)
//   with rad = deg * π/180.  This matches atan2(dy, dx) for mouse positions.

static constexpr qreal kRingFraction = 0.22;  // ring width as fraction of outer radius

ColorWheel::ColorWheel(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(100, 100);
}

QColor ColorWheel::color() const { return m_color; }

void ColorWheel::setColor(const QColor& c)
{
    if (c == m_color) return;
    int oldHue = m_color.hsvHue();
    m_color = c;
    if (c.hsvHue() != oldHue)
        m_triangleDirty = true;
    update();
    emit colorChanged(c);
}

QSize ColorWheel::sizeHint() const { return {200, 200}; }
bool ColorWheel::hasHeightForWidth() const { return true; }
int  ColorWheel::heightForWidth(int w) const { return w; }

QPointF ColorWheel::center() const
{
    return {width() / 2.0, height() / 2.0};
}

qreal ColorWheel::outerRadius() const
{
    return qMin(width(), height()) / 2.0 - 1.0;
}

qreal ColorWheel::innerRadius() const
{
    return outerRadius() * (1.0 - kRingFraction);
}

// Returns the 3 vertices of the HSV triangle inscribed in the inner circle:
//   [0] = pure hue  (S=1, V=1)
//   [1] = white     (S=0, V=1) — 120° clockwise from hue
//   [2] = black     (S=0, V=0) — 240° clockwise from hue
std::array<QPointF, 3> ColorWheel::triangleVertices() const
{
    QPointF c = center();
    qreal r = innerRadius();
    int hue = m_color.hsvHue();
    if (hue < 0) hue = 0;

    auto pt = [&](qreal deg) -> QPointF {
        qreal rad = -qDegreesToRadians(deg);
        return c + QPointF(r * std::cos(rad), r * std::sin(rad));
    };

    qreal base = (qreal)hue;
    return { pt(base), pt(base + 120.0), pt(base + 240.0) };
}

// Screen-space position corresponding to the current S/V values.
QPointF ColorWheel::svPosition() const
{
    qreal S = m_color.hsvSaturationF();
    qreal V = m_color.valueF();
    // Barycentric: wHue = S*V, wWhite = V*(1-S), wBlack = 1-V
    qreal wHue   = S * V;
    qreal wWhite = V * (1.0 - S);
    qreal wBlack = 1.0 - V;
    auto verts = triangleVertices();
    return verts[0] * wHue + verts[1] * wWhite + verts[2] * wBlack;
}

bool ColorWheel::pointInRing(QPointF p) const
{
    QPointF d = p - center();
    qreal distSq = d.x()*d.x() + d.y()*d.y();
    qreal rI = innerRadius();
    qreal rO = outerRadius();
    return distSq >= rI*rI && distSq <= rO*rO;
}

bool ColorWheel::pointInTriangle(QPointF p) const
{
    auto v = triangleVertices();
    QPointF e0 = v[1] - v[0], e1 = v[2] - v[0], ep = p - v[0];
    qreal denom = e0.x()*e1.y() - e0.y()*e1.x();
    if (qAbs(denom) < 1e-8) return false;
    qreal u = (ep.x()*e1.y() - ep.y()*e1.x()) / denom;
    qreal vc = (e0.x()*ep.y() - e0.y()*ep.x()) / denom;
    return u >= -1e-6 && vc >= -1e-6 && (u + vc) <= 1.0 + 1e-6;
}

void ColorWheel::handleRingDrag(QPointF p)
{
    QPointF d = p - center();
    qreal angleDeg = qRadiansToDegrees(-std::atan2(d.y(), d.x()));
    if (angleDeg < 0.0) angleDeg += 360.0;
    int hue = qRound(angleDeg) % 360;

    QColor c = m_color;
    c.setHsv(hue, c.hsvSaturation(), c.value(), c.alpha());
    if (c == m_color) return;
    m_color = c;
    m_triangleDirty = true;
    update();
    emit colorChanged(m_color);
}

void ColorWheel::handleTriangleDrag(QPointF p)
{
    auto v = triangleVertices();
    QPointF e0 = v[1] - v[0], e1 = v[2] - v[0], ep = p - v[0];
    qreal denom = e0.x()*e1.y() - e0.y()*e1.x();
    if (qAbs(denom) < 1e-8) return;

    qreal u  = (ep.x()*e1.y() - ep.y()*e1.x()) / denom;  // weight of v[1]=white
    qreal vc = (e0.x()*ep.y() - e0.y()*ep.x()) / denom;  // weight of v[2]=black
    qreal w  = 1.0 - u - vc;                              // weight of v[0]=hue

    // Clamp to triangle
    u  = qMax(0.0, u);
    vc = qMax(0.0, vc);
    w  = qMax(0.0, w);
    qreal sum = u + vc + w;
    if (sum > 1e-8) { u /= sum; vc /= sum; w /= sum; }

    qreal V_val = w + u;                              // wHue + wWhite
    qreal S_val = (V_val > 1e-8) ? w / V_val : 0.0;

    int hue = m_color.hsvHue();
    if (hue < 0) hue = 0;
    QColor nc = QColor::fromHsvF(hue / 360.0, S_val, V_val, m_color.alphaF());
    if (nc == m_color) return;
    m_color = nc;
    update();
    emit colorChanged(m_color);
}

void ColorWheel::rebuildRing()
{
    QSize sz = size();
    m_ringImage = QImage(sz, QImage::Format_ARGB32);
    m_ringImage.fill(Qt::transparent);

    QPainter p(&m_ringImage);
    p.setRenderHint(QPainter::Antialiasing);

    QPointF c = center();
    qreal rO = outerRadius();
    qreal rI = innerRadius();

    // Full-spectrum conical gradient
    QConicalGradient grad(c, 0.0);
    for (int h = 0; h <= 360; h += 3)
        grad.setColorAt(h / 360.0, QColor::fromHsv(h % 360, 255, 255));

    QPainterPath ring;
    ring.addEllipse(c, rO, rO);
    QPainterPath inner;
    inner.addEllipse(c, rI, rI);
    ring -= inner;

    p.fillPath(ring, grad);
    m_ringDirty = false;
}

void ColorWheel::rebuildTriangle()
{
    QSize sz = size();
    m_triangleImage = QImage(sz, QImage::Format_ARGB32);
    m_triangleImage.fill(Qt::transparent);

    auto verts = triangleVertices();
    QPointF v0 = verts[0];  // hue
    QPointF v1 = verts[1];  // white
    QPointF v2 = verts[2];  // black

    int hue = m_color.hsvHue();
    if (hue < 0) hue = 0;
    qreal hueF = hue / 360.0;

    // Precompute edge vectors and inverse denominator for barycentric coords
    QPointF e0 = v1 - v0, e1 = v2 - v0;
    qreal denom = e0.x()*e1.y() - e0.y()*e1.x();
    if (qAbs(denom) < 1e-8) { m_triangleDirty = false; return; }
    qreal invD = 1.0 / denom;

    int x0 = qMax(0, (int)std::floor(std::min({v0.x(), v1.x(), v2.x()})));
    int x1 = qMin(sz.width()  - 1, (int)std::ceil(std::max({v0.x(), v1.x(), v2.x()})));
    int y0 = qMax(0, (int)std::floor(std::min({v0.y(), v1.y(), v2.y()})));
    int y1 = qMin(sz.height() - 1, (int)std::ceil(std::max({v0.y(), v1.y(), v2.y()})));

    for (int y = y0; y <= y1; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(m_triangleImage.scanLine(y));
        for (int x = x0; x <= x1; ++x) {
            QPointF ep((qreal)x - v0.x(), (qreal)y - v0.y());
            qreal u  = (ep.x()*e1.y() - ep.y()*e1.x()) * invD;  // weight v1 (white)
            qreal vc = (e0.x()*ep.y() - e0.y()*ep.x()) * invD;  // weight v2 (black)
            qreal w  = 1.0 - u - vc;                             // weight v0 (hue)
            if (u < -1e-4 || vc < -1e-4 || w < -1e-4) continue;

            qreal V_val = w + u;
            qreal S_val = (V_val > 1e-8) ? w / V_val : 0.0;
            line[x] = QColor::fromHsvF(hueF, qBound(0.0,S_val,1.0), qBound(0.0,V_val,1.0)).rgba();
        }
    }

    m_triangleDirty = false;
}

void ColorWheel::paintEvent(QPaintEvent*)
{
    if (m_ringDirty)     rebuildRing();
    if (m_triangleDirty) rebuildTriangle();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.drawImage(0, 0, m_ringImage);
    p.drawImage(0, 0, m_triangleImage);

    QPointF c = center();

    // Hue indicator on the ring
    {
        int hue = m_color.hsvHue();
        if (hue < 0) hue = 0;
        qreal rad = -qDegreesToRadians(hue);
        qreal rMid = (innerRadius() + outerRadius()) / 2.0;
        QPointF ind = c + QPointF(rMid * std::cos(rad), rMid * std::sin(rad));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(ind, 5, 5);
        p.setPen(QPen(Qt::black, 1));
        p.drawEllipse(ind, 6, 6);
    }

    // S/V indicator inside the triangle
    {
        QPointF svPos = svPosition();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(svPos, 5, 5);
        p.setPen(QPen(Qt::black, 1));
        p.drawEllipse(svPos, 6, 6);
    }
}

void ColorWheel::resizeEvent(QResizeEvent*)
{
    m_ringDirty     = true;
    m_triangleDirty = true;
}

void ColorWheel::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    QPointF pos = QPointF(e->pos());
    if (pointInRing(pos)) {
        m_drag = DragZone::Ring;
        handleRingDrag(pos);
    } else if (pointInTriangle(pos)) {
        m_drag = DragZone::Triangle;
        handleTriangleDrag(pos);
    }
}

void ColorWheel::mouseMoveEvent(QMouseEvent* e)
{
    if (!(e->buttons() & Qt::LeftButton)) return;
    QPointF pos = QPointF(e->pos());
    if (m_drag == DragZone::Ring)
        handleRingDrag(pos);
    else if (m_drag == DragZone::Triangle)
        handleTriangleDrag(pos);
}

void ColorWheel::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        m_drag = DragZone::None;
}
