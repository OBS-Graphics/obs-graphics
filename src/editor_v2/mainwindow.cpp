#include "mainwindow.h"
#include "model/SceneDocument.h"
#include "model/EditorScene.h"
#include "model/UndoCommands.h"
#include "ui/elementproperties.h"
#include "ui/styleproperties.h"
#include "ui/sceneproperties.h"
#include "ui/widgets/CanvasWidget.h"
#include "ui/widgets/SceneTreeView.h"
#include "engine/scene.h"
#include "engine/graphic.h"
#include "engine/json.hpp"
#include "icons.h"

#include <algorithm>
#include <string>

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QUndoStack>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_doc(new SceneDocument(this))
    , m_editorScene(new EditorScene(m_doc, this))
    , m_canvas(new CanvasWidget(m_doc, m_editorScene, this))
{
    resize(1440, 860);
    setCentralWidget(m_canvas);

    // Scene tree — left dock
    auto* sceneDock = new QDockWidget("Scene", this);
    sceneDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    sceneDock->setWidget(new SceneTreeView(m_doc, m_editorScene, sceneDock));
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    // Properties — right dock with tabs
    auto* propDock = new QDockWidget("Properties", this);
    propDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);
    addDockWidget(Qt::RightDockWidgetArea, propDock);

    auto* tabs = new QTabWidget;
    propDock->setWidget(tabs);

    m_elementProperties = new ElementProperties(m_doc);
    auto* scrollElem = new QScrollArea;
    scrollElem->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollElem->setWidgetResizable(true);
    scrollElem->setWidget(m_elementProperties);
    tabs->addTab(scrollElem, "Element");

    m_styleProperties = new StyleProperties(m_doc);
    auto* scrollStyle = new QScrollArea;
    scrollStyle->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollStyle->setWidgetResizable(true);
    scrollStyle->setWidget(m_styleProperties);
    tabs->addTab(scrollStyle, "Styling");

    m_sceneProperties = new SceneProperties(m_doc);
    auto* scrollScene = new QScrollArea;
    scrollScene->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollScene->setWidgetResizable(true);
    scrollScene->setWidget(m_sceneProperties);
    tabs->addTab(scrollScene, "Scene");

    tabs->setCurrentIndex(0);

    setupMenuBar();
    setupToolBar();
    updateWindowTitle();

    connect(m_editorScene, &EditorScene::selectionChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_doc, &SceneDocument::modifiedChanged,
            this, &MainWindow::setWindowModified);
    connect(m_doc, &SceneDocument::filePathChanged,
            this, [this](const QString& path) {
                updateWindowTitle();
                statusBar()->showMessage(path.isEmpty() ? "Untitled" : path);
            });

    statusBar()->showMessage("Untitled");
}

// ── Menu bar ──────────────────────────────────────────────────────────────────

void MainWindow::setupMenuBar()
{
    // File
    auto* fileMenu = menuBar()->addMenu("&File");

    auto* newAct = fileMenu->addAction("&New");
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);

    auto* openAct = fileMenu->addAction("&Open...");
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);

    fileMenu->addSeparator();

    auto* saveAct = fileMenu->addAction("&Save");
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &MainWindow::onSave);

    auto* saveAsAct = fileMenu->addAction("Save &As...");
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, &MainWindow::onSaveAs);

    fileMenu->addSeparator();

    auto* exitAct = fileMenu->addAction("E&xit");
    exitAct->setShortcut(QKeySequence::Quit);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Edit
    auto* editMenu = menuBar()->addMenu("&Edit");

    m_undoAction = m_doc->undoStack()->createUndoAction(this, "&Undo");
    m_undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(m_undoAction);

    m_redoAction = m_doc->undoStack()->createRedoAction(this, "&Redo");
    m_redoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    editMenu->addAction(m_redoAction);

    // View
    auto* viewMenu = menuBar()->addMenu("&View");

    auto* snapAct = viewMenu->addAction("Snapping");
    snapAct->setCheckable(true);
    snapAct->setChecked(true);
    snapAct->setShortcut(QKeySequence(Qt::Key_Semicolon));
    connect(snapAct, &QAction::toggled, m_canvas, &CanvasWidget::setSnapping);

    viewMenu->addSeparator();

    auto* zoomInAct  = viewMenu->addAction("Zoom In",       m_canvas, &CanvasWidget::zoomIn);
    auto* zoomOutAct = viewMenu->addAction("Zoom Out",      m_canvas, &CanvasWidget::zoomOut);
    auto* fitAct     = viewMenu->addAction("Fit to Window", m_canvas, &CanvasWidget::fitToWindow);
    zoomInAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal));
    zoomOutAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    fitAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));

    viewMenu->addSeparator();

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

// ── Toolbar ───────────────────────────────────────────────────────────────────

