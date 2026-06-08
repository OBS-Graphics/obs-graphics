#pragma once

#include "model/EditorScene.h"
#include <QMainWindow>

class SceneDocument;
class EditorScene;
class ElementProperties;
class StyleProperties;
class SceneProperties;
class CanvasWidget;
class QAction;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSelectionChanged(SelectionId id);
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();

private:
    void setupMenuBar();
    void setupToolBar();
    void updateWindowTitle();
    void updateToolBarState(SelectionId sel);
    bool maybeSave();

    SceneDocument*     m_doc;
    EditorScene*       m_editorScene;
    CanvasWidget*      m_canvas;
    ElementProperties* m_elementProperties;
    StyleProperties*   m_styleProperties;
    SceneProperties*   m_sceneProperties;

    QAction* m_undoAction{nullptr};
    QAction* m_redoAction{nullptr};

    QAction* m_addRectAction{nullptr};
    QAction* m_addTextAction{nullptr};
    QAction* m_deleteAction{nullptr};
};
