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

#include "settings-dialog.h"
#include "icons.h"
#include "ui-util.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <filesystem>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("StreamCanvas Settings");
    setMinimumSize(480, 320);
    setWindowFlags(windowFlags() | Qt::Window);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* tabs = new QTabWidget();
    layout->addWidget(tabs, 1);

    // ── "General" tab ────────────────────────────────────────────────────
    auto* generalTab = new QWidget();
    auto* generalLayout = new QVBoxLayout(generalTab);
    generalLayout->setContentsMargins(8, 8, 8, 8);
    generalLayout->setSpacing(6);

    auto* form = new QFormLayout();
    auto* pathRow = new QHBoxLayout();
    m_editorPathEdit = new QLineEdit();
    m_editorPathEdit->setPlaceholderText("Path to the StreamCanvas editor executable...");
    pathRow->addWidget(m_editorPathEdit, 1);

    auto* browseBtn = new QPushButton(themedIcon(Icons16::File_FolderOpen), "Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseClicked);
    pathRow->addWidget(browseBtn);

    form->addRow(new QLabel("Editor path:"), pathRow);
    generalLayout->addLayout(form);
    generalLayout->addStretch(1);

    tabs->addTab(generalTab, "General");

    // ── "Data Sources" tab ───────────────────────────────────────────────
    auto* dataSourcesTab = new QWidget();
    auto* dsLayout = new QVBoxLayout(dataSourcesTab);
    dsLayout->setContentsMargins(8, 8, 8, 8);
    dsLayout->setSpacing(6);

    auto* toolbar = new QHBoxLayout();
    auto* addBtn = new QPushButton(themedIcon(Icons16::Action_Plus), "Add Data Source...");
    connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::onAddDataSourceClicked);
    toolbar->addWidget(addBtn);
    toolbar->addStretch();
    dsLayout->addLayout(toolbar);

    m_dataSourceTable = new QTableWidget(0, 3);
    m_dataSourceTable->setHorizontalHeaderLabels({"Name", "Path", ""});
    m_dataSourceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_dataSourceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_dataSourceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_dataSourceTable->horizontalHeader()->resizeSection(2, 70);
    m_dataSourceTable->verticalHeader()->setVisible(false);
    m_dataSourceTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_dataSourceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dsLayout->addWidget(m_dataSourceTable);

    tabs->addTab(dataSourcesTab, "Data Sources");

    auto* footer = new QHBoxLayout();
    footer->addStretch();
    auto* saveBtn = new QPushButton("Save");
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    footer->addWidget(saveBtn);
    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    footer->addWidget(closeBtn);
    layout->addLayout(footer);
}

void SettingsDialog::setEditorPath(const QString& path)
{
    m_editorPathEdit->setText(path);
}

void SettingsDialog::setDataSources(const QList<DataSourceRow>& rows)
{
    m_dataSourceTable->clearContents();
    m_dataSourceTable->setRowCount(0);

    for (const DataSourceRow& src : rows) {
        const QString& path = src.path;
        int row = m_dataSourceTable->rowCount();
        m_dataSourceTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(displayNameForPath(path, "Data Source"));
        auto* pathItem = new QTableWidgetItem(path);
        if (src.broken) {
            // The source stays in the pool and every title's dataSourceId
            // keeps pointing at it — it just can't fetch. Flag it so a
            // deleted/unmounted file or a script that failed to load is
            // visible here rather than only showing up as a title that
            // stopped updating.
            nameItem->setIcon(themedIcon(Icons16::Misc_Warning));
            nameItem->setToolTip(src.brokenReason);
            pathItem->setToolTip(src.brokenReason);
        }
        m_dataSourceTable->setItem(row, 0, nameItem);
        m_dataSourceTable->setItem(row, 1, pathItem);

        auto* actions = new QWidget();
        auto* hbox = new QHBoxLayout(actions);
        hbox->setContentsMargins(2, 2, 2, 2);
        hbox->setSpacing(2);

        auto* reloadBtn = makeIconButton(Icons16::Action_Refresh, "Reload");
        hbox->addWidget(reloadBtn);

        auto* removeBtn = makeIconButton(Icons16::Action_Trash, "Remove");
        hbox->addWidget(removeBtn);

        m_dataSourceTable->setCellWidget(row, 2, actions);

        // Capture the id by value, not the row index — setDataSources()
        // rebuilds the whole table on every pool mutation, so a captured row
        // index would go stale as soon as rows are added/removed.
        const QString id = src.id;
        connect(reloadBtn, &QToolButton::clicked, this, [this, id]() {
            emit dataSourceReloadRequested(id);
        });
        connect(removeBtn, &QToolButton::clicked, this, [this, id]() {
            emit dataSourceRemoveRequested(id);
        });
    }
}

void SettingsDialog::onBrowseClicked()
{
    QString path = QFileDialog::getOpenFileName(this, "Select Editor Executable", m_editorPathEdit->text());
    if (!path.isEmpty())
        m_editorPathEdit->setText(path);
}

void SettingsDialog::onSaveClicked()
{
    emit editorPathChanged(m_editorPathEdit->text());
}

void SettingsDialog::onAddDataSourceClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Add Data Source", QString(),
        "Data Files (*.json *.csv *.lua);;JSON Files (*.json);;CSV Files (*.csv);;Lua Scripts (*.lua)");
    if (path.isEmpty())
        return;

    emit dataSourceAddRequested(path);
}
