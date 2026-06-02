#pragma once

#include <QWidget>
#include "engine/types.hpp"

class SceneDocument;
class ElementPropertiesWidget;
class PaintEditorWidget;
class CornerRadiusWidget;
class QScrollArea;

class RectanglePropertiesWidget : public QWidget {
    Q_OBJECT
public:
    explicit RectanglePropertiesWidget(SceneDocument* doc, QWidget* parent = nullptr);

    void setSelection(int gi, int ei);

private slots:
    void onDocumentChanged();
    void onFillChanged(Paint paint);
    void onStrokeChanged(Paint paint);
    void onCornerRadiusChanged(float tl, float tr, float br, float bl);

private:
    SceneDocument*          m_doc;
    int                     m_gi{-1};
    int                     m_ei{-1};
    bool                    m_updating{false};

    ElementPropertiesWidget* m_elementProps;
    PaintEditorWidget*       m_fillEditor;
    PaintEditorWidget*       m_strokeEditor;
    CornerRadiusWidget*      m_cornerRadius;
};