void MainWindow::setupToolBar()
{
    QToolBar* tb = addToolBar("Main");
    tb->setMovable(false);
    tb->setIconSize(QSize(18, 18));

    auto* newAct = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_file), "");
    newAct->setToolTip("New Scene (Ctrl+N)");
    connect(newAct, &QAction::triggered, this, &MainWindow::onNew);

    auto* openAct = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_folder_open), "");
    openAct->setToolTip("Open Scene (Ctrl+O)");
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);

    auto* saveAct = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_floppy_disk), "");
    saveAct->setToolTip("Save Scene (Ctrl+S)");
    connect(saveAct, &QAction::triggered, this, &MainWindow::onSave);

    tb->addSeparator();

    auto* addGraphicAct = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_layer_group), "");
    addGraphicAct->setToolTip("Add Graphic");
    connect(addGraphicAct, &QAction::triggered, this, [this]() {
        using json = nlohmann::json;
        int maxN = 0;
        for (const auto& g : m_doc->scene().graphics) {
            if (g.id.rfind("graphic_", 0) == 0)
                try { maxN = std::max(maxN, std::stoi(g.id.substr(8))); } catch (...) {}
        }
        std::string newId = "graphic_" + std::to_string(maxN + 1);
        int newZ = (int)m_doc->scene().graphics.size();
        json j = {{"id", newId}, {"z_order", newZ}, {"elements", json::array()}};
        m_doc->undoStack()->push(new AddGraphicCmd(m_doc, std::move(j)));
        // find the newly inserted graphic by id and select it
        const auto& gs = m_doc->scene().graphics;
        for (int i = 0; i < (int)gs.size(); ++i) {
            if (gs[i].id == newId) {
                m_editorScene->setSelection({SelectionId::Level::Graphic, i, -1});
                break;
            }
        }
    });

    m_addRectAction = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_square), "");
    m_addRectAction->setToolTip("Add Rectangle Element (select a graphic first)");
    m_addRectAction->setEnabled(false);
    auto* addRectAct = m_addRectAction;
    connect(addRectAct, &QAction::triggered, this, [this]() {
        const SelectionId sel = m_editorScene->selection();
        int gi = -1;
        if (sel.level == SelectionId::Level::Graphic) gi = sel.graphicIndex;
        if (sel.level == SelectionId::Level::Element) gi = sel.graphicIndex;
        if (gi < 0 || gi >= (int)m_doc->scene().graphics.size()) return;
        const Graphic& graphic = m_doc->scene().graphics[gi];
        const std::string gid = graphic.id;
        int maxN = 0;
        for (const auto& el : graphic.elements)
            if (el.id.rfind("element_", 0) == 0)
                try { maxN = std::max(maxN, std::stoi(el.id.substr(8))); } catch (...) {}
        std::string newId = "element_" + std::to_string(maxN + 1);
        int newZ = (int)graphic.elements.size();
        using json = nlohmann::json;
        json j = {{"id",newId},{"type","rectangle"},{"x",100},{"y",100},
                  {"w",200},{"h",80},{"fill",{0.5,0.5,0.9,1.0}},{"z_order",newZ}};
        m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));
        const auto& els = m_doc->scene().graphics[gi].elements;
        for (int ei = 0; ei < (int)els.size(); ++ei) {
            if (els[ei].id == newId) {
                m_editorScene->setSelection({SelectionId::Level::Element, gi, ei});
                break;
            }
        }
    });

    m_addTextAction = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_font), "");
    m_addTextAction->setToolTip("Add Text Element (select a graphic first)");
    m_addTextAction->setEnabled(false);
    auto* addTextAct = m_addTextAction;
    connect(addTextAct, &QAction::triggered, this, [this]() {
        const SelectionId sel = m_editorScene->selection();
        int gi = -1;
        if (sel.level == SelectionId::Level::Graphic) gi = sel.graphicIndex;
        if (sel.level == SelectionId::Level::Element) gi = sel.graphicIndex;
        if (gi < 0 || gi >= (int)m_doc->scene().graphics.size()) return;
        const Graphic& graphic = m_doc->scene().graphics[gi];
        const std::string gid = graphic.id;
        int maxN = 0;
        for (const auto& el : graphic.elements)
            if (el.id.rfind("text_", 0) == 0)
                try { maxN = std::max(maxN, std::stoi(el.id.substr(5))); } catch (...) {}
        std::string newId = "text_" + std::to_string(maxN + 1);
        int newZ = (int)graphic.elements.size();
        using json = nlohmann::json;
        json j = {{"id",newId},{"type","text"},{"x",100},{"y",100},{"w",300},{"h",60},
                  {"text","New Text"},{"font_family","Sans"},{"font_size",36},
                  {"color",{1.0,1.0,1.0,1.0}},{"z_order",newZ}};
        m_doc->undoStack()->push(new AddElementCmd(m_doc, gid, std::move(j)));
        const auto& els = m_doc->scene().graphics[gi].elements;
        for (int ei = 0; ei < (int)els.size(); ++ei) {
            if (els[ei].id == newId) {
                m_editorScene->setSelection({SelectionId::Level::Element, gi, ei});
                break;
            }
        }
    });

    m_deleteAction = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_trash), "");
    m_deleteAction->setToolTip("Delete Selected");
    m_deleteAction->setEnabled(false);
    auto* deleteAct = m_deleteAction;
    connect(deleteAct, &QAction::triggered, this, [this]() {
        const SelectionId sel = m_editorScene->selection();
        if (sel.level == SelectionId::Level::Graphic) {
            const std::string gid = m_doc->scene().graphics[sel.graphicIndex].id;
            m_doc->undoStack()->push(new RemoveGraphicCmd(m_doc, gid));
        } else if (sel.level == SelectionId::Level::Element) {
            const auto& g = m_doc->scene().graphics[sel.graphicIndex];
            m_doc->undoStack()->push(
                new RemoveElementCmd(m_doc, g.id, g.elements[sel.elementIndex].id));
        }
    });

    tb->addSeparator();

    auto* zoomInAct = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_magnifying_glass_plus), "");
    zoomInAct->setToolTip("Zoom In (Ctrl+=)");
    connect(zoomInAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomIn);

    auto* zoomOutAct = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_magnifying_glass_minus), "");
    zoomOutAct->setToolTip("Zoom Out (Ctrl+-)");
    connect(zoomOutAct, &QAction::triggered, m_canvas, &CanvasWidget::zoomOut);

    auto* fitAct = tb->addAction(qta()->icon(fa::fa_solid, fa::fa_expand), "");
    fitAct->setToolTip("Fit to Window (Ctrl+0)");
    connect(fitAct, &QAction::triggered, m_canvas, &CanvasWidget::fitToWindow);
}

