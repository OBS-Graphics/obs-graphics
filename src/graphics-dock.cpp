/*
StreamCanvas — Animated broadcast graphics source for OBS Studio
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
#include "engine/script.h"
#include "icons.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

static const char* kStyleIn  = "QPushButton { background-color: #28752a; color: white; font-weight: bold; }";
static const char* kStyleOut = "QPushButton { background-color: #b42218; color: white; font-weight: bold; }";

namespace {

// Makes a profile/scene collection name safe to use as a single path component.
std::string sanitizeForPath(const std::string& name)
{
    std::string out = name;
    for (char& c : out) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|' || static_cast<unsigned char>(c) < 0x20)
            c = '_';
    }
    return out.empty() ? std::string("default") : out;
}

} // namespace

GraphicsDockWidget::GraphicsDockWidget(QWidget* parent, std::string configDir)
    : QWidget(parent), m_configDir(std::move(configDir))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Toolbar row
    auto* toolbar = new QWidget();
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);

    auto* addBtn = new QPushButton(themedIcon(Icons16::Action_Plus), " Add Title...");
    connect(addBtn, &QPushButton::clicked, this, &GraphicsDockWidget::onAddTitleClicked);
    toolbarLayout->addWidget(addBtn);
    toolbarLayout->addStretch();
    layout->addWidget(toolbar);

    // Title table: two columns — Title name | Actions
    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({"Title", ""});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(1, 160);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    // Titles are loaded once the active profile/scene collection is known —
    // see reloadForCurrentContext(), driven by OBS frontend events.
}

void GraphicsDockWidget::addTitleRow(std::shared_ptr<TitleSlot> slot)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);

    QString name = QString::fromStdString(
        std::filesystem::path(slot->path).stem().string());
    if (name.isEmpty())
        name = "Title";
    m_table->setItem(row, 0, new QTableWidgetItem(name));

    // Actions cell: [Toggle In/Out] [Data Source] [Remove]
    auto* actions = new QWidget();
    auto* hbox = new QHBoxLayout(actions);
    hbox->setContentsMargins(2, 2, 2, 2);
    hbox->setSpacing(2);

    auto* toggleBtn = new QPushButton(themedIcon(Icons16::Media_Play), " In");
    toggleBtn->setStyleSheet(kStyleIn);
    hbox->addWidget(toggleBtn, 1);

    auto* dsBtn = new QToolButton();
    dsBtn->setIcon(themedIcon(Icons16::Hardware_Database));
    dsBtn->setIconSize(QSize(16, 16));
    dsBtn->setAutoRaise(true);
    dsBtn->setFixedWidth(28);
    dsBtn->setToolTip("Data Source...");
    hbox->addWidget(dsBtn);

    auto* removeBtn = new QToolButton();
    removeBtn->setIcon(themedIcon(Icons16::Action_Trash));
    removeBtn->setIconSize(QSize(16, 16));
    removeBtn->setAutoRaise(true);
    removeBtn->setFixedWidth(28);
    removeBtn->setToolTip("Remove");
    hbox->addWidget(removeBtn);

    m_table->setCellWidget(row, 1, actions);

    RowWidgets rw;
    rw.toggleBtn = toggleBtn;
    rw.isIn = false;
    m_rowWidgets.push_back(rw);
    m_slots.push_back(std::move(slot));

    // Use slot raw pointer for stable identity across vector mutations
    auto* slotPtr = m_slots.back().get();

    connect(toggleBtn, &QPushButton::clicked, this, [this, slotPtr]() {
        auto it = std::find_if(m_slots.begin(), m_slots.end(),
            [slotPtr](const auto& s) { return s.get() == slotPtr; });
        if (it != m_slots.end())
            onToggle(static_cast<int>(it - m_slots.begin()));
    });

    connect(dsBtn, &QToolButton::clicked, this, [this, slotPtr]() {
        auto it = std::find_if(m_slots.begin(), m_slots.end(),
            [slotPtr](const auto& s) { return s.get() == slotPtr; });
        if (it != m_slots.end())
            onDataSource(static_cast<int>(it - m_slots.begin()));
    });

    connect(removeBtn, &QToolButton::clicked, this, [this, slotPtr]() {
        auto it = std::find_if(m_slots.begin(), m_slots.end(),
            [slotPtr](const auto& s) { return s.get() == slotPtr; });
        if (it != m_slots.end())
            onRemoveTitle(static_cast<int>(it - m_slots.begin()));
    });
}

void GraphicsDockWidget::rebuildGlobalList()
{
    auto newList = std::make_shared<TitleSlotList>(m_slots);
    g_title_slots.store(newList);
}

void GraphicsDockWidget::onAddTitleClicked()
{
    QString path =
        QFileDialog::getOpenFileName(this, "Load Title", QString(), "StreamCanvas Title (*.ogt)");
    if (path.isEmpty())
        return;

    auto slot = std::make_shared<TitleSlot>();
    try {
        slot->title = Title::Load(path.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Load Title",
                              QString("Could not load title:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Load Title",
                              "Could not load title: unknown error.");
        return;
    }

    slot->path = path.toStdString();
    slot->loaded = true;

    addTitleRow(slot);
    rebuildGlobalList();
    saveConfig();
}

void GraphicsDockWidget::onToggle(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;

    auto& slot = m_slots[row];
    auto& rw = m_rowWidgets[row];

    std::lock_guard<std::mutex> lock(slot->mutex);
    if (!rw.isIn) {
        // Button shows "In" → trigger In, then show "Out"
        size_t recIdx = 0;
        if (rw.dsDialog) {
            int sel = rw.dsDialog->selectedRecord();
            if (sel >= 0)
                recIdx = static_cast<size_t>(sel);
        }
        slot->title.TriggerIn(recIdx);
        rw.isIn = true;
        rw.toggleBtn->setText(" Out");
        rw.toggleBtn->setIcon(themedIcon(Icons16::Media_Stop));
        rw.toggleBtn->setStyleSheet(kStyleOut);
    } else {
        // Button shows "Out" → trigger Out, then show "In"
        slot->title.TriggerOut();
        rw.isIn = false;
        rw.toggleBtn->setText(" In");
        rw.toggleBtn->setIcon(themedIcon(Icons16::Media_Play));
        rw.toggleBtn->setStyleSheet(kStyleIn);
    }
}

void GraphicsDockWidget::onDataSource(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;

    auto& rw = m_rowWidgets[row];
    if (!rw.dsDialog) {
        QString name = m_table->item(row, 0) ? m_table->item(row, 0)->text() : "Title";
        rw.dsDialog = new DataSourceDialog(name, this);
        rw.dsDialog->setSlot(m_slots[row].get());
        connect(rw.dsDialog, &DataSourceDialog::dataSourceChanged,
                this, [this]() { saveConfig(); });
    }

    rw.dsDialog->show();
    rw.dsDialog->raise();
    rw.dsDialog->activateWindow();
}

void GraphicsDockWidget::onRemoveTitle(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;

    QString name = m_table->item(row, 0) ? m_table->item(row, 0)->text() : "this title";
    auto answer = QMessageBox::question(
        this, "Remove Title",
        QString("Remove \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    if (row < (int)m_rowWidgets.size() && m_rowWidgets[row].dsDialog) {
        delete m_rowWidgets[row].dsDialog;
        m_rowWidgets[row].dsDialog = nullptr;
    }

    m_table->removeRow(row);
    m_slots.erase(m_slots.begin() + row);
    m_rowWidgets.erase(m_rowWidgets.begin() + row);

    rebuildGlobalList();
    saveConfig();
}

void GraphicsDockWidget::saveConfig()
{
    if (m_loading)
        return;

    AppConfig cfg;
    for (auto& slot : m_slots)
        cfg.titles.push_back({slot->path, slot->dataSourcePath});
    cfg.Save(m_configPath);
}

std::string GraphicsDockWidget::computeConfigPath() const
{
    char* profileRaw = obs_frontend_get_current_profile();
    char* collectionRaw = obs_frontend_get_current_scene_collection();
    std::string profile = sanitizeForPath(profileRaw ? profileRaw : "");
    std::string collection = sanitizeForPath(collectionRaw ? collectionRaw : "");
    bfree(profileRaw);
    bfree(collectionRaw);

    auto dir = std::filesystem::path(m_configDir) / "profiles" / profile / collection;
    std::filesystem::create_directories(dir);
    return (dir / "config.json").string();
}

void GraphicsDockWidget::clearTitles()
{
    for (auto& rw : m_rowWidgets) {
        if (rw.dsDialog) {
            delete rw.dsDialog;
            rw.dsDialog = nullptr;
        }
    }
    m_table->setRowCount(0);
    m_slots.clear();
    m_rowWidgets.clear();
}

void GraphicsDockWidget::reloadForCurrentContext()
{
    clearTitles();
    rebuildGlobalList(); // publish the empty list before touching disk

    m_configPath = computeConfigPath();
    loadConfig();
}

void GraphicsDockWidget::loadConfig()
{
    AppConfig cfg = AppConfig::Load(m_configPath);

    if (cfg.titles.empty()) {
        // One-time migration: pre-4.x versions kept a single config shared by
        // every profile/scene collection at m_configDir/config.json. Adopt it
        // into whichever context is active on first run, then retire it so it
        // isn't re-adopted by the next profile/collection the user switches to.
        auto legacyPath = std::filesystem::path(m_configDir) / "config.json";
        if (std::filesystem::exists(legacyPath)) {
            cfg = AppConfig::Load(legacyPath.string());
            if (!cfg.titles.empty())
                cfg.Save(m_configPath);
            std::error_code ec;
            std::filesystem::rename(legacyPath, legacyPath.string() + ".migrated", ec);
        }
    }

    if (cfg.titles.empty())
        return;

    m_loading = true;

    for (auto& entry : cfg.titles) {
        if (entry.path.empty() || !std::filesystem::exists(entry.path))
            continue;

        auto slot = std::make_shared<TitleSlot>();
        try {
            slot->title = Title::Load(entry.path);
        } catch (...) {
            continue;
        }

        slot->path = entry.path;
        slot->loaded = true;

        // Restore data source if previously configured
        if (!entry.dataSourcePath.empty() && std::filesystem::exists(entry.dataSourcePath)) {
            try {
                std::string ext = std::filesystem::path(entry.dataSourcePath).extension().string();
                std::unique_ptr<IDataSource> ds;
                if (ext == ".csv")
                    ds = std::make_unique<CsvFileDataSource>(entry.dataSourcePath);
                else if (ext == ".lua")
                    ds = std::make_unique<ScriptDataSource>(entry.dataSourcePath);
                else
                    ds = std::make_unique<JsonFileDataSource>(entry.dataSourcePath);

                slot->title.dataSource = ds.get();
                slot->ownedDataSource = std::move(ds);
                slot->dataSourcePath = entry.dataSourcePath;
            } catch (...) {
                // silently skip unloadable data sources on startup
            }
        }

        addTitleRow(slot);
    }

    rebuildGlobalList();
    m_loading = false;
}
