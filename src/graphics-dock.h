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

#include "engine/data-source.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class GraphicsDockWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDockWidget(QWidget* parent, std::string configPath);

private slots:
    void onLoadClicked();
    void onToggleGraphic(int graphicIndex);

private:
    void rebuildTable();
    void rebuildDataSourceCell(int row, int graphicIndex);
    void onLoadDataSource(int graphicIndex);
    void onRemoveDataSource(int graphicIndex);
    void saveConfig();
    void loadConfig();

    QPushButton* m_loadBtn;
    QTableWidget* m_table;

    std::vector<std::unique_ptr<IDataSource>> m_dataSources;

    std::string m_configPath;
    std::string m_scenePath;
    bool m_loading{false};
};
