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

#pragma once

#include "app-config.h"
#include "app-settings.h"
#include "settings-dialog.h"
#include "shared-title.h"
#include "title-settings-dialog.h"

#include <QTableWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

struct RowWidgets {
    QPushButton* toggleBtn{nullptr};
    QToolButton* reloadBtn{nullptr};
    TitleSettingsDialog* settingsDialog{nullptr};
    bool isIn{false};
};

// Flat dock widget: one row per loaded title, with a toggle In/Out button,
// a per-title settings button, and a remove button.
class GraphicsDockWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDockWidget(QWidget* parent, std::string configDir);

    // Recomputes the config path for the current OBS profile + scene collection
    // and (re)loads titles for that context. Called on startup (once OBS reports
    // the active profile/collection) and whenever either one changes.
    void reloadForCurrentContext();

private slots:
    void onAddTitleClicked();
    void onToggle(int row);
    void onDataSource(int row);
    void onReloadTitle(int row);
    void onRemoveTitle(int row);
    void onAppSettingsClicked();
    void onOpenEditorClicked();

private:
    void addTitleRow(std::unique_ptr<TitleRow> row);
    // Registers the queued dock-side listeners that keep a row's toggle
    // button in sync with its Title's actual state. MUST be called with
    // g_scene_mutex already held by the caller (it pushes onto the fresh
    // Title's onTriggerIn/onTriggerOut vectors) — every call site is right
    // next to the AddTitle (or RemoveTitle+AddTitle) call that just produced
    // the Title being wired up.
    void registerTriggerCallbacks(TitleRow* rowPtr);
    // Reassigns row->title->zOrder = <index in m_rows> for every row, so the
    // dock's visible top-to-bottom order stays Scene::Render's stable-sorted
    // render order. Takes g_scene_mutex itself; must NOT be called with it
    // already held. Called after add, remove, and reload.
    void reassignZOrders();
    void saveConfig();
    void loadConfig();
    void clearTitles();
    std::string computeConfigPath() const;
    int rowIndexFor(TitleRow* rowPtr) const;
    void applyRowState(int row, bool isIn);
    // Puts the toggle button into the "fetching" look — greyed out and
    // unclickable — for the span of onToggle's off-thread DataBlocking call.
    // Cleared by whichever applyRowState() runs next, not by a paired call.
    void applyRowLoading(int row);
    void updateOpenEditorEnabled();
    // Refreshes the app SettingsDialog's data-source table and every open
    // TitleSettingsDialog's combo box. Called after any mutation of
    // g_scene.Pool()'s registry (add/reload/remove from the Data Sources tab).
    void refreshDataSourceUi();
    // Display name for a row: row->title->name if it has one, else the file
    // stem of row->path. Takes g_scene_mutex itself to read the name; must
    // NOT be called with it already held.
    QString rowDisplayName(TitleRow* row) const;

    QTableWidget* m_table{nullptr};
    // Stable addresses matter here: dialogs (TitleSettingsDialog::m_row) and
    // the trigger callbacks registered in registerTriggerCallbacks hold raw
    // TitleRow* into this vector, so rows are never moved/reallocated in
    // place — only appended, or erased (which is the one case a held
    // TitleRow* must first be retired: onRemoveTitle deletes the row's open
    // dialog before erasing).
    std::vector<std::unique_ptr<TitleRow>> m_rows;
    std::vector<RowWidgets> m_rowWidgets;
    std::string m_configDir;
    std::string m_configPath;
    bool m_loading{false};

    QPushButton* m_openEditorBtn{nullptr};
    SettingsDialog* m_settingsDialog{nullptr};
    AppSettings m_settings;
    std::string m_settingsPath;
};
