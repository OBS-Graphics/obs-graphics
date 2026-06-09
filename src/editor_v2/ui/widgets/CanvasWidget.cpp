#include "CanvasWidget.h"

#include <algorithm>
#include <cmath>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include "engine/element.h"
#include "engine/graphic.h"
#include "engine/json.hpp"
#include "engine/scene.h"

#include "model/SceneDocument.h"
#include "model/EditorScene.h"
#include "model/UndoCommands.h"

#include "SelectionHandles.h"

static constexpr int    kArrowMergeTag = 9999;

static Rectangle globalBounds(const Element& el)
{
    auto pos = el.GetGlobalPosition();
    return { pos.x, pos.y, el.bounds.width, el.bounds.height };
}
static constexpr double kZoomMin = 0.05;
static constexpr double kZoomMax = 30.0;
static constexpr double kZoomStep = 1.12;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

CanvasWidget::CanvasWidget(SceneDocument* doc,
                           EditorScene*   editorState,
                           QWidget*       parent)
    : QWidget(parent)
    , m_doc(doc)
    , m_editorState(editorState)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setFocusPolicy(Qt::StrongFocus);

    setupCairo();

    connect(m_doc,        &SceneDocument::documentChanged,
            this,         &CanvasWidget::onDocumentChanged);
    connect(m_editorState, &EditorScene::selectionChanged,
            this,          &CanvasWidget::onSelectionChanged);

    renderStaticScene();
}

CanvasWidget::~CanvasWidget()
{
    if (m_animTimer)
        m_animTimer->stop();
    m_image = QImage{};
    teardownCairo();
}

// ── Scene dimension helpers ───────────────────────────────────────────────────

int CanvasWidget::sceneW() const { return m_doc ? m_doc->scene().width  : 1920; }
int CanvasWidget::sceneH() const { return m_doc ? m_doc->scene().height : 1080; }

// ── Cairo setup/teardown ──────────────────────────────────────────────────────

void CanvasWidget::setupCairo()
{
    const int w = sceneW(), h = sceneH();
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
    m_cr      = cairo_create(m_surface);
    m_image   = QImage(cairo_image_surface_get_data(m_surface),
                       w, h,
                       cairo_image_surface_get_stride(m_surface),
                       QImage::Format_ARGB32_Premultiplied);
}

void CanvasWidget::teardownCairo()
{
    if (m_cr)      { cairo_destroy(m_cr);          m_cr      = nullptr; }
    if (m_surface) { cairo_surface_destroy(m_surface); m_surface = nullptr; }
}

// ── Coordinate helpers ────────────────────────────────────────────────────────

QRectF CanvasWidget::letterboxRect() const
{
    const double wW = width(), wH = height();
    const double base = std::min(wW / sceneW(), wH / sceneH()) * m_zoom;
    const double dW = sceneW() * base, dH = sceneH() * base;
    return { (wW - dW) / 2.0 + m_panOffset.x(),
             (wH - dH) / 2.0 + m_panOffset.y(),
             dW, dH };
}

QPointF CanvasWidget::widgetToScene(QPointF pt) const
{
    const QRectF lb = letterboxRect();
    if (lb.width() <= 0.0 || lb.height() <= 0.0)
        return {};
    return { (pt.x() - lb.left()) / lb.width()  * sceneW(),
             (pt.y() - lb.top())  / lb.height() * sceneH() };
}

QRectF CanvasWidget::sceneToWidget(const Rectangle& r) const
{
    const QRectF lb = letterboxRect();
    const double sx = lb.width()  / sceneW();
    const double sy = lb.height() / sceneH();
    return { lb.left() + r.x      * sx,
             lb.top()  + r.y      * sy,
             r.width  * sx,
             r.height * sy };
}

// ── Zoom helpers ──────────────────────────────────────────────────────────────

void CanvasWidget::zoomToward(QPointF cursor, double factor)
{
    const QRectF lb = letterboxRect();
    // Scene point under cursor stays fixed
    const double sx = lb.width()  > 0 ? (cursor.x() - lb.left()) / lb.width()  * sceneW() : 0;
    const double sy = lb.height() > 0 ? (cursor.y() - lb.top())  / lb.height() * sceneH() : 0;

    m_zoom = std::clamp(m_zoom * factor, kZoomMin, kZoomMax);

    const double wW = width(), wH = height();
    const double base = std::min(wW / sceneW(), wH / sceneH());
    const double dW = sceneW() * base * m_zoom;
    const double dH = sceneH() * base * m_zoom;

    m_panOffset.setX(cursor.x() - sx * dW / sceneW() - (wW - dW) / 2.0);
    m_panOffset.setY(cursor.y() - sy * dH / sceneH() - (wH - dH) / 2.0);
    update();
}

void CanvasWidget::zoomIn()      { zoomToward(rect().center(), 1.25); }
void CanvasWidget::zoomOut()     { zoomToward(rect().center(), 1.0 / 1.25); }
void CanvasWidget::fitToWindow() { m_zoom = 1.0; m_panOffset = {}; update(); }

