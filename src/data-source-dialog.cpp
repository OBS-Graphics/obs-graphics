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
#include "engine/script.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <filesystem>

DataSourceDialog::DataSourceDialog(const QString& titleName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Data Source — %1").arg(titleName));
    setMinimumSize(480, 320);
    setWindowFlags(windowFlags() | Qt::Window);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* toolbar = new QHBoxLayout();
    auto* loadBtn = new QPushButton("Load Data Source...");
    connect(loadBtn, &QPushButton::clicked, this, &DataSourceDialog::onLoadClicked);
    toolbar->addWidget(loadBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_fileLabel = new QLabel("No data source loaded.");
    m_fileLabel->setWordWrap(false);
    layout->addWidget(m_fileLabel);

    m_recordTable = new QTableWidget(0, 0);
    m_recordTable->horizontalHeader()->setStretchLastSection(true);
    m_recordTable->verticalHeader()->setVisible(false);
    m_recordTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recordTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recordTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_recordTable, &QTableWidget::itemSelectionChanged,
            this, &DataSourceDialog::onSelectionChanged);
    layout->addWidget(m_recordTable, 1);

    auto* footer = new QHBoxLayout();
    footer->addStretch();
    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    footer->addWidget(closeBtn);
    layout->addLayout(footer);
}

void DataSourceDialog::setSlot(TitleSlot* slot)
{
    m_slot = slot;
    if (m_slot && !m_slot->dataSourcePath.empty()) {
        m_fileLabel->setText(QString::fromStdString(m_slot->dataSourcePath));
        rebuildTable();
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
    if (!m_slot)
        return;

    QString path = QFileDialog::getOpenFileName(
        this, "Load Data Source", QString(),
        "Data Files (*.json *.csv *.lua);;JSON Files (*.json);;CSV Files (*.csv);;Lua Scripts (*.lua)");
    if (path.isEmpty())
        return;

    std::string pathStr = path.toStdString();
    std::string ext = std::filesystem::path(pathStr).extension().string();

    std::unique_ptr<IDataSource> ds;
    try {
        if (ext == ".csv")
            ds = std::make_unique<CsvFileDataSource>(pathStr);
        else if (ext == ".lua")
            ds = std::make_unique<ScriptDataSource>(pathStr);
        else
            ds = std::make_unique<JsonFileDataSource>(pathStr);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Load Data Source",
                              QString("Could not load:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Load Data Source",
                              "Could not load: unknown error.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        m_slot->ownedDataSource = std::move(ds);
        m_slot->title.dataSource = m_slot->ownedDataSource.get();
        m_slot->dataSourcePath = pathStr;
    }

    m_fileLabel->setText(path);
    rebuildTable();
    emit dataSourceChanged();
}

void DataSourceDialog::onSelectionChanged()
{
    int row = selectedRecord();
    if (row < 0 || !m_slot)
        return;
    std::lock_guard<std::mutex> lock(m_slot->mutex);
    m_slot->title.dataRecordIndex = static_cast<size_t>(row);
}

void DataSourceDialog::rebuildTable()
{
    QSignalBlocker blocker(m_recordTable);
    m_recordTable->clearContents();
    m_recordTable->setRowCount(0);
    m_recordTable->setColumnCount(0);

    if (!m_slot)
        return;

    IDataSource* ds = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        ds = m_slot->ownedDataSource.get();
    }
    if (!ds)
        return;

    std::vector<Record> records;
    try {
        records = ds->GetData();
    } catch (...) {
        return;
    }
    if (records.empty())
        return;

    QStringList columns;
    for (auto& [key, _] : records.front())
        columns << QString::fromStdString(key);
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
        m_recordTable->horizontalHeader()->setSectionResizeMode(
            columns.size() - 1, QHeaderView::Stretch);

    for (auto& rec : records) {
        int row = m_recordTable->rowCount();
        m_recordTable->insertRow(row);
        for (int col = 0; col < columns.size(); ++col) {
            auto it = rec.find(columns[col].toStdString());
            QString val = (it != rec.end()) ? QString::fromStdString(it->second) : QString();
            m_recordTable->setItem(row, col, new QTableWidgetItem(val));
        }
    }

    // Restore selected record
    size_t recIdx = 0;
    {
        std::lock_guard<std::mutex> lock(m_slot->mutex);
        recIdx = m_slot->title.dataRecordIndex;
    }
    if ((int)recIdx < m_recordTable->rowCount())
        m_recordTable->selectRow(static_cast<int>(recIdx));
}
