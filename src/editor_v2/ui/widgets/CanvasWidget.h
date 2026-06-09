#pragma once

#include <memory>

#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <cairo/cairo.h>

#include "engine/types.hpp"
#include "model/EditorScene.h"

class QTimer;
class QKeyEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QMouseEvent;
class QWheelEvent;

class SceneDocument;
class EditorScene;
struct Scene;

class CanvasWidget : public QWidget {
    Q_OBJECT

public:
    enum GuideFlag {
        GuideNone = 0,
        GuideRuleOfThirds = 1 << 0,
        GuideCenterLines = 1 << 1,
        GuideTitleSafe = 1 << 2,
        GuideActionSafe = 1 << 3,
    };
    Q_DECLARE_FLAGS(GuideFlags, GuideFlag)

    explicit CanvasWidget(SceneDocument* doc, EditorScene* editorState, QWidget* parent = nullptr);
    ~CanvasWidget() override;

    void startAnimationPreview(int graphicIndex, bool playIn);
    void stopAnimationPreview();

    // Guides
    void setGuides(GuideFlags flags);
    GuideFlags guides() const
    {
        return m_guideFlags;
    }

    // Snapping
    void setSnapping(bool on);
    bool snapping() const
    {
        return m_snappingEnabled;
    }

    // Zoom / pan
    void zoomIn();
    void zoomOut();
    void fitToWindow();
    double zoom() const
    {
        return m_zoom;
    }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onDocumentChanged();
    void onSelectionChanged(SelectionId id);
    void onAnimTick();

private:
    // Coordinate helpers (all derived from letterboxRect — zoom/pan automatic)
    QRectF letterboxRect() const;
    QPointF widgetToScene(QPointF pt) const;
    QRectF sceneToWidget(const Rectangle& r) const;

    // Hit testing
    SelectionId hitTest(QPointF scenePt) const;
    int hitHandle(QPointF widgetPt) const;

    // Rendering
    void renderStaticScene();
    void renderPreviewScene();
    void setupCairo();
    void teardownCairo();

    // Overlay drawing
    void drawCheckerboard(QPainter& p, const QRectF& lb);
    void drawGuides(QPainter& p, const QRectF& lb);
    void drawSnapLines(QPainter& p, const QRectF& lb);

    // Scene dimension helpers
    int sceneW() const;
    int sceneH() const;

    // Zoom helper
    void zoomToward(QPointF widgetCenter, double factor);

    // Snapping
    Rectangle applySnapping(Rectangle b, int gi, int ei);
    void appendGuideCandidates(QList<double>& candX, QList<double>& candY) const;

    // Cursor
    void updateCursorForPos(QPointF widgetPos);

    // Graphic helpers
    Rectangle graphicSceneBounds(int gi) const;

    // ── Document / selection ──────────────────────────────────────────────────
    SceneDocument* m_doc;
    EditorScene* m_editorState;

    // ── Cairo offscreen surface ───────────────────────────────────────────────
    cairo_surface_t* m_surface{nullptr};
    cairo_t* m_cr{nullptr};
    QImage m_image; // zero-copy wrapper around m_surface pixels

    // ── Animation preview ─────────────────────────────────────────────────────
    QTimer* m_animTimer{nullptr};
    QElapsedTimer m_elapsedTimer;
    std::unique_ptr<Scene> m_previewScene;

    // ── Guides ───────────────────────────────────────────────────────────────
    GuideFlags m_guideFlags{GuideRuleOfThirds | GuideCenterLines};

    // ── Snapping ─────────────────────────────────────────────────────────────
    bool m_snappingEnabled{true};
    QList<double> m_snapLinesX; // active snap lines in scene coords during drag
    QList<double> m_snapLinesY;

    // ── Zoom + pan ────────────────────────────────────────────────────────────
    double m_zoom{1.0};
    QPointF m_panOffset{0.0, 0.0};

    // Middle-button pan
    bool m_panning{false};
    QPointF m_panStart;
    QPointF m_panStartOffset;

    // ── Element drag / resize ─────────────────────────────────────────────────
    enum class DragMode { None, Move, Resize, MoveGraphic };
    DragMode m_dragMode{DragMode::None};
    int m_dragHandle{-1};
    QPointF m_dragStartWidget;
    QPointF m_dragStartScene;
    Rectangle m_dragOrigBounds;
    int m_dragGi{-1};
    int m_dragEi{-1};
    bool m_dragging{false};

    std::vector<Rectangle> m_graphicOrigBounds;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(CanvasWidget::GuideFlags)