// ── Snapping ──────────────────────────────────────────────────────────────────

void CanvasWidget::setSnapping(bool on)
{
    m_snappingEnabled = on;
    if (!on) { m_snapLinesX.clear(); m_snapLinesY.clear(); }
}

void CanvasWidget::appendGuideCandidates(QList<double>& candX, QList<double>& candY) const
{
    const double W = sceneW(), H = sceneH();
    if (m_guideFlags & GuideRuleOfThirds) {
        candX << W / 3.0 << 2.0 * W / 3.0;
        candY << H / 3.0 << 2.0 * H / 3.0;
    }
    if (m_guideFlags & GuideTitleSafe) {
        candX << W * 0.05 << W * 0.95;
        candY << H * 0.05 << H * 0.95;
    }
    if (m_guideFlags & GuideActionSafe) {
        candX << W * 0.10 << W * 0.90;
        candY << H * 0.10 << H * 0.90;
    }
}

Rectangle CanvasWidget::applySnapping(Rectangle b, int gi, int ei)
{
    m_snapLinesX.clear();
    m_snapLinesY.clear();

    const double threshold = 8.0 * sceneW() / letterboxRect().width();

    // Candidate snap lines — all in global scene space
    QList<double> candX = { 0.0, sceneW() / 2.0, double(sceneW()) };
    QList<double> candY = { 0.0, sceneH() / 2.0, double(sceneH()) };
    appendGuideCandidates(candX, candY);

    const Scene& scene = m_doc->scene();
    for (int ggi = 0; ggi < (int)scene.graphics.size(); ++ggi) {
        for (int eei = 0; eei < (int)scene.graphics[ggi].elements.size(); ++eei) {
            if (ggi == gi && eei == ei) continue;
            const Rectangle ob = globalBounds(scene.graphics[ggi].elements[eei]);
            candX << ob.x << ob.x + ob.width / 2.0 << ob.x + ob.width;
            candY << ob.y << ob.y + ob.height / 2.0 << ob.y + ob.height;
        }
    }

    // Convert b (local) to global for anchor computation
    double parentOffsetX = 0.0, parentOffsetY = 0.0;
    if (gi >= 0 && gi < (int)scene.graphics.size() &&
        ei >= 0 && ei < (int)scene.graphics[gi].elements.size()) {
        const Element& el = scene.graphics[gi].elements[ei];
        if (el.parent) {
            Point pg = el.parent->GetGlobalPosition();
            parentOffsetX = pg.x;
            parentOffsetY = pg.y;
        }
    }
    Rectangle gb = b;
    gb.x += parentOffsetX;
    gb.y += parentOffsetY;

    const double anchX[3] = { gb.x, gb.x + gb.width / 2.0, gb.x + gb.width };
    const double anchY[3] = { gb.y, gb.y + gb.height / 2.0, gb.y + gb.height };

    auto bestSnap = [&](const double anchors[], const QList<double>& cands,
                        double& outDelta, double& outLine) -> bool {
        double best = threshold;
        bool found = false;
        for (double a : { anchors[0], anchors[1], anchors[2] }) {
            for (double c : cands) {
                double d = std::abs(a - c);
                if (d < best) {
                    best     = d;
                    outDelta = c - a;
                    outLine  = c;
                    found    = true;
                }
            }
        }
        return found;
    };

    double dX = 0, snapX = 0, dY = 0, snapY = 0;
    if (bestSnap(anchX, candX, dX, snapX)) {
        gb.x += dX;
        m_snapLinesX << snapX;
    }
    if (bestSnap(anchY, candY, dY, snapY)) {
        gb.y += dY;
        m_snapLinesY << snapY;
    }

    b.x = gb.x - parentOffsetX;
    b.y = gb.y - parentOffsetY;
    return b;
}

Rectangle CanvasWidget::graphicSceneBounds(int gi) const
{
    const Scene& scene = m_doc->scene();
    if (gi < 0 || gi >= (int)scene.graphics.size()) return {};
    const Graphic& g = scene.graphics[gi];
    if (g.elements.empty()) return {};

    double minX =  1e9, minY =  1e9;
    double maxX = -1e9, maxY = -1e9;
    for (const auto& el : g.elements) {
        const Rectangle gb = globalBounds(el);
        minX = std::min(minX, gb.x);
        minY = std::min(minY, gb.y);
        maxX = std::max(maxX, gb.x + gb.width);
        maxY = std::max(maxY, gb.y + gb.height);
    }
    return { minX, minY, maxX - minX, maxY - minY };
}

// Snap a single edge (for resize handles)
static double snapEdge(double edge, const QList<double>& cands, double threshold)
{
    double best = threshold, result = edge;
    for (double c : cands) {
        double d = std::abs(edge - c);
        if (d < best) { best = d; result = c; }
    }
    return result;
}

// ── Hit testing ───────────────────────────────────────────────────────────────

