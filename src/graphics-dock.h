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
#include "data-source-dialog.h"
#include "engine/data-source.h"
#include "shared-scene.h"

#include <QMap>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

// Per-scene tab content: shows the graphics list for one loaded scene.
class ScenePageWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScenePageWidget(std::shared_ptr<SceneSlot> slot, QWidget* parent = nullptr);
    ~ScenePageWidget() override;

    std::shared_ptr<SceneSlot> slot() const { return m_slot; }
    SceneConfig toConfig() const;
    void restoreFromConfig(const SceneConfig& cfg);

signals:
    void configChanged();

private slots:
    void onToggleGraphic(int index);
    void onOpenDataSourceDialog(int index);
    void onLoadRequested(int index, const QString& path);
    void onRecordSelected(int index, int recordIndex);

private:
    void rebuildTable();
    QWidget* buildActionCell(int graphicIndex, bool isVisible);

    std::shared_ptr<SceneSlot> m_slot;
    QTableWidget* m_table{nullptr};
    std::vector<std::unique_ptr<IDataSource>> m_dataSources;
    QMap<int, DataSourceDialog*> m_openDialogs;
};

// Top-level dock widget: tab container with one ScenePageWidget per loaded scene.
class GraphicsDockWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDockWidget(QWidget* parent, std::string configPath);

private slots:
    void onAddTabClicked();
    void onTabCloseRequested(int index);
    void onTabChanged(int index);
    void onChildConfigChanged();

private:
    void addSceneTab(std::shared_ptr<SceneSlot> slot);
    void updateEmptyState();
    void saveConfig();
    void loadConfig();

    QStackedWidget* m_stack{nullptr};
    QTabWidget* m_tabs{nullptr};
    std::string m_configPath;
    bool m_loading{false};
};
