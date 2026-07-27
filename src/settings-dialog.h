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

#include <QDialog>
#include <QLineEdit>
#include <QList>
#include <QString>
#include <QTableWidget>

// One row of the Data Sources tab. A struct rather than a bare path so the
// dock can hand the dialog per-source status it can't work out for itself —
// the dialog does no I/O, so it cannot tell a live source from one whose file
// has been deleted or unmounted.
struct DataSourceRow {
    QString id;            // pool key (a uuid, IDataSource::GetId())
    QString path;          // full path; display-only, not the pool key
    bool missing{false};   // the file is no longer present on disk
};

// App-level (not per-title) settings dialog: editor path plus the shared
// data-source pool. The dialog owns no state and performs no file/pool I/O —
// it only emits requests and renders whatever it's told via setDataSources().
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void setEditorPath(const QString& path);
    void setDataSources(const QList<DataSourceRow>& rows); // rebuilds the table

signals:
    void editorPathChanged(const QString& path);
    void dataSourceAddRequested(const QString& path);       // a not-yet-registered source has no id
    void dataSourceReloadRequested(const QString& id);
    void dataSourceRemoveRequested(const QString& id);

private slots:
    void onBrowseClicked();
    void onSaveClicked();
    void onAddDataSourceClicked();

private:
    QLineEdit* m_editorPathEdit{nullptr};
    QTableWidget* m_dataSourceTable{nullptr};
};
