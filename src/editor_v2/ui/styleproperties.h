#ifndef STYLEPROPERTIES_H
#define STYLEPROPERTIES_H

#include "ui/cornerradiuseditor.h"
#include "ui/painteditor.h"
#include "engine/types.hpp"
#include <string>
#include <QWidget>
#include <QDoubleSpinBox>

class SceneDocument;

class StyleProperties : public QWidget
{
    Q_OBJECT
public:
    explicit StyleProperties(SceneDocument* doc, QWidget *parent = nullptr);

    void setSelection(const std::string& graphicId, const std::string& elementId);

private slots:
    void onDocumentChanged();
    void onStrokeWidthChanged(double value);
    void onFillPaintChanged(const Paint& paint);
    void onStrokePaintChanged(const Paint& paint);
    void onCornerRadiusChanged(float tl, float tr, float br, float bl);

private:
    SceneDocument* m_doc;
    std::string m_graphicId;
    std::string m_elementId;
    bool m_updating{false};

    QDoubleSpinBox* m_strokeWidth;
    PaintEditor* m_strokePaint;
    PaintEditor* m_fillPaint;
    CornerRadiusEditor* m_cornerRadius;
};

#endif // STYLEPROPERTIES_H
