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
#include <QProcess>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>

static const char* kStyleIn    = "QPushButton { background-color: #28752a; color: white; font-weight: bold; }";
static const char* kStyleOut   = "QPushButton { background-color: #b42218; color: white; font-weight: bold; }";
static const char* kStyleTimed = "QPushButton { background-color: #1a5fb4; color: white; font-weight: bold; }";

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
    m_settingsPath = (std::filesystem::path(m_configDir) / "settings.json").string();
    m_settings = AppSettings::Load(m_settingsPath);

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

    auto* appSettingsBtn = new QToolButton();
    appSettingsBtn->setIcon(themedIcon(Icons16::Navigation_Settings));
    appSettingsBtn->setIconSize(QSize(16, 16));
    appSettingsBtn->setAutoRaise(true);
    appSettingsBtn->setToolTip("Settings...");
    connect(appSettingsBtn, &QToolButton::clicked, this, &GraphicsDockWidget::onAppSettingsClicked);
    toolbarLayout->addWidget(appSettingsBtn);

    m_openEditorBtn = new QPushButton(themedIcon(Icons16::Action_ExternalLink), " Open Editor");
    connect(m_openEditorBtn, &QPushButton::clicked, this, &GraphicsDockWidget::onOpenEditorClicked);
    toolbarLayout->addWidget(m_openEditorBtn);
    updateOpenEditorEnabled();

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

    // Actions cell: [Toggle In/Out] [Settings] [Remove]
    auto* actions = new QWidget();
    auto* hbox = new QHBoxLayout(actions);
    hbox->setContentsMargins(2, 2, 2, 2);
    hbox->setSpacing(2);

    auto* toggleBtn = new QPushButton(themedIcon(Icons16::Media_Play), " In");
    toggleBtn->setStyleSheet(kStyleIn);
    hbox->addWidget(toggleBtn, 1);

    auto* settingsBtn = new QToolButton();
    settingsBtn->setIcon(themedIcon(Icons16::Navigation_Settings));
    settingsBtn->setIconSize(QSize(16, 16));
    settingsBtn->setAutoRaise(true);
    settingsBtn->setFixedWidth(28);
    settingsBtn->setToolTip("Settings...");
    hbox->addWidget(settingsBtn);

    auto* reloadBtn = new QToolButton();
    reloadBtn->setIcon(themedIcon(Icons16::Action_Refresh));
    reloadBtn->setIconSize(QSize(16, 16));
    reloadBtn->setAutoRaise(true);
    reloadBtn->setFixedWidth(28);
    reloadBtn->setToolTip("Reload from file");
    hbox->addWidget(reloadBtn);

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
    rw.reloadBtn = reloadBtn;
    rw.isIn = false;
    m_rowWidgets.push_back(rw);
    m_slots.push_back(std::move(slot));

    // Use slot raw pointer for stable identity across vector mutations
    auto* slotPtr = m_slots.back().get();

    connect(toggleBtn, &QPushButton::clicked, this, [this, slotPtr]() {
        int row = rowForSlot(slotPtr);
        if (row >= 0)
            onToggle(row);
    });

    connect(settingsBtn, &QToolButton::clicked, this, [this, slotPtr]() {
        int row = rowForSlot(slotPtr);
        if (row >= 0)
            onDataSource(row);
    });

    connect(reloadBtn, &QToolButton::clicked, this, [this, slotPtr]() {
        int row = rowForSlot(slotPtr);
        if (row >= 0)
            onReloadTitle(row);
    });

    connect(removeBtn, &QToolButton::clicked, this, [this, slotPtr]() {
        int row = rowForSlot(slotPtr);
        if (row >= 0)
            onRemoveTitle(row);
    });

    // Keep the dock's toggle button in sync with the Title's actual state,
    // regardless of what triggered the change: a duration timeout (fired from
    // the render thread inside Tick()) or a Lua script calling trigger_in()/
    // trigger_out() on itself.
    registerTriggerCallbacks(slotPtr);
}

void GraphicsDockWidget::registerTriggerCallbacks(TitleSlot* slotPtr)
{
    // Both onTriggerIn/onTriggerOut fire under slot->mutex already held by
    // the caller (a direct host call, a duration timeout fired from the
    // render thread inside Tick(), or a Lua script's trigger_in()/
    // trigger_out()), so these lambdas must stay non-blocking — they only
    // post a queued UI update. The context-object overload of invokeMethod
    // safely no-ops if this dock has since been destroyed.
    slotPtr->title.onTriggerIn.push_back([this, slotPtr](size_t, double) {
        QMetaObject::invokeMethod(this, [this, slotPtr]() {
            int row = rowForSlot(slotPtr);
            if (row >= 0)
                applyRowState(row, true);
        }, Qt::QueuedConnection);
    });
    slotPtr->title.onTriggerOut.push_back([this, slotPtr]() {
        QMetaObject::invokeMethod(this, [this, slotPtr]() {
            int row = rowForSlot(slotPtr);
            if (row >= 0)
                applyRowState(row, false);
        }, Qt::QueuedConnection);
    });
}

