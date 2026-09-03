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

#include "shared-title.h"

#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>

// Per-title dialog: "Data Source" tab (a combo box binding this title to one
// of g_scene.Pool()'s registered source ids, plus the resulting record table)
// and a "Settings" tab for title-level options (currently: auto-hide duration).
// Loading/reloading/removing a data source itself now lives one level up, in
// the app-level SettingsDialog's "Data Sources" tab — this dialog only picks
// which already-registered id (if any) this title is bound to.
class TitleSettingsDialog : public QDialog {
    Q_OBJECT
public:
    TitleSettingsDialog(const QString& titleName, QWidget* parent = nullptr);

    int selectedRecord() const;

    // Repopulates the combo from g_scene.Pool().Ids() and re-selects this
    // title's current dataSourceId. Called by GraphicsDockWidget on every
    // open dialog after any mutation of the pool's registry (add/reload/
    // remove from the app-level Data Sources tab).
    void refreshDataSourceList();

signals:
    void configChanged();
    // Entry points into the per-title "New manual data source..."/"Edit
    // manual data..." buttons. This dialog owns no pool state, so it only
    // asks — GraphicsDockWidget does the Pool().Add()/SetTable() and the
    // rebind. manualSourceEditRequested carries the currently bound source's
    // id (only emitted while m_manualEditBtn is enabled, i.e. it resolves to
    // a ManualDataSource).
    void manualSourceCreateRequested();
    void manualSourceEditRequested(const QString& id);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onDataSourceChanged(int index);
    void onSelectionChanged();
    void onDurationChanged();
    void updateScriptErrorLabel();

private:
    void rebuildTable();
    // Enables m_manualEditBtn only when the currently bound source resolves
    // to a ManualDataSource — same dynamic_cast-on-the-UI-thread precedent
    // updateScriptErrorLabel() already uses for ScriptDataSource. Called from
    // refreshDataSourceList() and onDataSourceChanged() so it can never go
    // stale relative to what the combo is actually pointed at.
    void updateManualEditEnabled();

    TitleRow* m_row{nullptr};
    QComboBox* m_dataSourceCombo{nullptr};
    QToolButton* m_manualCreateBtn{nullptr};
    QToolButton* m_manualEditBtn{nullptr};
    QLabel* m_scriptErrorLabel{nullptr};
    QTableWidget* m_recordTable{nullptr};
    QGroupBox* m_durationGroup{nullptr};
    QDoubleSpinBox* m_durationSpin{nullptr};
    QTimer* m_scriptErrorTimer{nullptr};

    friend class GraphicsDockWidget;
    void setRow(TitleRow* row);
};
