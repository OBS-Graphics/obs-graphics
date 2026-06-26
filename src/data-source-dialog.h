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

#include <QDialog>
#include <QPushButton>
#include <QTableWidget>

#include <string>

// Modeless dialog for browsing and selecting records from a data source.
// ScenePageWidget owns the IDataSource objects; this dialog only holds a non-owning pointer.
// Ownership of loaded data sources is transferred to ScenePageWidget via the loadRequested signal.
class DataSourceDialog : public QDialog {
    Q_OBJECT
public:
    DataSourceDialog(int graphicIndex, const QString& title, QWidget* parent = nullptr);

    // Update the displayed data source (non-owning). Pass nullptr to clear the table.
    void setDataSource(IDataSource* ds, int selectedRecord = 0);

    // Returns the currently selected row index, or -1 if nothing selected.
    int selectedRecord() const;

signals:
    // Emitted when the user picks a file. ScenePageWidget creates the data source.
    void loadRequested(int graphicIndex, const QString& path);
    // Emitted when the user selects a row. ScenePageWidget sets dataRecordIndex on the graphic.
    void recordSelected(int graphicIndex, int recordIndex);

private slots:
    void onLoadClicked();
    void onSelectionChanged();

private:
    void rebuildTable();

    int m_graphicIndex;
    IDataSource* m_dataSource{nullptr}; // non-owning
    QTableWidget* m_recordTable{nullptr};
};
