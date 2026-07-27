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

#include "title-settings-dialog.h"
#include "engine/data-pool.h"
#include "engine/script.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "icons.h"
#include "ui-util.h"

#include <filesystem>

TitleSettingsDialog::TitleSettingsDialog(const QString& titleName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Settings — %1").arg(titleName));
    setMinimumSize(480, 320);
    setWindowFlags(windowFlags() | Qt::Window);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto* tabs = new QTabWidget();
    layout->addWidget(tabs, 1);

    // ── "Data Source" tab ────────────────────────────────────────────────
    auto* dataSourceTab = new QWidget();
    auto* dsLayout = new QVBoxLayout(dataSourceTab);
    dsLayout->setContentsMargins(8, 8, 8, 8);
    dsLayout->setSpacing(6);

    auto* pickerRow = new QHBoxLayout();
    pickerRow->addWidget(new QLabel("Data Source:"));
    m_dataSourceCombo = new QComboBox();
    connect(m_dataSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TitleSettingsDialog::onDataSourceChanged);
    pickerRow->addWidget(m_dataSourceCombo, 1);
    dsLayout->addLayout(pickerRow);

    m_scriptErrorLabel = new QLabel();
    m_scriptErrorLabel->setWordWrap(true);
    m_scriptErrorLabel->setStyleSheet("color: #b42218;");
    m_scriptErrorLabel->setVisible(false);
    dsLayout->addWidget(m_scriptErrorLabel);

    m_scriptErrorTimer = new QTimer(this);
    m_scriptErrorTimer->setInterval(250); // matches ScriptDataSource's default _poll_interval
    connect(m_scriptErrorTimer, &QTimer::timeout, this, &TitleSettingsDialog::updateScriptErrorLabel);

    m_recordTable = new QTableWidget(0, 0);
    m_recordTable->horizontalHeader()->setStretchLastSection(true);
    m_recordTable->verticalHeader()->setVisible(false);
    m_recordTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recordTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recordTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_recordTable, &QTableWidget::itemSelectionChanged,
            this, &TitleSettingsDialog::onSelectionChanged);
    dsLayout->addWidget(m_recordTable, 1);

    tabs->addTab(dataSourceTab, "Data Source");

    // ── "Settings" tab ───────────────────────────────────────────────────
    auto* settingsTab = new QWidget();
    auto* settingsLayout = new QVBoxLayout(settingsTab);
    settingsLayout->setContentsMargins(8, 8, 8, 8);
    settingsLayout->setSpacing(6);

    m_durationGroup = new QGroupBox("Auto-hide after");
    m_durationGroup->setCheckable(true);
    m_durationGroup->setChecked(false);
    auto* durationLayout = new QHBoxLayout(m_durationGroup);
    m_durationSpin = new QDoubleSpinBox();
    m_durationSpin->setRange(0.1, 3600.0);
    m_durationSpin->setSingleStep(0.5);
    m_durationSpin->setDecimals(1);
    m_durationSpin->setSuffix(" s");
    m_durationSpin->setValue(5.0);
    durationLayout->addWidget(m_durationSpin);
    durationLayout->addStretch();
    connect(m_durationGroup, &QGroupBox::toggled, this, &TitleSettingsDialog::onDurationChanged);
    connect(m_durationSpin, &QDoubleSpinBox::valueChanged, this, &TitleSettingsDialog::onDurationChanged);
    settingsLayout->addWidget(m_durationGroup);
    settingsLayout->addStretch(1);

    tabs->addTab(settingsTab, "Settings");

    auto* footer = new QHBoxLayout();
    footer->addStretch();
    auto* closeBtn = new QPushButton("Close");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);
    footer->addWidget(closeBtn);
    layout->addLayout(footer);
}

void TitleSettingsDialog::setRow(TitleRow* row)
{
    m_row = row;
    if (!m_row)
        return;

    refreshDataSourceList(); // also rebuilds the record table
    updateScriptErrorLabel();

    // m_row->duration is a plain host-side field (not on Title), so no scene
    // lock is needed to read it.
    double duration = m_row->duration;
    QSignalBlocker groupBlocker(m_durationGroup);
    QSignalBlocker spinBlocker(m_durationSpin);
    m_durationGroup->setChecked(duration >= 0.0);
    m_durationSpin->setValue(duration >= 0.0 ? duration : 5.0);
}

void TitleSettingsDialog::refreshDataSourceList()
{
    if (!m_row)
        return;

    QSignalBlocker blocker(m_dataSourceCombo);
    m_dataSourceCombo->clear();
    m_dataSourceCombo->addItem("(none)", QString());

    for (auto& id : g_scene.Pool().Ids()) {
        IDataSource* src = g_scene.Pool().Get(id);
        if (!src)
            continue;
        QString qid = QString::fromStdString(id);
        QString path = QString::fromStdString(src->GetFilePath());
        m_dataSourceCombo->addItem(displayNameForPath(path, path), qid);
        m_dataSourceCombo->setItemData(m_dataSourceCombo->count() - 1, path, Qt::ToolTipRole);
    }

    int idx = m_dataSourceCombo->findData(QString::fromStdString(m_row->dataSourceId));
    m_dataSourceCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    rebuildTable();
}

void TitleSettingsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    updateScriptErrorLabel();
    m_scriptErrorTimer->start();
}

void TitleSettingsDialog::hideEvent(QHideEvent* event)
{
    m_scriptErrorTimer->stop();
    QDialog::hideEvent(event);
}

void TitleSettingsDialog::updateScriptErrorLabel()
{
    if (!m_row) {
        m_scriptErrorLabel->setVisible(false);
        return;
    }

    // g_scene.Pool().Get() is the documented host-UI-only escape hatch for
    // exactly this kind of dynamic_cast; safe to call from the UI thread
    // without the scene lock — it doesn't touch any Title.
    auto* script = dynamic_cast<ScriptDataSource*>(g_scene.Pool().Get(m_row->dataSourceId));
    if (script && script->LoadFailed()) {
        m_scriptErrorLabel->setText(QString::fromStdString(script->GetLoadError()));
        m_scriptErrorLabel->setVisible(true);
    } else {
        m_scriptErrorLabel->setVisible(false);
    }
}

int TitleSettingsDialog::selectedRecord() const
{
    auto selected = m_recordTable->selectedItems();
    if (selected.isEmpty())
        return -1;
    return m_recordTable->row(selected.first());
}

void TitleSettingsDialog::onDurationChanged()
{
    if (!m_row)
        return;

    double duration = m_durationGroup->isChecked() ? m_durationSpin->value() : -1.0;
    m_row->duration = duration;
    // Also mirror onto the live Title so a currently-visible title honours a
    // changed auto-hide immediately, rather than only on its next TriggerIn.
    if (m_row->title) {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        m_row->title->duration = duration;
    }
    emit configChanged();
}

void TitleSettingsDialog::onDataSourceChanged(int index)
{
    if (!m_row || index < 0)
        return;

    std::string id = m_dataSourceCombo->itemData(index).toString().toStdString();

    // No Bind/Unbind exists any more — a Title binds to a source by plain
    // assignment of dataSourceId (empty = unbound). Update the host-side row
    // first (no lock needed, it isn't part of g_scene), then take the scene
    // lock just to mutate the live Title.
    m_row->dataSourceId = id;
    if (m_row->title) {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        m_row->title->dataSourceId = id;
    }

    rebuildTable();
    updateScriptErrorLabel();
    emit configChanged();
}

void TitleSettingsDialog::onSelectionChanged()
{
    int row = selectedRecord();
    if (row < 0 || !m_row || !m_row->title)
        return;

    // Resolve records with no lock held — Pool().Data() only touches the
    // pool's own mutex and never blocks. Never call DataBlocking() while
    // holding g_scene_mutex.
    std::vector<Record> recs;
    if (!m_row->dataSourceId.empty())
        recs = g_scene.Pool().Data(m_row->dataSourceId);

    std::lock_guard<std::mutex> lock(g_scene_mutex);
    m_row->title->dataRecordIndex = static_cast<size_t>(row);

    // Apply the newly-selected record whatever the title's state — picking a
    // record in this dialog is a direct request to see that record, so a
    // title already on screen must swap to it now, not on its next TriggerIn.
    //
    // The no-arg Title::UpdateData() that Scene::Tick runs every frame while
    // Visible won't do it for us: that one pulls via DataPool::DataIfChanged,
    // and changing which record we index into doesn't move the source's cache
    // version, so it reads as unchanged and applies nothing. Hence this
    // explicit two-arg apply. For the same reason it also won't be clobbered
    // on the next tick.
    //
    // instant only when nothing is on screen: a hidden title has no visible
    // "before" to animate away from, while a visible one should run the
    // element data-in/data-out animations, which is what the animated
    // SetContent path drives.
    const bool hidden = m_row->title->state == TitleState::Hidden;
    m_row->title->UpdateData(recs, /*instant=*/hidden);
}

void TitleSettingsDialog::rebuildTable()
{
    QSignalBlocker blocker(m_recordTable);
    m_recordTable->clearContents();
    m_recordTable->setRowCount(0);
    m_recordTable->setColumnCount(0);

    if (!m_row || m_row->dataSourceId.empty())
        return;

    std::vector<Record> records = g_scene.Pool().Data(m_row->dataSourceId);
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
    if (m_row->title) {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        recIdx = m_row->title->dataRecordIndex;
    }
    if ((int)recIdx < m_recordTable->rowCount())
        m_recordTable->selectRow(static_cast<int>(recIdx));
}
