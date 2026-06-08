#pragma once
#include <QWidget>
#include <QColor>
#include <QImage>
#include <array>

class ColorWheel : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit ColorWheel(QWidget* parent = nullptr);
    QColor color() const;
    void setColor(const QColor& c);
    QSize sizeHint() const override;
    bool hasHeightForWidth() const override;
    int  heightForWidth(int w) const override;
signals:
    void colorChanged(const QColor&);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    enum class DragZone { None, Ring, Triangle };

    QColor   m_color{Qt::red};
    DragZone m_drag{DragZone::None};
    QImage   m_ringImage;
    QImage   m_triangleImage;
    bool     m_ringDirty{true};
    bool     m_triangleDirty{true};

    void rebuildRing();
    void rebuildTriangle();

    QPointF center() const;
    qreal   outerRadius() const;
    qreal   innerRadius() const;

    bool pointInRing(QPointF p) const;
    bool pointInTriangle(QPointF p) const;

    void handleRingDrag(QPointF p);
    void handleTriangleDrag(QPointF p);

    std::array<QPointF, 3> triangleVertices() const;
    QPointF svPosition() const;
};
