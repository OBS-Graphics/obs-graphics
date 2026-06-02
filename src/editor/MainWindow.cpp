#include "MainWindow.h"

#include "model/SceneDocument.h"
#include "model/EditorScene.h"
#include "ui/SceneTreeView.h"
#include "ui/CanvasWidget.h"
#include "ui/PropertiesPanel.h"

#include <QActionGroup>

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QGroupBox>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QUndoStack>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_document(new SceneDocument(this))
    , m_editorScene(new EditorScene(m_document, this))
{
    resize(1400, 860);

    setupCentralWidget();
    setupMenuBar();
    updateWindowTitle();

    connect(m_document, &SceneDocument::modifiedChanged,
            this, &MainWindow::setWindowModified);
    connect(m_document, &SceneDocument::filePathChanged,
            this, [this](const QString&) { updateWindowTitle(); });
    connect(m_document, &SceneDocument::documentChanged,
            this, [this]() { updateWindowTitle(); });
    connect(m_document, &SceneDocument::filePathChanged,
            this, [this](const QString& path) {
                statusBar()->showMessage(path.isEmpty() ? "Untitled" : path);
            });

    statusBar()->showMessage("Untitled");
}

MainWindow::~MainWindow() = default;

void MainWindow::setupCentralWidget()
{
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: scene tree
    auto* leftGroup  = new QGroupBox("Scene", splitter);
    auto* leftLayout = new QVBoxLayout(leftGroup);
    leftLayout->setContentsMargins(2, 4, 2, 2);
    m_treeView = new SceneTreeView(m_document, m_editorScene, leftGroup);
    leftLayout->addWidget(m_treeView);

    splitter->addWidget(leftGroup);

    // Center: canvas
    m_canvas = new CanvasWidget(m_document, m_editorScene, splitter);
    m_canvas->setMinimumWidth(400);
    splitter->addWidget(m_canvas);

    // Right: properties
    m_properties = new PropertiesPanel(m_document, m_editorScene, splitter);
    m_properties->setMinimumWidth(260);
    splitter->addWidget(m_properties);

    splitter->setSizes({240, 900, 300});
    setCentralWidget(splitter);
}

void MainWindow::setupMenuBar()
{
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* newAction = fileMenu->addAction("&New");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::onNew);

    auto* openAction = fileMenu->addAction("&Open...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::onOpen);

    fileMenu->addSeparator();

    auto* saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::onSave);

    auto* saveAsAction = fileMenu->addAction("Save &As...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAs);

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu("&Edit");

    m_undoAction = m_document->undoStack()->createUndoAction(this, "&Undo");
    m_undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(m_undoAction);

    m_redoAction = m_document->undoStack()->createRedoAction(this, "&Redo");
    m_redoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    editMenu->addAction(m_redoAction);

    // ── View ─────────────────────────────────────────────────────────────────
    auto* viewMenu = menuBar()->addMenu("&View");

    // Snapping
    auto* snapAct = viewMenu->addAction("Snapping");
    snapAct->setCheckable(true);
    snapAct->setChecked(true);
    snapAct->setShortcut(QKeySequence(Qt::Key_Semicolon));
    connect(snapAct, &QAction::toggled, m_canvas, &CanvasWidget::setSnapping);

    viewMenu->addSeparator();

    // Zoom
    auto* zoomInAct  = viewMenu->addAction("Zoom In",        m_canvas, &CanvasWidget::zoomIn);
    auto* zoomOutAct = viewMenu->addAction("Zoom Out",       m_canvas, &CanvasWidget::zoomOut);
    auto* fitAct     = viewMenu->addAction("Fit to Window",  m_canvas, &CanvasWidget::fitToWindow);
    zoomInAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
    zoomOutAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    fitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    viewMenu->addSeparator();

    // Guides submenu
    auto* guidesMenu = viewMenu->addMenu("Guides");
    auto makeGuide = [&](const QString& label, CanvasWidget::GuideFlag flag, bool on) {
        auto* act = guidesMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(on);
        connect(act, &QAction::toggled, this, [this, flag](bool checked) {
            CanvasWidget::GuideFlags f = m_canvas->guides();
            if (checked) f |= flag; else f &= ~flag;
            m_canvas->setGuides(f);
        });
    };
    makeGuide("Rule of Thirds",    CanvasWidget::GuideRuleOfThirds, true);
    makeGuide("Center Lines",      CanvasWidget::GuideCenterLines,  true);
    makeGuide("Title Safe (90%)",  CanvasWidget::GuideTitleSafe,    false);
    makeGuide("Action Safe (80%)", CanvasWidget::GuideActionSafe,   false);
}

void MainWindow::onNew()
{
    if (!maybeSave())
        return;

    m_document->reset();
    m_editorScene->setSelection(SelectionId{});
}

void MainWindow::onOpen()
{
    if (!maybeSave())
        return;

    QString path = QFileDialog::getOpenFileName(
        this, "Open Scene", QString(), "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return;

    if (!m_document->load(path)) {
        QMessageBox::warning(this, "Open Failed",
                             QString("Could not open file:\n%1").arg(path));
    }
}

void MainWindow::onSave()
{
    if (m_document->filePath().isEmpty()) {
        onSaveAs();
        return;
    }
    if (!m_document->save())
        QMessageBox::warning(this, "Save Failed", "Could not save the file.");
}

void MainWindow::onSaveAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save Scene As", QString(), "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty())
        return;

    if (!m_document->saveAs(path))
        QMessageBox::warning(this, "Save Failed",
                             QString("Could not save file:\n%1").arg(path));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave())
        event->accept();
    else
        event->ignore();
}

bool MainWindow::maybeSave()
{
    if (!m_document->isModified())
        return true;

    auto result = QMessageBox::question(
        this, "Unsaved Changes",
        QString("The scene \"%1\" has unsaved changes.\n"
                "Do you want to save before continuing?")
            .arg(m_document->sceneName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (result == QMessageBox::Save) {
        onSave();
        return !m_document->isModified();
    }
    return result == QMessageBox::Discard;
}

void MainWindow::updateWindowTitle()
{
    setWindowTitle(
        QString("%1[*] — obs-graphics Scene Editor")
            .arg(m_document->sceneName()));
}
