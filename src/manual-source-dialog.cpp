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

#include "manual-source-dialog.h"
#include "icons.h"
#include "ui-util.h"

#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QSplitter>
#include <QVBoxLayout>

ManualSourceDialog::ManualSourceDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Manual Data Source");
    setMinimumSize(640, 480);
    setWindowFlags(windowFlags() | Qt::Window);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // ── Name ─────────────────────────────────────────────────────────────
    auto* form = new QFormLayout();
    m_nameEdit = new QLineEdit();
    form->addRow(new QLabel("Name:"), m_nameEdit);
    layout->addLayout(form);

    auto* splitter = new QSplitter(Qt::Vertical);
    layout->addWidget(splitter, 1);

    // ── Columns ──────────────────────────────────────────────────────────
    auto* columnsPane = new QWidget();
    auto* columnsLayout = new QVBoxLayout(columnsPane);
    columnsLayout->setContentsMargins(0, 0, 0, 0);
    columnsLayout->setSpacing(4);

    auto* columnsHeader = new QHBoxLayout();
    columnsHeader->addWidget(new QLabel("Columns"));
    columnsHeader->addStretch();
    m_columnAddBtn = makeIconButton(Icons16::Action_Plus, "Add column");
    m_columnRemoveBtn = makeIconButton(Icons16::Action_Trash, "Remove column");
    m_columnUpBtn = makeIconButton(Icons16::Navigation_ArrowUp, "Move column up");
    m_columnDownBtn = makeIconButton(Icons16::Navigation_ArrowDown, "Move column down");
    columnsHeader->addWidget(m_columnAddBtn);
    columnsHeader->addWidget(m_columnRemoveBtn);
    columnsHeader->addWidget(m_columnUpBtn);
    columnsHeader->addWidget(m_columnDownBtn);
    columnsLayout->addLayout(columnsHeader);

    connect(m_columnAddBtn, &QToolButton::clicked, this, &ManualSourceDialog::addColumn);
    connect(m_columnRemoveBtn, &QToolButton::clicked, this, &ManualSourceDialog::removeSelectedColumn);
    connect(m_columnUpBtn, &QToolButton::clicked, this, [this]() { moveSelectedColumn(-1); });
    connect(m_columnDownBtn, &QToolButton::clicked, this, [this]() { moveSelectedColumn(1); });

    m_columnsTable = new QTableWidget(0, 2);
    m_columnsTable->setHorizontalHeaderLabels({"Element id", "Type"});
    m_columnsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_columnsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_columnsTable->verticalHeader()->setVisible(false);
    m_columnsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_columnsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    // Both cells are QComboBoxes set via setCellWidget(), so there is nothing
    // here for the base QTableWidgetItem edit machinery to trigger — leave
    // the default edit triggers, they never fire.
    columnsLayout->addWidget(m_columnsTable);

    splitter->addWidget(columnsPane);

    // ── Records ──────────────────────────────────────────────────────────
    auto* recordsPane = new QWidget();
    auto* recordsLayout = new QVBoxLayout(recordsPane);
    recordsLayout->setContentsMargins(0, 0, 0, 0);
    recordsLayout->setSpacing(4);

    auto* recordsHeader = new QHBoxLayout();
    recordsHeader->addWidget(new QLabel("Records"));
    recordsHeader->addStretch();
    m_recordAddBtn = makeIconButton(Icons16::Action_Plus, "Add record");
    m_recordRemoveBtn = makeIconButton(Icons16::Action_Trash, "Remove record");
    m_recordUpBtn = makeIconButton(Icons16::Navigation_ArrowUp, "Move record up");
    m_recordDownBtn = makeIconButton(Icons16::Navigation_ArrowDown, "Move record down");
    recordsHeader->addWidget(m_recordAddBtn);
    recordsHeader->addWidget(m_recordRemoveBtn);
    recordsHeader->addWidget(m_recordUpBtn);
    recordsHeader->addWidget(m_recordDownBtn);
    recordsLayout->addLayout(recordsHeader);

    connect(m_recordAddBtn, &QToolButton::clicked, this, &ManualSourceDialog::addRecord);
    connect(m_recordRemoveBtn, &QToolButton::clicked, this, &ManualSourceDialog::removeSelectedRecord);
    connect(m_recordUpBtn, &QToolButton::clicked, this, [this]() { moveSelectedRecord(-1); });
    connect(m_recordDownBtn, &QToolButton::clicked, this, [this]() { moveSelectedRecord(1); });

    m_recordsTable = new QTableWidget(0, 0);
    m_recordsTable->verticalHeader()->setVisible(false);
    m_recordsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recordsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    // The one editable table in this codebase — every other QTableWidget here
    // is NoEditTriggers because it only ever displays host-owned data. This
    // one *is* the operator's data entry surface, so text cells are edited
    // in place.
    m_recordsTable->setEditTriggers(QAbstractItemView::DoubleClicked |
                                     QAbstractItemView::SelectedClicked |
                                     QAbstractItemView::EditKeyPressed);
    // A single connection for the whole table rather than one per cell: the
    // signal hands us the item that actually changed, so row()/column() are
    // read fresh at fire time and can never go stale the way a captured
    // index in a per-cell lambda would (see columnsRowOf()/recordsPositionOf()
    // below for the case where that hazard is real).
    connect(m_recordsTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item)
            onRecordCellChanged(item->row(), item->column());
    });
    recordsLayout->addWidget(m_recordsTable);

    splitter->addWidget(recordsPane);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    // ── Ok / Cancel ──────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ManualSourceDialog::setElementIds(const QStringList& textIds, const QStringList& imageIds)
{
    m_textIds = textIds;
    m_imageIds = imageIds;
    rebuildColumnsTable(); // completer contents depend on these
}

