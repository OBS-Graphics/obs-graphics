/*
obs-graphics — Animated broadcast graphics source for OBS Studio
Copyright (C) 2026 Diego Lopes <diego95lopes@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include "graphics-dock.h"
#include "icons.h"
#include "engine/script.h"

#include <obs-module.h>

#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

// ── ScenePageWidget ──────────────────────────────────────────────────────────

ScenePageWidget::ScenePageWidget(std::shared_ptr<SceneSlot> slot, QWidget* parent)
    : QWidget(parent), m_slot(std::move(slot))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);

    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({"Name", ""});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(1, 115);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    rebuildTable();
}

ScenePageWidget::~ScenePageWidget()
{
    // Clear all raw dataSource pointers before releasing ownership of the IDataSource objects.
    // This must happen before m_dataSources is destroyed.
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        for (auto& g : m_slot->scene.graphics)
            g.dataSource = nullptr;
    }
    m_dataSources.clear();

    // Close any open data source dialogs.
    for (auto* dialog : m_openDialogs)
        delete dialog;
    m_openDialogs.clear();
}

void ScenePageWidget::rebuildTable()
{
    struct GraphicInfo {
        std::string id;
        bool isVisible;
    };

    std::vector<GraphicInfo> infos;
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        infos.reserve(m_slot->scene.graphics.size());
        for (auto& g : m_slot->scene.graphics) {
            bool vis = (g.state == GraphicState::Visible || g.state == GraphicState::AnimatingIn);
            infos.push_back({g.id, vis});
        }
    }

    m_dataSources.resize(infos.size());
    m_table->setRowCount(0);

    for (int i = 0; i < (int)infos.size(); ++i) {
        int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(infos[i].id)));
        m_table->setCellWidget(row, 1, buildActionCell(i, infos[i].isVisible));
    }
}

static void applyToggleStyle(QPushButton* btn, bool isVisible)
{
    if (isVisible) {
        btn->setText("Hide");
        btn->setIcon(themedIcon(Icons16::Action_EyeCrossed));
        btn->setStyleSheet(
            "QPushButton { background-color: #b42218; color: white; font-weight: bold; }");
    } else {
        btn->setText("Show");
        btn->setIcon(themedIcon(Icons16::Action_Eye));
        btn->setStyleSheet(
            "QPushButton { background-color: #28752a; color: white; font-weight: bold; }");
    }
}

QWidget* ScenePageWidget::buildActionCell(int graphicIndex, bool isVisible)
{
    auto* container = new QWidget();
    auto* hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(4, 2, 4, 2);
    hbox->setSpacing(0);

    // Show/Hide button with label and color
    auto* toggleBtn = new QPushButton();
    toggleBtn->setFixedHeight(24);
    applyToggleStyle(toggleBtn, isVisible);
    connect(toggleBtn, &QPushButton::clicked, this, [this, graphicIndex]() {
        onToggleGraphic(graphicIndex);
    });
    hbox->addWidget(toggleBtn, 1);

    hbox->addSpacing(6);

    // Data source button
    auto* dbBtn = new QToolButton();
    dbBtn->setAutoRaise(true);
    dbBtn->setFixedSize(24, 24);
    dbBtn->setIcon(themedIcon(Icons16::Hardware_Database));
    dbBtn->setToolTip("Data source...");
    connect(dbBtn, &QToolButton::clicked, this, [this, graphicIndex]() {
        onOpenDataSourceDialog(graphicIndex);
    });
    hbox->addWidget(dbBtn);

    return container;
}

void ScenePageWidget::onToggleGraphic(int index)
{
    bool nowVisible = false;
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        if (index < 0 || index >= (int)m_slot->scene.graphics.size())
            return;

        Graphic& g = m_slot->scene.graphics[index];
        bool isVisible = (g.state == GraphicState::Visible || g.state == GraphicState::AnimatingIn);
        if (isVisible) {
            g.TriggerOut();
            nowVisible = false;
        } else {
            g.TriggerIn(g.dataRecordIndex);
            nowVisible = true;
        }
    }

    // Update toggle button appearance
    if (auto* cell = m_table->cellWidget(index, 1)) {
        if (auto* toggleBtn = cell->findChild<QPushButton*>())
            applyToggleStyle(toggleBtn, nowVisible);
    }

    emit configChanged();
}

void ScenePageWidget::onOpenDataSourceDialog(int index)
{
    // Raise existing dialog if already open
    if (m_openDialogs.contains(index)) {
        m_openDialogs[index]->show();
        m_openDialogs[index]->raise();
        m_openDialogs[index]->activateWindow();
        return;
    }

    std::string graphicId;
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        if (index < 0 || index >= (int)m_slot->scene.graphics.size())
            return;
        graphicId = m_slot->scene.graphics[index].id;
    }

    auto* dialog = new DataSourceDialog(
        index,
        QString("Data Source — %1").arg(QString::fromStdString(graphicId)),
        this->window());

    connect(dialog, &DataSourceDialog::loadRequested, this, &ScenePageWidget::onLoadRequested);
    connect(dialog, &DataSourceDialog::recordSelected, this, &ScenePageWidget::onRecordSelected);

    // Show current data source if already loaded
    if (index < (int)m_dataSources.size() && m_dataSources[index]) {
        int currentRecord = 0;
        {
            std::lock_guard<std::mutex> lock(m_slot->mutex);
            if (index < (int)m_slot->scene.graphics.size())
                currentRecord = (int)m_slot->scene.graphics[index].dataRecordIndex;
        }
        dialog->setDataSource(m_dataSources[index].get(), currentRecord);
    }

    m_openDialogs[index] = dialog;
    dialog->show();
}

void ScenePageWidget::onLoadRequested(int index, const QString& path)
{
    std::unique_ptr<IDataSource> ds;
    try {
        if (path.endsWith(".json", Qt::CaseInsensitive))
            ds = std::make_unique<JsonFileDataSource>(path.toStdString());
        else if (path.endsWith(".lua", Qt::CaseInsensitive))
            ds = std::make_unique<ScriptDataSource>(path.toStdString());
        else
            ds = std::make_unique<CsvFileDataSource>(path.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Load Data Source",
                              QString("Could not load data source:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Load Data Source",
                              "Could not load data source: unknown error.");
        return;
    }

    IDataSource* rawPtr = ds.get();

    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        if (index < 0 || index >= (int)m_slot->scene.graphics.size())
            return;
        m_slot->scene.graphics[index].dataSource = rawPtr;
        m_slot->scene.graphics[index].dataRecordIndex = 0;
    }

    if (index >= (int)m_dataSources.size())
        m_dataSources.resize(index + 1);
    m_dataSources[index] = std::move(ds);

    // Update the open dialog with the new data source
    if (m_openDialogs.contains(index))
        m_openDialogs[index]->setDataSource(rawPtr, 0);

    emit configChanged();
}

void ScenePageWidget::onRecordSelected(int index, int recordIndex)
{
    std::lock_guard<std::mutex> lock(m_slot->mutex);
    if (index < 0 || index >= (int)m_slot->scene.graphics.size())
        return;
    m_slot->scene.graphics[index].dataRecordIndex = (size_t)recordIndex;
}

SceneConfig ScenePageWidget::toConfig() const
{
    SceneConfig cfg;
    cfg.scenePath = m_slot->path;

    std::vector<std::pair<std::string, size_t>> graphicInfo; // id → dataRecordIndex
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        for (auto& g : m_slot->scene.graphics)
            graphicInfo.push_back({g.id, g.dataRecordIndex});
    }

    for (int i = 0; i < (int)graphicInfo.size(); ++i) {
        auto& [id, recordIdx] = graphicInfo[i];
        cfg.selectedRecords[id] = (int)recordIdx;

        if (i < (int)m_dataSources.size() && m_dataSources[i]) {
            std::string path = m_dataSources[i]->GetFilePath();
            std::string type = path.ends_with(".csv")  ? "csv"
                               : path.ends_with(".lua") ? "lua"
                                                        : "json";
            cfg.dataSources[id] = {path, type};
        }
    }

    return cfg;
}

void ScenePageWidget::restoreFromConfig(const SceneConfig& cfg)
{
    std::vector<std::string> graphicIds;
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        for (auto& g : m_slot->scene.graphics)
            graphicIds.push_back(g.id);
    }

    for (int i = 0; i < (int)graphicIds.size(); ++i) {
        const std::string& id = graphicIds[i];

        auto dsIt = cfg.dataSources.find(id);
        if (dsIt == cfg.dataSources.end())
            continue;
        if (!std::filesystem::exists(dsIt->second.path))
            continue;

        std::unique_ptr<IDataSource> ds;
        try {
            if (dsIt->second.type == "csv")
                ds = std::make_unique<CsvFileDataSource>(dsIt->second.path);
            else if (dsIt->second.type == "lua")
                ds = std::make_unique<ScriptDataSource>(dsIt->second.path);
            else
                ds = std::make_unique<JsonFileDataSource>(dsIt->second.path);
        } catch (...) {
            continue;
        }

        IDataSource* rawPtr = ds.get();

        int selectedRecord = 0;
        auto recIt = cfg.selectedRecords.find(id);
        if (recIt != cfg.selectedRecords.end())
            selectedRecord = recIt->second;

        {
            std::lock_guard<std::mutex> lock(m_slot->mutex);
            if (i < (int)m_slot->scene.graphics.size()) {
                m_slot->scene.graphics[i].dataSource = rawPtr;
                m_slot->scene.graphics[i].dataRecordIndex = (size_t)selectedRecord;
            }
        }

        if (i >= (int)m_dataSources.size())
            m_dataSources.resize(i + 1);
        m_dataSources[i] = std::move(ds);
    }
}

// ── GraphicsDockWidget ───────────────────────────────────────────────────────

GraphicsDockWidget::GraphicsDockWidget(QWidget* parent, std::string configPath)
    : QWidget(parent), m_configPath(std::move(configPath))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_stack = new QStackedWidget();

    // ── Empty state (index 0) ───────────────────────────────────────────────
    auto* emptyWidget = new QWidget();
    auto* emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setAlignment(Qt::AlignCenter);

    auto* emptyLabel = new QLabel("No scenes loaded");
    emptyLabel->setAlignment(Qt::AlignCenter);
    QFont labelFont = emptyLabel->font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 1.1);
    emptyLabel->setFont(labelFont);
    emptyLabel->setEnabled(false); // grayed out appearance
    emptyLayout->addWidget(emptyLabel);

    emptyLayout->addSpacing(12);

    auto* emptyLoadBtn = new QPushButton(themedIcon(Icons16::Action_Plus), " Load Scene...");
    emptyLoadBtn->setFixedWidth(160);
    connect(emptyLoadBtn, &QPushButton::clicked, this, &GraphicsDockWidget::onAddTabClicked);
    emptyLayout->addWidget(emptyLoadBtn, 0, Qt::AlignCenter);

    m_stack->addWidget(emptyWidget); // index 0

    // ── Tab widget (index 1) ────────────────────────────────────────────────
    m_tabs = new QTabWidget();
    m_tabs->setTabsClosable(false); // We set custom close buttons per tab
    m_tabs->setMovable(false);

    // "+" button pinned to the left of the tab bar
    auto* addBtn = new QToolButton();
    addBtn->setIcon(themedIcon(Icons16::Action_Plus));
    addBtn->setAutoRaise(true);
    addBtn->setToolTip("Load scene...");
    connect(addBtn, &QToolButton::clicked, this, &GraphicsDockWidget::onAddTabClicked);
    m_tabs->setCornerWidget(addBtn, Qt::TopLeftCorner);

    connect(m_tabs, &QTabWidget::currentChanged, this, &GraphicsDockWidget::onTabChanged);

    m_stack->addWidget(m_tabs); // index 1

    layout->addWidget(m_stack);

    loadConfig();
    updateEmptyState();
}

void GraphicsDockWidget::addSceneTab(std::shared_ptr<SceneSlot> slot)
{
    auto* page = new ScenePageWidget(slot, this);
    connect(page, &ScenePageWidget::configChanged, this, &GraphicsDockWidget::onChildConfigChanged);

    QString label = QString::fromStdString(
        std::filesystem::path(slot->path).stem().string());
    if (label.isEmpty())
        label = "Scene";

    int idx = m_tabs->addTab(page, label);

    // Custom close button using Qlementine icon
    auto* closeBtn = new QToolButton();
    closeBtn->setIcon(themedIcon(Icons16::Action_CloseSmall));
    closeBtn->setAutoRaise(true);
    closeBtn->setFixedSize(16, 16);
    closeBtn->setToolTip("Close");
    connect(closeBtn, &QToolButton::clicked, this, [this, slot]() {
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto* p = qobject_cast<ScenePageWidget*>(m_tabs->widget(i))) {
                if (p->slot() == slot) {
                    onTabCloseRequested(i);
                    return;
                }
            }
        }
    });
    m_tabs->tabBar()->setTabButton(idx, QTabBar::RightSide, closeBtn);
}

void GraphicsDockWidget::updateEmptyState()
{
    m_stack->setCurrentIndex(m_tabs->count() > 0 ? 1 : 0);
}

void GraphicsDockWidget::onAddTabClicked()
{
    QString path =
        QFileDialog::getOpenFileName(this, "Load Scene", QString(), "JSON Files (*.json)");
    if (path.isEmpty())
        return;

    auto slot = std::make_shared<SceneSlot>();

    try {
        Scene loaded = Scene::Load(path.toStdString());
        obs_video_info ovi;
        if (obs_get_video_info(&ovi)) {
            loaded.width = (int)ovi.base_width;
            loaded.height = (int)ovi.base_height;
        }
        slot->scene = std::move(loaded);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Load Scene",
                              QString("Could not load scene:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Load Scene",
                              "Could not load scene: unknown error.");
        return;
    }

    slot->path = path.toStdString();
    slot->loaded = true;

    g_scene_slots.push_back(slot);

    if (!g_active_slot.load())
        g_active_slot.store(slot);

    addSceneTab(slot);
    updateEmptyState();
    m_tabs->setCurrentIndex(m_tabs->count() - 1);
    // onTabChanged fires and sets g_active_slot; saveConfig called there
}

void GraphicsDockWidget::onTabCloseRequested(int index)
{
    auto* page = qobject_cast<ScenePageWidget*>(m_tabs->widget(index));
    if (!page)
        return;

    auto closingSlot = page->slot();

    // Switch active slot before removing the tab
    if (g_active_slot.load() == closingSlot) {
        std::shared_ptr<SceneSlot> newActive;
        int count = m_tabs->count();
        if (count > 1) {
            int newIdx = (index > 0) ? index - 1 : 1;
            if (auto* newPage = qobject_cast<ScenePageWidget*>(m_tabs->widget(newIdx)))
                newActive = newPage->slot();
        }
        g_active_slot.store(newActive);
    }

    // Remove from global slot list
    g_scene_slots.erase(std::remove(g_scene_slots.begin(), g_scene_slots.end(), closingSlot),
                        g_scene_slots.end());

    // Remove tab widget (does not delete it)
    m_tabs->removeTab(index);
    delete page;

    updateEmptyState();
    saveConfig();
}

void GraphicsDockWidget::onTabChanged(int index)
{
    if (m_loading)
        return;
    if (index < 0)
        return;

    if (auto* page = qobject_cast<ScenePageWidget*>(m_tabs->widget(index)))
        g_active_slot.store(page->slot());

    saveConfig();
}

void GraphicsDockWidget::onChildConfigChanged()
{
    saveConfig();
}

void GraphicsDockWidget::saveConfig()
{
    if (m_loading)
        return;

    AppConfig cfg;
    cfg.activeTabIndex = m_tabs->currentIndex();

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto* page = qobject_cast<ScenePageWidget*>(m_tabs->widget(i)))
            cfg.scenes.push_back(page->toConfig());
    }

    cfg.Save(m_configPath);
}

void GraphicsDockWidget::loadConfig()
{
    AppConfig cfg = AppConfig::Load(m_configPath);
    if (cfg.scenes.empty())
        return;

    m_loading = true;

    for (auto& scCfg : cfg.scenes) {
        if (scCfg.scenePath.empty() || !std::filesystem::exists(scCfg.scenePath))
            continue;

        auto slot = std::make_shared<SceneSlot>();
        try {
            Scene loaded = Scene::Load(scCfg.scenePath);
            obs_video_info ovi;
            if (obs_get_video_info(&ovi)) {
                loaded.width = (int)ovi.base_width;
                loaded.height = (int)ovi.base_height;
            }
            slot->scene = std::move(loaded);
        } catch (...) {
            continue;
        }

        slot->path = scCfg.scenePath;
        slot->loaded = true;

        g_scene_slots.push_back(slot);
        addSceneTab(slot);

        // Restore data sources into the newly created page widget
        if (auto* page = qobject_cast<ScenePageWidget*>(m_tabs->widget(m_tabs->count() - 1)))
            page->restoreFromConfig(scCfg);
    }

    if (m_tabs->count() == 0) {
        m_loading = false;
        updateEmptyState();
        return;
    }

    int activeIdx = std::clamp(cfg.activeTabIndex, 0, m_tabs->count() - 1);
    m_tabs->setCurrentIndex(activeIdx);

    if (auto* page = qobject_cast<ScenePageWidget*>(m_tabs->widget(activeIdx)))
        g_active_slot.store(page->slot());

    m_loading = false;
    updateEmptyState();
}
