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

#pragma once

#include "app-config.h"
#include "shared-title.h"

#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

// Flat dock widget: one row per loaded title, with Trigger In / Trigger Out / Remove actions.
class GraphicsDockWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDockWidget(QWidget* parent, std::string configPath);

private slots:
    void onAddTitleClicked();
    void onTriggerIn(int row);
    void onTriggerOut(int row);
    void onRemoveTitle(int row);

private:
    void addTitleRow(std::shared_ptr<TitleSlot> slot);
    void rebuildGlobalList();
    void saveConfig();
    void loadConfig();

    QTableWidget* m_table{nullptr};
    std::vector<std::shared_ptr<TitleSlot>> m_slots; // parallel to table rows, UI-thread only
    std::string m_configPath;
    bool m_loading{false};
};