void ManualSourceDialog::setTable(const QString& name, const QList<ManualColumnRow>& columns,
                                   const QList<QStringList>& rows)
{
    m_nameEdit->setText(name);
    m_columns = columns;
    m_rows = rows;
    // A freshly-loaded table has no notion of "the operator already picked a
    // type by hand" — treat every incoming column as eligible for the
    // imageIds auto-flip, same as a brand new column would be.
    m_columnAutoType.clear();
    for (int i = 0; i < m_columns.size(); ++i)
        m_columnAutoType.push_back(true);

    rebuildColumnsTable();
    rebuildRecordsTable();
}

QString ManualSourceDialog::tableName() const
{
    return m_nameEdit->text();
}

QList<ManualColumnRow> ManualSourceDialog::columns() const
{
    return m_columns;
}

QList<QStringList> ManualSourceDialog::rows() const
{
    return m_rows;
}

// ── Columns table ───────────────────────────────────────────────────────

void ManualSourceDialog::rebuildColumnsTable()
{
    QSignalBlocker tableBlocker(m_columnsTable);
    m_columnsTable->clearContents();
    // Drop to zero rows and grow back up, rather than resizing straight to
    // the target count: shrinking a QTableWidget removes rows from its model,
    // which is what actually tears down their setCellWidget() combos. Only
    // resizing (e.g. from 3 rows to 3 rows, on a rename) would otherwise
    // leave the old combos as orphaned overlays on top of the new ones.
    m_columnsTable->setRowCount(0);

    QStringList completions = m_textIds;
    completions += m_imageIds;

    for (int row = 0; row < m_columns.size(); ++row) {
        m_columnsTable->insertRow(row);
        const ManualColumnRow& col = m_columns[row];

        auto* nameCombo = new QComboBox();
        nameCombo->setEditable(true);
        nameCombo->setInsertPolicy(QComboBox::NoInsert);
        auto* completer = new QCompleter(completions, nameCombo);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        nameCombo->setCompleter(completer);
        {
            // Set the initial contents before connecting — addItems() and
            // setCurrentText() would otherwise fire editTextChanged() back
            // into onColumnNameEdited() and re-derive the model from itself
            // mid-rebuild.
            QSignalBlocker comboBlocker(nameCombo);
            nameCombo->addItems(completions);
            nameCombo->setCurrentText(col.name);
        }
        // Capture the combo pointer, not a row index — the row is resolved
        // from the widget at edit time via columnsRowOf() because Add/
        // Remove/Move rebuild every row's widgets, which would otherwise
        // leave a captured index pointing at the wrong row.
        connect(nameCombo, &QComboBox::editTextChanged, this, [this, nameCombo](const QString& text) {
            int r = columnsRowOf(nameCombo);
            if (r >= 0)
                onColumnNameEdited(r, text);
        });
        m_columnsTable->setCellWidget(row, 0, nameCombo);

        auto* typeCombo = new QComboBox();
        {
            QSignalBlocker comboBlocker(typeCombo);
            typeCombo->addItem("Text");
            typeCombo->addItem("Image");
            typeCombo->setCurrentIndex(col.image ? 1 : 0);
        }
        connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, typeCombo](int index) {
                    int r = columnsRowOf(typeCombo);
                    if (r >= 0)
                        onColumnTypeChanged(r, index);
                });
        m_columnsTable->setCellWidget(row, 1, typeCombo);
    }

    m_recordAddBtn->setEnabled(!m_columns.isEmpty());
}

