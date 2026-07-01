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
#include "data-source-dialog.h"
#include "shared-title.h"

#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

struct RowWidgets {
    QPushButton* toggleBtn{nullptr};
    DataSourceDialog* dsDialog{nullptr};
    bool isIn{false};
};

// Flat dock widget: one row per loaded title, with a toggle In/Out button,
// a data-source config button, and a remove button.
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
    void onRemoveTitle(int row);

private:
    void addTitleRow(std::shared_ptr<TitleSlot> slot);
    void rebuildGlobalList();
    void saveConfig();
    void loadConfig();
    void clearTitles();
    std::string computeConfigPath() const;

    QTableWidget* m_table{nullptr};
    std::vector<std::shared_ptr<TitleSlot>> m_slots;
    std::vector<RowWidgets> m_rowWidgets;
    std::string m_configDir;
    std::string m_configPath;
    bool m_loading{false};
};