SelectionId CanvasWidget::hitTest(QPointF scenePt) const
{
    const Scene& scene = m_doc->scene();

    std::vector<int> gOrder;
    gOrder.reserve(scene.graphics.size());
    for (int gi = 0; gi < (int)scene.graphics.size(); ++gi) gOrder.push_back(gi);
    std::stable_sort(gOrder.begin(), gOrder.end(), [&](int a, int b) {
        return scene.graphics[a].zOrder > scene.graphics[b].zOrder;
    });

    for (int gi : gOrder) {
        const Graphic& g = scene.graphics[gi];
        std::vector<int> eOrder;
        eOrder.reserve(g.elements.size());
        for (int ei = 0; ei < (int)g.elements.size(); ++ei) eOrder.push_back(ei);
        std::stable_sort(eOrder.begin(), eOrder.end(), [&](int a, int b) {
            return g.elements[a].zOrder > g.elements[b].zOrder;
        });

        for (int ei : eOrder) {
            const Rectangle gb = globalBounds(g.elements[ei]);
            if (scenePt.x() >= gb.x && scenePt.x() <= gb.x + gb.width &&
                scenePt.y() >= gb.y && scenePt.y() <= gb.y + gb.height)
                return { SelectionId::Level::Element, gi, ei };
        }
    }
    return {};
}

int CanvasWidget::hitHandle(QPointF widgetPt) const
{
    const SelectionId sel = m_editorState->selection();
    if (sel.level != SelectionId::Level::Element) return -1;
    const Scene& scene = m_doc->scene();
    if (sel.graphicIndex < 0 || sel.graphicIndex >= (int)scene.graphics.size()) return -1;
    const Graphic& g = scene.graphics[sel.graphicIndex];
    if (sel.elementIndex < 0 || sel.elementIndex >= (int)g.elements.size()) return -1;
    return SelectionHandles::hitTest(widgetPt, sceneToWidget(globalBounds(g.elements[sel.elementIndex])));
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void CanvasWidget::renderStaticScene()
{
    if (!m_cr || !m_surface) return;

    cairo_save(m_cr);
    cairo_set_operator(m_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cr);
    cairo_restore(m_cr);

    // Force all graphics visible (editor display — bypass state machine)
    // Setting timer=1e9 ensures EvaluateAnimation returns t=1 (at-rest position)
    Scene& s = const_cast<Scene&>(m_doc->scene());
    for (auto& g : s.graphics) {
        g.state = GraphicState::Visible;
        g.timer = 1e9;
    }

    cairo_set_antialias(m_cr, CAIRO_ANTIALIAS_BEST);

    s.Render(m_cr);
    cairo_surface_flush(m_surface);
    update();
}

void CanvasWidget::renderPreviewScene()
{
    if (!m_cr || !m_surface || !m_previewScene) return;

    cairo_save(m_cr);
    cairo_set_operator(m_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cr);
    cairo_restore(m_cr);

    m_previewScene->Render(m_cr);
    cairo_surface_flush(m_surface);
    update();
}

// ── Overlay helpers ───────────────────────────────────────────────────────────

void CanvasWidget::setGuides(GuideFlags flags) { m_guideFlags = flags; update(); }

void CanvasWidget::drawCheckerboard(QPainter& p, const QRectF& lb)
{
    static constexpr int sz = 10;
    static QPixmap checker;
    if (checker.isNull()) {
        checker = QPixmap(sz * 2, sz * 2);
        QPainter cp(&checker);
        cp.fillRect(0,  0,  sz, sz, QColor(72, 72, 72));
        cp.fillRect(sz, sz, sz, sz, QColor(72, 72, 72));
        cp.fillRect(sz, 0,  sz, sz, QColor(56, 56, 56));
        cp.fillRect(0,  sz, sz, sz, QColor(56, 56, 56));
    }
    p.save();
    p.setClipRect(lb);
    p.drawTiledPixmap(lb.toRect(), checker);
    p.restore();
}

void CanvasWidget::drawGuides(QPainter& p, const QRectF& lb)
{
    if (m_guideFlags == GuideNone) return;
    p.save();
    p.setClipRect(lb);

    if (m_guideFlags & GuideRuleOfThirds) {
        p.setPen(QPen(QColor(255, 255, 255, 55), 1.0));
        for (int i = 1; i <= 2; ++i) {
            double x = lb.left() + lb.width()  * i / 3.0;
            double y = lb.top()  + lb.height() * i / 3.0;
            p.drawLine(QPointF(x, lb.top()),   QPointF(x, lb.bottom()));
            p.drawLine(QPointF(lb.left(), y),  QPointF(lb.right(), y));
        }
    }
    if (m_guideFlags & GuideCenterLines) {
        p.setPen(QPen(QColor(255, 255, 255, 38), 1.0, Qt::DashLine));
        double cx = lb.center().x(), cy = lb.center().y();
        p.drawLine(QPointF(cx, lb.top()),   QPointF(cx, lb.bottom()));
        p.drawLine(QPointF(lb.left(), cy),  QPointF(lb.right(), cy));
    }
    if (m_guideFlags & GuideTitleSafe) {
        double mx = lb.width() * 0.05, my = lb.height() * 0.05;
        p.setPen(QPen(QColor(255, 210, 50, 80), 1.0));
        p.drawRect(lb.adjusted(mx, my, -mx, -my));
    }
    if (m_guideFlags & GuideActionSafe) {
        double mx = lb.width() * 0.10, my = lb.height() * 0.10;
        p.setPen(QPen(QColor(255, 100, 50, 80), 1.0));
        p.drawRect(lb.adjusted(mx, my, -mx, -my));
    }
    p.restore();
}

void CanvasWidget::drawSnapLines(QPainter& p, const QRectF& lb)
{
    if (m_snapLinesX.isEmpty() && m_snapLinesY.isEmpty()) return;
    p.save();
    p.setClipRect(lb);
    p.setPen(QPen(QColor(0, 150, 255, 210), 1.0));
    for (double sx : m_snapLinesX) {
        double wx = lb.left() + sx / sceneW() * lb.width();
        p.drawLine(QPointF(wx, lb.top()), QPointF(wx, lb.bottom()));
    }
    for (double sy : m_snapLinesY) {
        double wy = lb.top() + sy / sceneH() * lb.height();
        p.drawLine(QPointF(lb.left(), wy), QPointF(lb.right(), wy));
    }
    p.restore();
}

// ── paintEvent ────────────────────────────────────────────────────────────────

void CanvasWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(38, 38, 38));

    const QRectF lb = letterboxRect();
    drawCheckerboard(p, lb);

    if (!m_image.isNull())
        p.drawImage(lb, m_image);

    // Subtle scene border
    p.setPen(QPen(QColor(0, 0, 0, 120), 1));
    p.drawLine(lb.topLeft(),    lb.topRight());
    p.drawLine(lb.topLeft(),    lb.bottomLeft());
    p.setPen(QPen(QColor(100, 100, 100, 140), 1));
    p.drawLine(lb.topRight(),   lb.bottomRight());
    p.drawLine(lb.bottomLeft(), lb.bottomRight());

    drawGuides(p, lb);

    if (m_dragging)
        drawSnapLines(p, lb);

    // Selection overlay
    if (!m_previewScene) {
        const SelectionId sel = m_editorState->selection();
        if (sel.level == SelectionId::Level::Element) {
            const Scene& scene = m_doc->scene();
            if (sel.graphicIndex >= 0 && sel.graphicIndex < (int)scene.graphics.size()) {
                const Graphic& g = scene.graphics[sel.graphicIndex];
                if (sel.elementIndex >= 0 && sel.elementIndex < (int)g.elements.size())
                    SelectionHandles::draw(p,
                        sceneToWidget(globalBounds(g.elements[sel.elementIndex])),
                        m_dragging ? m_dragHandle : -1,
                        palette().highlight().color());
            }
        } else if (sel.level == SelectionId::Level::Graphic) {
            const Scene& scene = m_doc->scene();
            if (sel.graphicIndex >= 0 && sel.graphicIndex < (int)scene.graphics.size() &&
                !scene.graphics[sel.graphicIndex].elements.empty()) {
                const QRectF gRect = sceneToWidget(graphicSceneBounds(sel.graphicIndex));
                p.save();
                p.setRenderHint(QPainter::Antialiasing, false);
                p.setPen(QPen(QColor(0, 0, 0, 100), 3.0));
                p.setBrush(Qt::NoBrush);
                p.drawRect(gRect);
                p.setPen(QPen(QColor(80, 200, 80), 1.5, Qt::DashLine));
                p.drawRect(gRect);
                p.restore();
            }
        }
    }
}