void ManualSourceDialog::addColumn()
{
    m_columns.push_back(ManualColumnRow{});
    m_columnAutoType.push_back(true);
    for (QStringList& row : m_rows)
        row.push_back(QString());

    rebuildColumnsTable();
    rebuildRecordsTable();
}

void ManualSourceDialog::removeSelectedColumn()
{
    int row = m_columnsTable->currentRow();
    if (row < 0 || row >= m_columns.size())
        return; // nothing selected — no-op

    m_columns.removeAt(row);
    m_columnAutoType.removeAt(row);
    for (QStringList& r : m_rows)
        r.removeAt(row);

    rebuildColumnsTable();
    rebuildRecordsTable();
}

void ManualSourceDialog::moveSelectedColumn(int delta)
{
    int row = m_columnsTable->currentRow();
    int target = row + delta;
    if (row < 0 || target < 0 || target >= m_columns.size())
        return; // no selection, or already at the end in that direction — no-op

    m_columns.move(row, target);
    m_columnAutoType.move(row, target);
    for (QStringList& r : m_rows)
        r.move(row, target);

    rebuildColumnsTable();
    rebuildRecordsTable();
    m_columnsTable->selectRow(target);
}

void ManualSourceDialog::onColumnNameEdited(int row, const QString& text)
{
    if (row < 0 || row >= m_columns.size())
        return;

    m_columns[row].name = text;

    // Auto-flip to Image the first time the name matches a known image
    // element id — but only while this column's type is still "automatic".
    // Once the operator has picked a type by hand (onColumnTypeChanged),
    // renaming never overrides that choice again.
    if (m_columnAutoType[row] && !m_columns[row].image && m_imageIds.contains(text)) {
        m_columns[row].image = true;
        auto* typeCombo = qobject_cast<QComboBox*>(m_columnsTable->cellWidget(row, 1));
        if (typeCombo) {
            QSignalBlocker blocker(typeCombo); // reflect the model, don't re-enter as a manual change
            typeCombo->setCurrentIndex(1);
        }
        rebuildRecordsTable(); // this column's cells now render as image cells
    }
}

void ManualSourceDialog::onColumnTypeChanged(int row, int index)
{
    if (row < 0 || row >= m_columns.size())
        return;

    m_columns[row].image = (index == 1);
    m_columnAutoType[row] = false; // the operator touched this by hand — stop auto-flipping it
    rebuildRecordsTable();
}

int ManualSourceDialog::columnsRowOf(QWidget* w) const
{
    for (int r = 0; r < m_columnsTable->rowCount(); ++r) {
        if (m_columnsTable->cellWidget(r, 0) == w || m_columnsTable->cellWidget(r, 1) == w)
            return r;
    }
    return -1;
}

// ── Records table ───────────────────────────────────────────────────────

