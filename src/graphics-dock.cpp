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

#include "graphics-dock.h"
#include "icons.h"

#include <obs-module.h>

#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <filesystem>

GraphicsDockWidget::GraphicsDockWidget(QWidget* parent, std::string configPath)
    : QWidget(parent), m_configPath(std::move(configPath))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Toolbar row
    auto* toolbar = new QWidget();
    auto* toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);

    auto* addBtn = new QPushButton(themedIcon(Icons16::Action_Plus), " Add Title...");
    connect(addBtn, &QPushButton::clicked, this, &GraphicsDockWidget::onAddTitleClicked);
    toolbarLayout->addWidget(addBtn);
    toolbarLayout->addStretch();

    layout->addWidget(toolbar);

    // Title table
    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({"Title", "File", ""});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(2, 130);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    loadConfig();
}

void GraphicsDockWidget::addTitleRow(std::shared_ptr<TitleSlot> slot)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);

    QString name = QString::fromStdString(
        std::filesystem::path(slot->path).stem().string());
    if (name.isEmpty())
        name = "Title";

    m_table->setItem(row, 0, new QTableWidgetItem(name));
    m_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(slot->path)));

    // Actions cell
    auto* actions = new QWidget();
    auto* hbox = new QHBoxLayout(actions);
    hbox->setContentsMargins(2, 1, 2, 1);
    hbox->setSpacing(2);

    auto* inBtn = new QPushButton("In");
    inBtn->setFixedHeight(22);
    inBtn->setStyleSheet("QPushButton { background-color: #28752a; color: white; font-weight: bold; }");
    connect(inBtn, &QPushButton::clicked, this, [this, row]() { onTriggerIn(row); });
    hbox->addWidget(inBtn);

    auto* outBtn = new QPushButton("Out");
    outBtn->setFixedHeight(22);
    outBtn->setStyleSheet("QPushButton { background-color: #b42218; color: white; font-weight: bold; }");
    connect(outBtn, &QPushButton::clicked, this, [this, row]() { onTriggerOut(row); });
    hbox->addWidget(outBtn);

    auto* removeBtn = new QToolButton();
    removeBtn->setIcon(themedIcon(Icons16::Action_CloseSmall));
    removeBtn->setAutoRaise(true);
    removeBtn->setFixedSize(22, 22);
    removeBtn->setToolTip("Remove");
    connect(removeBtn, &QToolButton::clicked, this, [this, row]() { onRemoveTitle(row); });
    hbox->addWidget(removeBtn);

    m_table->setCellWidget(row, 2, actions);
    m_slots.push_back(std::move(slot));
}

void GraphicsDockWidget::rebuildGlobalList()
{
    auto newList = std::make_shared<TitleSlotList>(m_slots);
    g_title_slots.store(newList);
}

void GraphicsDockWidget::onAddTitleClicked()
{
    QString path =
        QFileDialog::getOpenFileName(this, "Load Title", QString(), "OBS Graphics Title (*.ogt)");
    if (path.isEmpty())
        return;

    auto slot = std::make_shared<TitleSlot>();
    try {
        slot->title = Title::Load(path.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Load Title",
                              QString("Could not load title:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Load Title",
                              "Could not load title: unknown error.");
        return;
    }

    slot->path = path.toStdString();
    slot->loaded = true;

    addTitleRow(slot);
    rebuildGlobalList();
    saveConfig();
}

void GraphicsDockWidget::onTriggerIn(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;
    auto& slot = m_slots[row];
    std::lock_guard<std::mutex> lock(slot->mutex);
    slot->title.TriggerIn();
}

void GraphicsDockWidget::onTriggerOut(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;
    auto& slot = m_slots[row];
    std::lock_guard<std::mutex> lock(slot->mutex);
    slot->title.TriggerOut();
}

void GraphicsDockWidget::onRemoveTitle(int row)
{
    if (row < 0 || row >= (int)m_slots.size())
        return;

    m_table->removeRow(row);
    m_slots.erase(m_slots.begin() + row);

    // Re-wire row indices in the buttons below the removed row
    for (int r = row; r < m_table->rowCount(); ++r) {
        if (auto* cell = m_table->cellWidget(r, 2)) {
            auto buttons = cell->findChildren<QPushButton*>();
            auto toolButtons = cell->findChildren<QToolButton*>();
            // Disconnect and reconnect with updated row index
            for (auto* btn : buttons)
                btn->disconnect();
            for (auto* btn : toolButtons)
                btn->disconnect();

            if (buttons.size() >= 2 && toolButtons.size() >= 1) {
                connect(buttons[0], &QPushButton::clicked, this, [this, r]() { onTriggerIn(r); });
                connect(buttons[1], &QPushButton::clicked, this, [this, r]() { onTriggerOut(r); });
                connect(toolButtons[0], &QToolButton::clicked, this, [this, r]() { onRemoveTitle(r); });
            }
        }
    }

    rebuildGlobalList();
    saveConfig();
}

void GraphicsDockWidget::saveConfig()
{
    if (m_loading)
        return;

    AppConfig cfg;
    for (auto& slot : m_slots)
        cfg.titlePaths.push_back(slot->path);
    cfg.Save(m_configPath);
}

void GraphicsDockWidget::loadConfig()
{
    AppConfig cfg = AppConfig::Load(m_configPath);
    if (cfg.titlePaths.empty())
        return;

    m_loading = true;

    for (auto& path : cfg.titlePaths) {
        if (path.empty() || !std::filesystem::exists(path))
            continue;

        auto slot = std::make_shared<TitleSlot>();
        try {
            slot->title = Title::Load(path);
        } catch (...) {
            continue;
        }

        slot->path = path;
        slot->loaded = true;
        addTitleRow(slot);
    }

    rebuildGlobalList();
    m_loading = false;
}
