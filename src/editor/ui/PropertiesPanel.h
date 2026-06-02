#pragma once

#include <QScrollArea>
#include <QStackedWidget>
#include "model/EditorScene.h"

class SceneDocument;
class EditorScene;
class ScenePropertiesWidget;
class GraphicPropertiesWidget;
class RectanglePropertiesWidget;
class TextPropertiesWidget;

// QStackedWidget that reports the current page's size hint instead of the
// maximum across all pages, preventing the scroll area from being forced wide
// by the widest page (e.g. TextPropertiesWidget's QFontComboBox).
class AdaptiveStack : public QStackedWidget {
    Q_OBJECT
public:
    using QStackedWidget::QStackedWidget;
    QSize sizeHint() const override {
        auto* w = currentWidget();
        return w ? w->sizeHint() : QStackedWidget::sizeHint();
    }
    QSize minimumSizeHint() const override {
        auto* w = currentWidget();
        return w ? w->minimumSizeHint() : QStackedWidget::minimumSizeHint();
    }
};

class PropertiesPanel : public QScrollArea {
    Q_OBJECT
public:
    explicit PropertiesPanel(SceneDocument* doc, EditorScene* editorState,
                             QWidget* parent = nullptr);

private slots:
    void onSelectionChanged(SelectionId id);

private:
    AdaptiveStack*              m_stack;
    QWidget*                    m_emptyPage;
    ScenePropertiesWidget*      m_sceneProps;
    GraphicPropertiesWidget*    m_graphicProps;
    RectanglePropertiesWidget*  m_rectProps;
    TextPropertiesWidget*       m_textProps;

    SceneDocument* m_doc;
    EditorScene*   m_editorState;
};
