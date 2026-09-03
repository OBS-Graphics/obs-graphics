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
#include <QList>
#include <QLineEdit>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QToolButton>

// One column of a manual data source's table. `name` is the element id the
// title looks this column up by; `image` says whether its cells hold a
// filesystem path (shown with a browse button) or plain text.
struct ManualColumnRow {
    QString name;
    bool image{false};
};

// Editor for a "manual input" data source: the operator types the records by
// hand instead of pointing at a CSV or script. Like SettingsDialog and
// TitleSettingsDialog, this dialog owns no pool/scene state and does no I/O
// beyond the QFileDialog used to pick an image path — it only renders what
// it's told via setTable() and hands back the edited result once accepted.
//
// The dialog keeps its own model (m_columns/m_rows) independent of the two
// QTableWidgets; see manual-source-dialog.cpp for why.
class ManualSourceDialog : public QDialog {
    Q_OBJECT
public:
    explicit ManualSourceDialog(QWidget* parent = nullptr);

    // Column-name completions offered while editing the "Element id" cell of
    // the columns table. An id present in `imageIds` flips that column to an
    // Image cell the first time it's typed/selected; textIds does not.
    void setElementIds(const QStringList& textIds, const QStringList& imageIds);

    // Rebuilds the dialog's model and both tables from scratch. `rows` must
    // already be rectangular against `columns` (one QStringList per record,
    // same length as `columns`) — callers reload this dialog from data it
    // previously handed back via rows(), so that invariant always holds.
    void setTable(const QString& name, const QList<ManualColumnRow>& columns,
                  const QList<QStringList>& rows);

    QString tableName() const;
    QList<ManualColumnRow> columns() const;
    QList<QStringList> rows() const;

private:
    // -- columns table -------------------------------------------------
    void rebuildColumnsTable();
    void addColumn();
    void removeSelectedColumn();
    void moveSelectedColumn(int delta);
    // Element-id cell (col 0) changed for the column at `row`, either by the
    // combo's editable line edit or by picking a completion.
    void onColumnNameEdited(int row, const QString& text);
    void onColumnTypeChanged(int row, int index);
    // Locates the row whose col-0 cell widget is `w`. Never trust a captured
    // row index in a per-cell lambda: rebuildColumnsTable() rebuilds every
    // cell whenever a column is added/removed/moved, so any index captured
    // at connect time can go stale by the time the lambda runs. Resolving it
    // from the widget instead, at click/edit time, is immune to that.
    int columnsRowOf(QWidget* w) const;

    // -- records table --------------------------------------------------
    void rebuildRecordsTable();
    void addRecord();
    void removeSelectedRecord();
    void moveSelectedRecord(int delta);
    void onRecordCellChanged(int row, int col);
    void onImageBrowse(QWidget* container);
    // Same staleness hazard as columnsRowOf(), two dimensions instead of one:
    // rebuildRecordsTable() rebuilds every cell whenever a row OR a column
    // list changes, so an image cell's browse button resolves its position
    // from the container widget it lives in rather than a captured (row,col).
    QPair<int, int> recordsPositionOf(QWidget* w) const;

    QLineEdit* m_nameEdit{nullptr};

    QTableWidget* m_columnsTable{nullptr};
    QToolButton* m_columnAddBtn{nullptr};
    QToolButton* m_columnRemoveBtn{nullptr};
    QToolButton* m_columnUpBtn{nullptr};
    QToolButton* m_columnDownBtn{nullptr};

    QTableWidget* m_recordsTable{nullptr};
    QToolButton* m_recordAddBtn{nullptr};
    QToolButton* m_recordRemoveBtn{nullptr};
    QToolButton* m_recordUpBtn{nullptr};
    QToolButton* m_recordDownBtn{nullptr};

    QStringList m_textIds;
    QStringList m_imageIds;

    // The authoritative model. Both tables are always a rendering of this,
    // rebuilt from it wholesale on every structural change (add/remove/move
    // a column or row) — never patched in place — so the two can never drift
    // out of sync with each other. A cell edit writes back into this model
    // immediately, it is never read out of the widgets at accept() time.
    QList<ManualColumnRow> m_columns;
    QList<QStringList> m_rows;

    // Parallel to m_columns: true while a column's type was last set by the
    // imageIds auto-flip rather than by the operator picking the Type combo
    // themselves. Only an auto-flipped column is eligible to be auto-flipped
    // again by a further rename; once the operator touches the Type combo by
    // hand, renaming never overrides their choice again.
    QList<bool> m_columnAutoType;
};