void ManualSourceDialog::rebuildRecordsTable()
{
    QSignalBlocker tableBlocker(m_recordsTable);
    m_recordsTable->clearContents();
    // Same reasoning as rebuildColumnsTable(): drop to zero rows/columns and
    // grow back up so any image column's cell-widget containers are actually
    // torn down (by row/column removal) rather than left as stale overlays
    // under freshly-set QTableWidgetItems when a column flips text<->image.
    m_recordsTable->setRowCount(0);
    m_recordsTable->setColumnCount(m_columns.size());

    QStringList headers;
    for (const ManualColumnRow& col : m_columns)
        headers << (col.name.isEmpty() ? QString("(unnamed)") : col.name);
    m_recordsTable->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < m_rows.size(); ++row) {
        m_recordsTable->insertRow(row);
        const QStringList& rec = m_rows[row];
        for (int col = 0; col < m_columns.size(); ++col) {
            // setTable()'s contract guarantees rows are already padded to
            // m_columns.size(), and every structural edit below keeps that
            // invariant — but guard anyway rather than reading out of range.
            QString value = col < rec.size() ? rec[col] : QString();

            if (m_columns[col].image) {
                auto* container = new QWidget();
                auto* hbox = new QHBoxLayout(container);
                hbox->setContentsMargins(2, 2, 2, 2);
                hbox->setSpacing(2);

                auto* pathEdit = new QLineEdit(value);
                pathEdit->setReadOnly(true);
                hbox->addWidget(pathEdit, 1);

                auto* browseBtn = makeIconButton(Icons16::File_FolderOpen, "Browse...");
                hbox->addWidget(browseBtn);

                // Capture the container, not (row, col) — Add/Remove/Move on
                // either table rebuilds every cell, which would leave a
                // captured position stale. onImageBrowse() re-derives the
                // current (row, col) from this same container via
                // recordsPositionOf() at click time.
                connect(browseBtn, &QToolButton::clicked, this, [this, container]() {
                    onImageBrowse(container);
                });

                m_recordsTable->setCellWidget(row, col, container);
            } else {
                m_recordsTable->setItem(row, col, new QTableWidgetItem(value));
            }
        }
    }
}

void ManualSourceDialog::addRecord()
{
    if (m_columns.isEmpty())
        return; // guarded by m_recordAddBtn's enabled state too, but no-op defensively

    QStringList row;
    for (int i = 0; i < m_columns.size(); ++i)
        row.push_back(QString());
    m_rows.push_back(row);

    rebuildRecordsTable();
}

void ManualSourceDialog::removeSelectedRecord()
{
    int row = m_recordsTable->currentRow();
    if (row < 0 || row >= m_rows.size())
        return; // nothing selected — no-op

    m_rows.removeAt(row);
    rebuildRecordsTable();
}

void ManualSourceDialog::moveSelectedRecord(int delta)
{
    int row = m_recordsTable->currentRow();
    int target = row + delta;
    if (row < 0 || target < 0 || target >= m_rows.size())
        return; // no selection, or already at the end in that direction — no-op

    m_rows.move(row, target);
    rebuildRecordsTable();
    m_recordsTable->selectRow(target);
}

void ManualSourceDialog::onRecordCellChanged(int row, int col)
{
    if (row < 0 || row >= m_rows.size() || col < 0 || col >= m_columns.size())
        return;
    if (m_columns[col].image)
        return; // image cells are a cellWidget, not a QTableWidgetItem — nothing to read here

    QTableWidgetItem* item = m_recordsTable->item(row, col);
    if (!item)
        return;

    m_rows[row][col] = item->text();
}

void ManualSourceDialog::onImageBrowse(QWidget* container)
{
    QPair<int, int> pos = recordsPositionOf(container);
    int row = pos.first, col = pos.second;
    if (row < 0 || col < 0)
        return;

    QString current = m_rows[row][col];
    QString path = QFileDialog::getOpenFileName(
        this, "Select Image", current,
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;All Files (*)");
    if (path.isEmpty())
        return;

    m_rows[row][col] = path;

    // Update the existing line edit in place rather than rebuilding the
    // whole records table — a structural rebuild isn't needed for a value
    // edit, and would needlessly drop the current row selection.
    auto* pathEdit = container->findChild<QLineEdit*>();
    if (pathEdit)
        pathEdit->setText(path);
}

QPair<int, int> ManualSourceDialog::recordsPositionOf(QWidget* w) const
{
    for (int r = 0; r < m_recordsTable->rowCount(); ++r) {
        for (int c = 0; c < m_recordsTable->columnCount(); ++c) {
            if (m_recordsTable->cellWidget(r, c) == w)
                return {r, c};
        }
    }
    return {-1, -1};
}
