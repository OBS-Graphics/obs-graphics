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

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("StreamCanvas Settings");
    setMinimumWidth(420);
    setWindowFlags(windowFlags() | Qt::Window);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* form = new QFormLayout();
    auto* pathRow = new QHBoxLayout();
    m_editorPathEdit = new QLineEdit();
    m_editorPathEdit->setPlaceholderText("Path to the StreamCanvas editor executable...");
    pathRow->addWidget(m_editorPathEdit, 1);

    auto* browseBtn = new QPushButton(themedIcon(Icons16::File_FolderOpen), "Browse...");
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseClicked);
    pathRow->addWidget(browseBtn);

    form->addRow(new QLabel("Editor path:"), pathRow);
    layout->addLayout(form);

    layout->addStretch(1);

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
