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

#include "data-source-dialog.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

DataSourceDialog::DataSourceDialog(int graphicIndex, const QString& title, QWidget* parent)
    : QDialog(parent), m_graphicIndex(graphicIndex)
{
    setWindowTitle(title);
    setMinimumSize(480, 320);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowFlags(windowFlags() | Qt::Window);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Toolbar
    auto* toolbar = new QHBoxLayout();
    auto* loadBtn = new QPushButton("Load Data Source...");
    connect(loadBtn, &QPushButton::clicked, this, &DataSourceDialog::onLoadClicked);
    toolbar->addWidget(loadBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    // Records table
    m_recordTable = new QTableWidget(0, 0);
    m_recordTable->horizontalHeader()->setStretchLastSection(true);
    m_recordTable->verticalHeader()->setVisible(false);
    m_recordTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recordTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recordTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_recordTable, &QTableWidget::itemSelectionChanged, this,
            &DataSourceDialog::onSelectionChanged);
    layout->addWidget(m_recordTable, 1);

    // Footer
    auto* footer = new QHBoxLayout();
    footer->addStretch();
    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    footer->addWidget(closeBtn);
    layout->addLayout(footer);
}

void DataSourceDialog::setDataSource(IDataSource* ds, int selectedRecord)
{
    m_dataSource = ds;
    rebuildTable();

    if (ds && selectedRecord >= 0 && selectedRecord < m_recordTable->rowCount()) {
        m_recordTable->selectRow(selectedRecord);
    }
}

int DataSourceDialog::selectedRecord() const
{
    auto selected = m_recordTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    return m_recordTable->row(selected.first());
}

void DataSourceDialog::onLoadClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Load Data Source", QString(),
        "Data Files (*.json *.csv *.lua);;JSON Files (*.json);;CSV Files (*.csv);;Lua Scripts (*.lua)");
    if (!path.isEmpty())
        emit loadRequested(m_graphicIndex, path);
}

void DataSourceDialog::onSelectionChanged()
{
    int row = selectedRecord();
    if (row >= 0)
        emit recordSelected(m_graphicIndex, row);
}

void DataSourceDialog::rebuildTable()
{
    // Block signals while rebuilding to avoid spurious recordSelected emissions
    QSignalBlocker blocker(m_recordTable);

    m_recordTable->clearContents();
    m_recordTable->setRowCount(0);
    m_recordTable->setColumnCount(0);

    if (!m_dataSource)
        return;

    std::vector<Record> records;
    try {
        records = m_dataSource->GetData();
    } catch (...) {
        return;
    }

    if (records.empty())
        return;

    // Collect all column names from all records (preserves order from first record)
    QStringList columns;
    for (auto& [key, _] : records.front())
        columns << QString::fromStdString(key);
    // Add any keys from subsequent records not in the first
    for (auto& rec : records) {
        for (auto& [key, _] : rec) {
            QString qkey = QString::fromStdString(key);
            if (!columns.contains(qkey))
                columns << qkey;
        }
    }

    m_recordTable->setColumnCount(columns.size());
    m_recordTable->setHorizontalHeaderLabels(columns);
    m_recordTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    if (!columns.isEmpty())
        m_recordTable->horizontalHeader()->setSectionResizeMode(columns.size() - 1,
                                                                QHeaderView::Stretch);

    for (auto& rec : records) {
        int row = m_recordTable->rowCount();
        m_recordTable->insertRow(row);
        for (int col = 0; col < columns.size(); ++col) {
            auto it = rec.find(columns[col].toStdString());
            QString val = (it != rec.end()) ? QString::fromStdString(it->second) : QString();
            m_recordTable->setItem(row, col, new QTableWidgetItem(val));
        }
    }
}