void CanvasWidget::resizeEvent(QResizeEvent*) { update(); }

// ── Slots ─────────────────────────────────────────────────────────────────────

void CanvasWidget::onDocumentChanged()
{
    // Recreate Cairo surface if scene dimensions changed
    if (m_surface) {
        int w = sceneW(), h = sceneH();
        if (cairo_image_surface_get_width(m_surface)  != w ||
            cairo_image_surface_get_height(m_surface) != h) {
            m_image = QImage{};
            teardownCairo();
            setupCairo();
        }
    }
    if (!m_previewScene) renderStaticScene();
}

void CanvasWidget::onSelectionChanged(SelectionId) { update(); }

void CanvasWidget::onAnimTick()
{
    if (!m_previewScene) return;
    const float delta = static_cast<float>(m_elapsedTimer.restart()) / 1000.0f;
    m_previewScene->Tick(delta);
    renderPreviewScene();
}

// ── Animation preview ─────────────────────────────────────────────────────────

void CanvasWidget::startAnimationPreview(int graphicIndex, bool playIn)
{
    stopAnimationPreview();
    const nlohmann::json j = SceneDocument::sceneToJson(m_doc->scene());
    m_previewScene = std::make_unique<Scene>(Scene::LoadString(j.dump()));

    if (graphicIndex >= 0 && graphicIndex < (int)m_previewScene->graphics.size()) {
        if (playIn) m_previewScene->graphics[graphicIndex].TriggerIn();
        else        m_previewScene->graphics[graphicIndex].TriggerOut();
    }

    m_elapsedTimer.start();
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, &CanvasWidget::onAnimTick);
    m_animTimer->start();
}