// ── File operations ───────────────────────────────────────────────────────────

void MainWindow::onNew()
{
    if (!maybeSave()) return;
    m_doc->reset();
    m_editorScene->setSelection(SelectionId{});
}

void MainWindow::onOpen()
{
    if (!maybeSave()) return;
    QString path = QFileDialog::getOpenFileName(
        this, "Open Scene", QString(), "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    if (!m_doc->load(path))
        QMessageBox::warning(this, "Open Failed",
                             QString("Could not open file:\n%1").arg(path));
    else
        m_editorScene->setSelection(SelectionId{});
}

void MainWindow::onSave()
{
    if (m_doc->filePath().isEmpty()) { onSaveAs(); return; }
    if (!m_doc->save())
        QMessageBox::warning(this, "Save Failed", "Could not save the file.");
}

void MainWindow::onSaveAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, "Save Scene As", QString(), "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;
    if (!m_doc->saveAs(path))
        QMessageBox::warning(this, "Save Failed",
                             QString("Could not save file:\n%1").arg(path));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    maybeSave() ? event->accept() : event->ignore();
}

bool MainWindow::maybeSave()
{
    if (!m_doc->isModified()) return true;
    auto result = QMessageBox::question(
        this, "Unsaved Changes",
        QString("The scene \"%1\" has unsaved changes.\nSave before continuing?")
            .arg(m_doc->sceneName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (result == QMessageBox::Save) {
        onSave();
        return !m_doc->isModified();
    }
    return result == QMessageBox::Discard;
}

void MainWindow::updateWindowTitle()
{
    setWindowTitle(QString("%1[*] — obs-graphics Scene Editor")
                       .arg(m_doc->sceneName()));
}

// ── Selection ─────────────────────────────────────────────────────────────────

void MainWindow::updateToolBarState(SelectionId id)
{
    bool hasGraphic = (id.level == SelectionId::Level::Graphic ||
                       id.level == SelectionId::Level::Element);
    bool hasAny     = (id.level == SelectionId::Level::Graphic ||
                       id.level == SelectionId::Level::Element);

    if (m_addRectAction) m_addRectAction->setEnabled(hasGraphic);
    if (m_addTextAction) m_addTextAction->setEnabled(hasGraphic);
    if (m_deleteAction)  m_deleteAction->setEnabled(hasAny);
}

void MainWindow::onSelectionChanged(SelectionId id)
{
    updateToolBarState(id);

    if (id.level == SelectionId::Level::Element) {
        const Scene& s = m_doc->scene();
        if (id.graphicIndex < 0 || id.graphicIndex >= (int)s.graphics.size()) return;
        const Graphic& g = s.graphics[id.graphicIndex];
        if (id.elementIndex < 0 || id.elementIndex >= (int)g.elements.size()) return;
        m_elementProperties->setSelection(g.id, g.elements[id.elementIndex].id);
        m_styleProperties->setSelection(g.id, g.elements[id.elementIndex].id);
    } else {
        m_elementProperties->setSelection({}, {});
        m_styleProperties->setSelection({}, {});
    }
}