int GraphicsDockWidget::rowForSlot(TitleSlot* slotPtr) const
{
    auto it = std::find_if(m_slots.begin(), m_slots.end(),
        [slotPtr](const auto& s) { return s.get() == slotPtr; });
    if (it == m_slots.end())
        return -1;
    return static_cast<int>(it - m_slots.begin());
}

void GraphicsDockWidget::applyRowState(int row, bool isIn)
{
    if (row < 0 || row >= (int)m_rowWidgets.size())
        return;

    auto& slot = m_slots[row];
    auto& rw = m_rowWidgets[row];
    rw.isIn = isIn;
    rw.reloadBtn->setEnabled(!isIn);

    if (!isIn) {
        rw.toggleBtn->setText(" In");
        rw.toggleBtn->setIcon(themedIcon(Icons16::Media_Play));
        rw.toggleBtn->setStyleSheet(kStyleIn);
        return;
    }

    double duration;
    {
        std::lock_guard<std::mutex> lock(slot->mutex);
        duration = slot->duration;
    }
    bool timed = duration >= 0.0;
    rw.toggleBtn->setText(" Out");
    rw.toggleBtn->setIcon(themedIcon(timed ? Icons16::Misc_Clock : Icons16::Media_Stop));
    rw.toggleBtn->setStyleSheet(timed ? kStyleTimed : kStyleOut);
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

    // Don't touch the button here — Title::TriggerIn/TriggerOut below
    // synchronously fires the onTriggerIn/onTriggerOut callback registered in
    // addTitleRow, which applies the row state via a queued UI update. Doing
    // it again here would apply it twice per click.
    bool nowIn = !rw.isIn;
    std::lock_guard<std::mutex> lock(slot->mutex);
    if (nowIn) {
        size_t recIdx = 0;
        if (rw.settingsDialog) {
            int sel = rw.settingsDialog->selectedRecord();
            if (sel >= 0)
                recIdx = static_cast<size_t>(sel);
        }
        slot->title.TriggerIn(recIdx, slot->duration);
    } else {
        slot->title.TriggerOut();
    }
}

void GraphicsDockWidget::onDataSource(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;

    auto& rw = m_rowWidgets[row];
    if (!rw.settingsDialog) {
        QString name = m_table->item(row, 0) ? m_table->item(row, 0)->text() : "Title";
        rw.settingsDialog = new TitleSettingsDialog(name, this);
        rw.settingsDialog->setSlot(m_slots[row].get());
        connect(rw.settingsDialog, &TitleSettingsDialog::configChanged,
                this, [this]() { saveConfig(); });
    }

    rw.settingsDialog->show();
    rw.settingsDialog->raise();
    rw.settingsDialog->activateWindow();
}

void GraphicsDockWidget::onReloadTitle(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;
    if (m_rowWidgets[row].isIn)
        return; // blocked while visible; button should already be disabled

    auto& slot = m_slots[row];

    Title newTitle;
    try {
        newTitle = Title::Load(slot->path);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Reload Title",
                              QString("Could not reload title:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Reload Title",
                              "Could not reload title: unknown error.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(slot->mutex);
        newTitle.dataSource = slot->ownedDataSource.get();
        slot->title = std::move(newTitle);
        registerTriggerCallbacks(slot.get());
    }
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

    if (row < (int)m_rowWidgets.size() && m_rowWidgets[row].settingsDialog) {
        delete m_rowWidgets[row].settingsDialog;
        m_rowWidgets[row].settingsDialog = nullptr;
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
        cfg.titles.push_back({slot->path, slot->dataSourcePath, slot->duration});
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
        if (rw.settingsDialog) {
            delete rw.settingsDialog;
            rw.settingsDialog = nullptr;
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
        slot->duration = entry.duration;

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

void GraphicsDockWidget::updateOpenEditorEnabled()
{
    bool enabled = !m_settings.editorPath.empty() &&
                   std::filesystem::exists(m_settings.editorPath);
    m_openEditorBtn->setEnabled(enabled);
}

void GraphicsDockWidget::onAppSettingsClicked()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
        connect(m_settingsDialog, &SettingsDialog::editorPathChanged, this, [this](const QString& path) {
            m_settings.editorPath = path.toStdString();
            m_settings.Save(m_settingsPath);
            updateOpenEditorEnabled();
        });
    }

    m_settingsDialog->setEditorPath(QString::fromStdString(m_settings.editorPath));
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void GraphicsDockWidget::onOpenEditorClicked()
{
    if (m_settings.editorPath.empty())
        return;
    QProcess::startDetached(QString::fromStdString(m_settings.editorPath));
}