void CanvasWidget::stopAnimationPreview()
{
    if (m_animTimer) {
        m_animTimer->stop();
        m_animTimer->deleteLater();
        m_animTimer = nullptr;
    }
    m_previewScene.reset();
    renderStaticScene();
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void CanvasWidget::keyPressEvent(QKeyEvent* event)
{
    const SelectionId sel = m_editorState->selection();
    if (m_previewScene ||
        (sel.level != SelectionId::Level::Element &&
         sel.level != SelectionId::Level::Graphic)) {
        QWidget::keyPressEvent(event);
        return;
    }

    double dx = 0, dy = 0;
    const double step = (event->modifiers() & Qt::ShiftModifier) ? 10.0 : 1.0;
    switch (event->key()) {
    case Qt::Key_Left:  dx = -step; break;
    case Qt::Key_Right: dx = +step; break;
    case Qt::Key_Up:    dy = -step; break;
    case Qt::Key_Down:  dy = +step; break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }

    const Scene& scene = m_doc->scene();
    const int gi = sel.graphicIndex;
    if (gi < 0 || gi >= (int)scene.graphics.size()) return;

    if (sel.level == SelectionId::Level::Graphic) {
        const Graphic& g = scene.graphics[gi];
        const std::string gid = g.id;
        m_doc->undoStack()->beginMacro("Nudge Graphic");
        for (const auto& el : g.elements) {
            Rectangle b = el.bounds;
            b.x += dx;
            b.y += dy;
            m_doc->undoStack()->push(new SetElementFieldCmd<Rectangle>(
                m_doc, gid, el.id, b,
                [](Element& e) -> Rectangle& { return e.bounds; }, "nudge"));
        }
        m_doc->undoStack()->endMacro();
    } else {
        const int ei = sel.elementIndex;
        if (ei < 0 || ei >= (int)scene.graphics[gi].elements.size()) return;
        Rectangle b = scene.graphics[gi].elements[ei].bounds;
        b.x += dx;
        b.y += dy;
        const std::string gid = scene.graphics[gi].id;
        const std::string eid = scene.graphics[gi].elements[ei].id;
        m_doc->undoStack()->push(new SetElementFieldCmd<Rectangle>(
            m_doc, gid, eid, b,
            [](Element& e) -> Rectangle& { return e.bounds; },
            "nudge", kArrowMergeTag));
    }

    event->accept();
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void CanvasWidget::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = event->angleDelta().y() > 0 ? kZoomStep : 1.0 / kZoomStep;
        zoomToward(event->position(), factor);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void CanvasWidget::mousePressEvent(QMouseEvent* event)
{
    // Middle-button pan
    if (event->button() == Qt::MiddleButton) {
        m_panning        = true;
        m_panStart       = event->position();
        m_panStartOffset = m_panOffset;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton || m_previewScene) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF wpos = event->position();

    // 1. Resize handle hit?
    const int handle = hitHandle(wpos);
    if (handle >= 0) {
        const SelectionId sel = m_editorState->selection();
        const Rectangle& bounds =
            m_doc->scene().graphics[sel.graphicIndex].elements[sel.elementIndex].bounds;
        m_dragMode        = DragMode::Resize;
        m_dragHandle      = handle;
        m_dragStartWidget = wpos;
        m_dragStartScene  = widgetToScene(wpos);
        m_dragOrigBounds  = bounds;
        m_dragGi          = sel.graphicIndex;
        m_dragEi          = sel.elementIndex;
        m_dragging        = true;
        return;
    }

    const SelectionId curSel = m_editorState->selection();
    const QPointF sp = widgetToScene(wpos);

    // 2. Graphic selected → drag whole group if click is inside the bounding box
    if (curSel.level == SelectionId::Level::Graphic) {
        const Scene& scene = m_doc->scene();
        if (curSel.graphicIndex >= 0 && curSel.graphicIndex < (int)scene.graphics.size()) {
            const Graphic& g = scene.graphics[curSel.graphicIndex];
            if (!g.elements.empty()) {
                const QRectF gRect = sceneToWidget(graphicSceneBounds(curSel.graphicIndex));
                if (gRect.contains(wpos)) {
                    m_graphicOrigBounds.clear();
                    for (const auto& el : g.elements)
                        m_graphicOrigBounds.push_back(el.bounds);
                    m_dragMode       = DragMode::MoveGraphic;
                    m_dragHandle     = -1;
                    m_dragStartScene = sp;
                    m_dragGi         = curSel.graphicIndex;
                    m_dragging       = true;
                    return;
                }
            }
        }
    }

    // 3. Inside already-selected element → move
    if (curSel.level == SelectionId::Level::Element) {
        const Scene& scene = m_doc->scene();
        if (curSel.graphicIndex >= 0 && curSel.graphicIndex < (int)scene.graphics.size()) {
            const Graphic& g = scene.graphics[curSel.graphicIndex];
            if (curSel.elementIndex >= 0 && curSel.elementIndex < (int)g.elements.size()) {
                const Rectangle gb = globalBounds(g.elements[curSel.elementIndex]);
                if (sp.x() >= gb.x && sp.x() <= gb.x + gb.width &&
                    sp.y() >= gb.y && sp.y() <= gb.y + gb.height) {
                    m_dragMode        = DragMode::Move;
                    m_dragHandle      = -1;
                    m_dragStartWidget = wpos;
                    m_dragStartScene  = sp;
                    m_dragOrigBounds  = g.elements[curSel.elementIndex].bounds;
                    m_dragGi          = curSel.graphicIndex;
                    m_dragEi          = curSel.elementIndex;
                    m_dragging        = true;
                    return;
                }
            }
        }
    }

    // 4. Hit-test for new selection
    const QPointF scenePt = widgetToScene(wpos);
    const SelectionId hit = hitTest(scenePt);
    m_editorState->setSelection(hit);

    if (hit.level == SelectionId::Level::Element) {
        const Rectangle& b =
            m_doc->scene().graphics[hit.graphicIndex].elements[hit.elementIndex].bounds;
        m_dragMode        = DragMode::Move;
        m_dragHandle      = -1;
        m_dragStartWidget = wpos;
        m_dragStartScene  = scenePt;
        m_dragOrigBounds  = b;
        m_dragGi          = hit.graphicIndex;
        m_dragEi          = hit.elementIndex;
        m_dragging        = true;
    }
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event)
{
    // Middle-button pan
    if (m_panning) {
        m_panOffset = m_panStartOffset + (event->position() - m_panStart);
        update();
        return;
    }

    if (!m_dragging || m_previewScene) {
        if (!m_previewScene)
            updateCursorForPos(event->position());
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPointF wpos    = event->position();
    const QPointF scenePt = widgetToScene(wpos);

    if (m_dragMode == DragMode::MoveGraphic) {
        const double dx  = scenePt.x() - m_dragStartScene.x();
        const double dy  = scenePt.y() - m_dragStartScene.y();
        const int    gi  = m_dragGi;
        const auto   orig = m_graphicOrigBounds;
        m_doc->applyMutation([gi, dx, dy, orig](Scene& s) {
            if (gi >= (int)s.graphics.size()) return;
            Graphic& g = s.graphics[gi];
            for (int ei = 0; ei < (int)g.elements.size() && ei < (int)orig.size(); ++ei) {
                g.elements[ei].bounds.x = orig[ei].x + dx;
                g.elements[ei].bounds.y = orig[ei].y + dy;
            }
        });
        return;
    }

    Rectangle newBounds = m_dragOrigBounds;
    const double ox = m_dragOrigBounds.x,     oy = m_dragOrigBounds.y;
    const double ow = m_dragOrigBounds.width, oh = m_dragOrigBounds.height;

    if (m_dragMode == DragMode::Move) {
        newBounds.x = ox + (scenePt.x() - m_dragStartScene.x());
        newBounds.y = oy + (scenePt.y() - m_dragStartScene.y());

    } else {  // Resize
        const double dx = scenePt.x() - m_dragStartScene.x();
        const double dy = scenePt.y() - m_dragStartScene.y();
        switch (m_dragHandle) {
        case 0: newBounds.x=ox+dx; newBounds.y=oy+dy; newBounds.width=std::max(1.0,ow-dx); newBounds.height=std::max(1.0,oh-dy); break;
        case 1: newBounds.y=oy+dy; newBounds.height=std::max(1.0,oh-dy); break;
        case 2: newBounds.y=oy+dy; newBounds.width=std::max(1.0,ow+dx); newBounds.height=std::max(1.0,oh-dy); break;
        case 3: newBounds.x=ox+dx; newBounds.width=std::max(1.0,ow-dx); break;
        case 4: newBounds.width=std::max(1.0,ow+dx); break;
        case 5: newBounds.x=ox+dx; newBounds.width=std::max(1.0,ow-dx); newBounds.height=std::max(1.0,oh+dy); break;
        case 6: newBounds.height=std::max(1.0,oh+dy); break;
        case 7: newBounds.width=std::max(1.0,ow+dx); newBounds.height=std::max(1.0,oh+dy); break;
        default: break;
        }

        // Shift: maintain original aspect ratio (corners only)
        if ((event->modifiers() & Qt::ShiftModifier) && oh > 0 &&
            (m_dragHandle==0 || m_dragHandle==2 || m_dragHandle==5 || m_dragHandle==7)) {
            // Use the dimension that changed proportionally more
            const double wScale = newBounds.width  / ow;
            const double hScale = newBounds.height / oh;
            const double scale  = std::min(wScale, hScale);
            newBounds.width  = std::max(1.0, ow * scale);
            newBounds.height = std::max(1.0, oh * scale);
            // Reposition so the opposite corner stays fixed
            switch (m_dragHandle) {
            case 0: newBounds.x = ox + ow - newBounds.width; newBounds.y = oy + oh - newBounds.height; break;
            case 2: newBounds.x = ox;                         newBounds.y = oy + oh - newBounds.height; break;
            case 5: newBounds.x = ox + ow - newBounds.width; newBounds.y = oy;                          break;
            case 7: newBounds.x = ox;                         newBounds.y = oy;                          break;
            default: break;
            }
        }

        // Ctrl: scale from center (size change is doubled, opposite edge also moves)
        if (event->modifiers() & Qt::ControlModifier) {
            const double cx = ox + ow / 2.0;
            const double cy = oy + oh / 2.0;
            newBounds.width  = std::max(1.0, 2.0 * newBounds.width  - ow);
            newBounds.height = std::max(1.0, 2.0 * newBounds.height - oh);
            newBounds.x      = cx - newBounds.width  / 2.0;
            newBounds.y      = cy - newBounds.height / 2.0;
        }
    }

    // Apply snapping
    if (m_snappingEnabled) {
        if (m_dragMode == DragMode::Move) {
            newBounds = applySnapping(newBounds, m_dragGi, m_dragEi);
        } else {
            // Snap only the edge(s) being dragged
            const double threshold = 8.0 * sceneW() / letterboxRect().width();
            QList<double> candX = { 0.0, sceneW() / 2.0, double(sceneW()) };
            QList<double> candY = { 0.0, sceneH() / 2.0, double(sceneH()) };
            appendGuideCandidates(candX, candY);
            const Scene& scene = m_doc->scene();
            for (int ggi = 0; ggi < (int)scene.graphics.size(); ++ggi) {
                for (int eei = 0; eei < (int)scene.graphics[ggi].elements.size(); ++eei) {
                    if (ggi == m_dragGi && eei == m_dragEi) continue;
                    const Rectangle& ob = scene.graphics[ggi].elements[eei].bounds;
                    candX << ob.x << ob.x + ob.width / 2.0 << ob.x + ob.width;
                    candY << ob.y << ob.y + ob.height / 2.0 << ob.y + ob.height;
                }
            }

            m_snapLinesX.clear();
            m_snapLinesY.clear();

            auto snapX = [&](double& edge) {
                double s = snapEdge(edge, candX, threshold);
                if (s != edge) { m_snapLinesX << s; edge = s; }
            };
            auto snapY = [&](double& edge) {
                double s = snapEdge(edge, candY, threshold);
                if (s != edge) { m_snapLinesY << s; edge = s; }
            };

            switch (m_dragHandle) {
            case 0: { snapX(newBounds.x); newBounds.width = ox+ow - newBounds.x; snapY(newBounds.y); newBounds.height = oy+oh - newBounds.y; break; }
            case 1: { snapY(newBounds.y); newBounds.height = oy+oh - newBounds.y; break; }
            case 2: { double r=newBounds.x+newBounds.width; snapX(r); newBounds.width=r-newBounds.x; snapY(newBounds.y); newBounds.height=oy+oh-newBounds.y; break; }
            case 3: { snapX(newBounds.x); newBounds.width = ox+ow - newBounds.x; break; }
            case 4: { double r=newBounds.x+newBounds.width; snapX(r); newBounds.width=r-newBounds.x; break; }
            case 5: { snapX(newBounds.x); newBounds.width=ox+ow-newBounds.x; double b2=newBounds.y+newBounds.height; snapY(b2); newBounds.height=b2-newBounds.y; break; }
            case 6: { double b2=newBounds.y+newBounds.height; snapY(b2); newBounds.height=b2-newBounds.y; break; }
            case 7: { double r=newBounds.x+newBounds.width; snapX(r); newBounds.width=r-newBounds.x; double b2=newBounds.y+newBounds.height; snapY(b2); newBounds.height=b2-newBounds.y; break; }
            default: break;
            }
        }
    } else {
        m_snapLinesX.clear();
        m_snapLinesY.clear();
    }

    const int gi = m_dragGi, ei = m_dragEi;
    m_doc->applyMutation([gi, ei, newBounds](Scene& s) {
        if (gi < (int)s.graphics.size()) {
            Graphic& g = s.graphics[gi];
            if (ei < (int)g.elements.size())
                g.elements[ei].bounds = newBounds;
        }
    });
}

void CanvasWidget::mouseReleaseEvent(QMouseEvent* event)
{
    // Middle-button pan end
    if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton || !m_dragging || m_previewScene) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_dragging = false;
    m_snapLinesX.clear();
    m_snapLinesY.clear();

    if (m_dragMode == DragMode::MoveGraphic) {
        const Scene& scene = m_doc->scene();
        const int gi = m_dragGi;
        if (gi >= 0 && gi < (int)scene.graphics.size()) {
            const Graphic& g = scene.graphics[gi];
            std::vector<Rectangle> finalBounds;
            finalBounds.reserve(g.elements.size());
            for (const auto& el : g.elements) finalBounds.push_back(el.bounds);

            bool moved = false;
            for (int ei = 0; ei < (int)finalBounds.size() && ei < (int)m_graphicOrigBounds.size(); ++ei) {
                if (finalBounds[ei].x != m_graphicOrigBounds[ei].x ||
                    finalBounds[ei].y != m_graphicOrigBounds[ei].y) { moved = true; break; }
            }

            if (moved) {
                const auto orig = m_graphicOrigBounds;
                m_doc->applyMutation([gi, orig](Scene& s) {
                    Graphic& g = s.graphics[gi];
                    for (int ei = 0; ei < (int)g.elements.size() && ei < (int)orig.size(); ++ei)
                        g.elements[ei].bounds = orig[ei];
                });
                const std::string gid = g.id;
                m_doc->undoStack()->beginMacro("Move Graphic");
                for (int ei = 0; ei < (int)g.elements.size() && ei < (int)finalBounds.size(); ++ei) {
                    const std::string eid = g.elements[ei].id;
                    const Rectangle fb = finalBounds[ei];
                    m_doc->undoStack()->push(new SetElementFieldCmd<Rectangle>(
                        m_doc, gid, eid, fb,
                        [](Element& e) -> Rectangle& { return e.bounds; }, "bounds"));
                }
                m_doc->undoStack()->endMacro();
            }
        }
        m_dragMode = DragMode::None;
        m_dragGi   = -1;
        m_graphicOrigBounds.clear();
        return;
    }

    const Scene& scene = m_doc->scene();
    if (m_dragGi < 0 || m_dragGi >= (int)scene.graphics.size()) { m_dragMode = DragMode::None; return; }
    const Graphic& g = scene.graphics[m_dragGi];
    if (m_dragEi < 0 || m_dragEi >= (int)g.elements.size())     { m_dragMode = DragMode::None; return; }

    const Rectangle finalBounds = g.elements[m_dragEi].bounds;

    if (finalBounds.x     != m_dragOrigBounds.x     ||
        finalBounds.y     != m_dragOrigBounds.y     ||
        finalBounds.width != m_dragOrigBounds.width ||
        finalBounds.height!= m_dragOrigBounds.height) {

        const int gi = m_dragGi, ei = m_dragEi;
        const std::string gid = scene.graphics[gi].id;
        const std::string eid = scene.graphics[gi].elements[ei].id;
        auto* cmd = new SetElementFieldCmd<Rectangle>(
            m_doc, gid, eid, finalBounds,
            [](Element& e) -> Rectangle& { return e.bounds; }, "bounds");

        // Revert to original so cmd->redo() applies the final value cleanly
        m_doc->applyMutation([gi, ei, orig = m_dragOrigBounds](Scene& s) {
            if (gi < (int)s.graphics.size()) {
                Graphic& g = s.graphics[gi];
                if (ei < (int)g.elements.size())
                    g.elements[ei].bounds = orig;
            }
        });
        m_doc->undoStack()->push(cmd);
    }

    m_dragMode   = DragMode::None;
    m_dragHandle = -1;
    m_dragGi     = -1;
    m_dragEi     = -1;
    updateCursorForPos(event->position());
}

void CanvasWidget::leaveEvent(QEvent* event)
{
    unsetCursor();
    QWidget::leaveEvent(event);
}

void CanvasWidget::updateCursorForPos(QPointF wpos)
{
    static const Qt::CursorShape kHandleCursors[8] = {
        Qt::SizeFDiagCursor, Qt::SizeVerCursor,  Qt::SizeBDiagCursor,
        Qt::SizeHorCursor,   Qt::SizeHorCursor,
        Qt::SizeBDiagCursor, Qt::SizeVerCursor,  Qt::SizeFDiagCursor
    };

    const int handle = hitHandle(wpos);
    if (handle >= 0) {
        setCursor(kHandleCursors[handle]);
        return;
    }

    const SelectionId sel = m_editorState->selection();
    if (sel.level == SelectionId::Level::Element) {
        const QPointF sp = widgetToScene(wpos);
        const Scene& scene = m_doc->scene();
        if (sel.graphicIndex >= 0 && sel.graphicIndex < (int)scene.graphics.size()) {
            const Graphic& g = scene.graphics[sel.graphicIndex];
            if (sel.elementIndex >= 0 && sel.elementIndex < (int)g.elements.size()) {
                const Rectangle gb = globalBounds(g.elements[sel.elementIndex]);
                if (sp.x() >= gb.x && sp.x() <= gb.x + gb.width &&
                    sp.y() >= gb.y && sp.y() <= gb.y + gb.height) {
                    setCursor(Qt::SizeAllCursor);
                    return;
                }
            }
        }
    } else if (sel.level == SelectionId::Level::Graphic) {
        const QRectF gRect = sceneToWidget(graphicSceneBounds(sel.graphicIndex));
        if (gRect.contains(wpos)) {
            setCursor(Qt::SizeAllCursor);
            return;
        }
    }

    unsetCursor();
}
