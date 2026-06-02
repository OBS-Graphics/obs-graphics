#pragma once

#include <QMainWindow>

class SceneDocument;
class EditorScene;
class SceneTreeView;
class CanvasWidget;
class PropertiesPanel;
class QAction;
class QSplitter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();

private:
    void setupMenuBar();
    void setupCentralWidget();
    void updateWindowTitle();
    bool maybeSave();

    SceneDocument*  m_document{nullptr};
    EditorScene*    m_editorScene{nullptr};
    SceneTreeView*  m_treeView{nullptr};
    CanvasWidget*   m_canvas{nullptr};
    PropertiesPanel* m_properties{nullptr};

    QAction* m_undoAction{nullptr};
    QAction* m_redoAction{nullptr};
};
