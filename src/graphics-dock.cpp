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

#include "graphics-dock.h"
#include "engine/script.h"
#include "engine/uuid.h"
#include "icons.h"
#include "plugin-support.h"
#include "ui-util.h"

// Qt first, deliberately. On Linux, <obs-module.h> pulls in
// util/sse-intrin.h, which includes simde/x86/sse2.h with
// SIMDE_ENABLE_NATIVE_ALIASES — that #defines _mm_loadu_epi64 and friends onto
// simde's own implementations. <QtConcurrent> then reaches GCC's
// avx512vlintrin.h, whose real _mm_loadu_epi64 definitions expand through
// those macros and collide. Including Qt first means the native intrinsics are
// already declared when simde runs, so simde skips the aliases entirely.
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QtConcurrent>
#include <QToolButton>
#include <QVBoxLayout>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <algorithm>
#include <filesystem>

static const char* kStyleIn    = "QPushButton { background-color: #28752a; color: white; font-weight: bold; }";
static const char* kStyleOut   = "QPushButton { background-color: #b42218; color: white; font-weight: bold; }";
static const char* kStyleTimed = "QPushButton { background-color: #1a5fb4; color: white; font-weight: bold; }";
// Disabling a button doesn't grey it while a stylesheet is setting an explicit
// background — the sheet wins over the disabled palette — so the fetching look
// needs its own colours rather than just setEnabled(false).
static const char* kStyleLoading = "QPushButton { background-color: #4a4a4a; color: #bdbdbd; font-weight: bold; }";

namespace {

// Makes a profile/scene collection name safe to use as a single path component.
std::string sanitizeForPath(const std::string& name)
{
    std::string out = name;
    for (char& c : out) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' ||
            c == '<' || c == '>' || c == '|' || static_cast<unsigned char>(c) < 0x20)
            c = '_';
    }
    return out.empty() ? std::string("default") : out;
}

// Single by-extension construction point for every IDataSource the dock (or
// the app-level Data Sources tab, via onAppSettingsClicked's wired signals)
// creates. May throw; see tryMakeDataSource for the reporting wrapper.
std::unique_ptr<IDataSource> makeDataSource(const std::string& path)
{
    std::string ext = std::filesystem::path(path).extension().string();
    if (ext == ".csv")
        return std::make_unique<CsvFileDataSource>(path);
    if (ext == ".lua")
        return std::make_unique<ScriptDataSource>(path);
    return std::make_unique<JsonFileDataSource>(path);
}

// makeDataSource plus the modal every user-initiated construction wants:
// returns nullptr (having already reported) instead of throwing, so callers
// read as a plain guard. `verb` is lowercase and used in both the title and
// the body ("load" / "reload").
std::unique_ptr<IDataSource> tryMakeDataSource(QWidget* parent, const std::string& path, const QString& verb)
{
    QString title = QString("Failed to %1 Data Source").arg(verb[0].toUpper() + verb.mid(1));
    try {
        return makeDataSource(path);
    } catch (const std::exception& e) {
        QMessageBox::critical(parent, title, QString("Could not %1:\n%2").arg(verb, e.what()));
    } catch (...) {
        QMessageBox::critical(parent, title, QString("Could not %1: unknown error.").arg(verb));
    }
    return nullptr;
}

// What "broken" means for a bound data source, shared by the title-row
// warning icon (GraphicsDockWidget::refreshRowBrokenState) and the Data
// Sources tab (refreshDataSourceUi): the file backing it is gone, or — for a
// ScriptDataSource specifically — the file loaded but its Lua failed to
// parse/run (ScriptDataSource::LoadFailed()). `src` may be nullptr (a
// dataSourceId that no longer resolves in the pool); that counts as broken
// too.
struct DataSourceHealth {
    bool broken{false};
    QString reason; // empty when !broken
};

DataSourceHealth checkDataSourceHealth(const IDataSource* src)
{
    if (!src)
        return {true, "Data source not found."};

    std::error_code ec;
    std::string path = src->GetFilePath();
    if (!std::filesystem::exists(path, ec) || ec)
        return {true, QString("File not found:\n%1").arg(QString::fromStdString(path))};

    if (auto* script = dynamic_cast<const ScriptDataSource*>(src); script && script->LoadFailed())
        return {true, QString("Script failed to load:\n%1")
                           .arg(QString::fromStdString(script->GetLoadError()))};

    return {false, {}};
}

// Title::Load's diagnostic is never fatal (see engine/title.h), but a
// Warning is worth a modal when it's the direct result of a user action
// (Add/Reload); at startup (loadConfig) neither severity may block OBS
// launch with a dialog, so both just get logged.
void reportLoadDiagnostic(QWidget* parent, const Title& title, bool allowModal)
{
    const auto& diag = title.GetLoadDiagnostic();
    switch (diag.severity) {
    case Title::LoadDiagnostic::Severity::Warning:
        if (allowModal)
            QMessageBox::warning(parent, "Title Schema Warning", QString::fromStdString(diag.message));
        else
            obs_log(LOG_WARNING, "%s", diag.message.c_str());
        break;
    case Title::LoadDiagnostic::Severity::Info:
        obs_log(LOG_INFO, "%s", diag.message.c_str());
        break;
    default:
        break;
    }
}

} // namespace

GraphicsDockWidget::GraphicsDockWidget(QWidget* parent, std::string configDir)
    : QWidget(parent), m_configDir(std::move(configDir))
{
    m_settingsPath = (std::filesystem::path(m_configDir) / "settings.json").string();
    m_settings = AppSettings::Load(m_settingsPath);

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

    auto* appSettingsBtn = new QToolButton();
    appSettingsBtn->setIcon(themedIcon(Icons16::Navigation_Settings));
    appSettingsBtn->setIconSize(QSize(16, 16));
    appSettingsBtn->setAutoRaise(true);
    appSettingsBtn->setToolTip("Settings...");
    connect(appSettingsBtn, &QToolButton::clicked, this, &GraphicsDockWidget::onAppSettingsClicked);
    toolbarLayout->addWidget(appSettingsBtn);

    m_openEditorBtn = new QPushButton(themedIcon(Icons16::Action_ExternalLink), " Open Editor");
    connect(m_openEditorBtn, &QPushButton::clicked, this, &GraphicsDockWidget::onOpenEditorClicked);
    toolbarLayout->addWidget(m_openEditorBtn);
    updateOpenEditorEnabled();

    layout->addWidget(toolbar);

    // Catches a ScriptDataSource's load failing asynchronously some time
    // after its row was created — the explicit refreshRowBrokenState() call
    // sites (add/reload/rebind/pool mutation) can't see that on their own.
    // Interval matches ScriptDataSource's default poll interval and
    // TitleSettingsDialog::m_scriptErrorTimer's own cadence.
    m_brokenStateTimer = new QTimer(this);
    m_brokenStateTimer->setInterval(250);
    connect(m_brokenStateTimer, &QTimer::timeout, this, &GraphicsDockWidget::refreshAllRowBrokenStates);
    m_brokenStateTimer->start();

    // Title table: two columns — Title name | Actions
    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({"Title", ""});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(1, 160);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    // Titles are loaded once the active profile/scene collection is known —
    // see reloadForCurrentContext(), driven by OBS frontend events.
}

QString GraphicsDockWidget::rowDisplayName(TitleRow* row) const
{
    std::string name;
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        if (row->title)
            name = row->title->name;
    }
    if (!name.empty())
        return QString::fromStdString(name);
    return displayNameForPath(QString::fromStdString(row->path), "Title");
}

void GraphicsDockWidget::addTitleRow(std::unique_ptr<TitleRow> row)
{
    int rowIdx = m_table->rowCount();
    m_table->insertRow(rowIdx);

    m_table->setItem(rowIdx, 0, new QTableWidgetItem(rowDisplayName(row.get())));

    // Actions cell: [Toggle In/Out] [Settings] [Reload] [Remove]
    auto* actions = new QWidget();
    auto* hbox = new QHBoxLayout(actions);
    hbox->setContentsMargins(2, 2, 2, 2);
    hbox->setSpacing(2);

    auto* toggleBtn = new QPushButton(themedIcon(Icons16::Media_Play), " In");
    toggleBtn->setStyleSheet(kStyleIn);
    hbox->addWidget(toggleBtn, 1);

    auto* settingsBtn = makeIconButton(Icons16::Navigation_Settings, "Settings...");
    hbox->addWidget(settingsBtn);

    auto* reloadBtn = makeIconButton(Icons16::Action_Refresh, "Reload from file");
    hbox->addWidget(reloadBtn);

    auto* removeBtn = makeIconButton(Icons16::Action_Trash, "Remove");
    hbox->addWidget(removeBtn);

    m_table->setCellWidget(rowIdx, 1, actions);

    RowWidgets rw;
    rw.toggleBtn = toggleBtn;
    rw.reloadBtn = reloadBtn;
    rw.isIn = false;
    m_rowWidgets.push_back(rw);

    // Use the row's raw pointer for stable identity across vector mutations.
    TitleRow* rowPtr = row.get();
    m_rows.push_back(std::move(row));

    connect(toggleBtn, &QPushButton::clicked, this, [this, rowPtr]() {
        int r = rowIndexFor(rowPtr);
        if (r >= 0)
            onToggle(r);
    });

    connect(settingsBtn, &QToolButton::clicked, this, [this, rowPtr]() {
        int r = rowIndexFor(rowPtr);
        if (r >= 0)
            onDataSource(r);
    });

    connect(reloadBtn, &QToolButton::clicked, this, [this, rowPtr]() {
        int r = rowIndexFor(rowPtr);
        if (r >= 0)
            onReloadTitle(r);
    });

    connect(removeBtn, &QToolButton::clicked, this, [this, rowPtr]() {
        int r = rowIndexFor(rowPtr);
        if (r >= 0)
            onRemoveTitle(r);
    });

    // Note: the trigger callbacks that keep this row's button in sync with
    // the Title's state are registered by registerTriggerCallbacks(), called
    // by every caller of AddTitle (onAddTitleClicked, loadConfig,
    // onReloadTitle) in the same g_scene_mutex critical section as the
    // AddTitle call itself — not here, since this function runs without the
    // lock held (it needs to call rowDisplayName(), which takes the lock on
    // its own).

    refreshRowBrokenState(rowIdx);
}

void GraphicsDockWidget::registerTriggerCallbacks(TitleRow* rowPtr)
{
    // Precondition: called with g_scene_mutex already held by the caller —
    // this pushes onto the fresh Title's onTriggerIn/onTriggerOut vectors,
    // which must not race a concurrent Scene::Tick.
    //
    // Both callbacks fire under g_scene_mutex too — from the render thread,
    // inside Scene::Tick, regardless of whether the trigger was a duration
    // timeout, a Lua script's trigger_out(), or our own onToggle() (which
    // also holds the lock around its TriggerIn/TriggerOut call) — so these
    // lambdas must stay non-blocking: they only post a queued UI update.
    // Dereferencing `this` as invokeMethod's context object to find its
    // thread is only safe because OBS always tears down the Graphics Source
    // instance (and with it, the global tick callback that's the only caller
    // of Scene::Tick) before it destroys this dock, never the reverse.
    rowPtr->title->onTriggerIn.push_back([this, rowPtr](size_t, double) {
        QMetaObject::invokeMethod(this, [this, rowPtr]() {
            int r = rowIndexFor(rowPtr);
            if (r >= 0)
                applyRowState(r, true);
        }, Qt::QueuedConnection);
    });
    rowPtr->title->onTriggerOut.push_back([this, rowPtr]() {
        QMetaObject::invokeMethod(this, [this, rowPtr]() {
            int r = rowIndexFor(rowPtr);
            if (r >= 0)
                applyRowState(r, false);
        }, Qt::QueuedConnection);
    });
}

int GraphicsDockWidget::rowIndexFor(TitleRow* rowPtr) const
{
    auto it = std::find_if(m_rows.begin(), m_rows.end(),
        [rowPtr](const auto& r) { return r.get() == rowPtr; });
    if (it == m_rows.end())
        return -1;
    return static_cast<int>(it - m_rows.begin());
}

void GraphicsDockWidget::applyRowState(int row, bool isIn)
{
    if (row < 0 || row >= (int)m_rowWidgets.size())
        return;

    auto& rw = m_rowWidgets[row];
    rw.isIn = isIn;
    rw.reloadBtn->setEnabled(!isIn);

    if (!isIn) {
        rw.toggleBtn->setText(" In");
        rw.toggleBtn->setIcon(themedIcon(Icons16::Media_Play));
        refreshRowBrokenState(row); // sets kStyleIn, or the broken look if applicable
        return;
    }

    double duration;
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        duration = m_rows[row]->title->duration;
    }
    bool timed = duration >= 0.0;
    rw.toggleBtn->setText(" Out");
    rw.toggleBtn->setIcon(themedIcon(timed ? Icons16::Misc_Clock : Icons16::Media_Stop));
    rw.toggleBtn->setStyleSheet(timed ? kStyleTimed : kStyleOut);
}

void GraphicsDockWidget::applyRowLoading(int row)
{
    if (row < 0 || row >= (int)m_rowWidgets.size())
        return;

    auto& rw = m_rowWidgets[row];
    rw.fetching = true;
    rw.toggleBtn->setEnabled(false);
    rw.toggleBtn->setText(" Loading...");
    rw.toggleBtn->setIcon(themedIcon(Icons16::Misc_Hourglass));
    rw.toggleBtn->setStyleSheet(kStyleLoading);

    // rw.isIn is deliberately left alone: this is a transient look over the
    // fetch, not a fourth row state. The title is still Hidden, and if the
    // fetch never lands the row is exactly where it was.
}

void GraphicsDockWidget::refreshRowBrokenState(int row)
{
    if (row < 0 || row >= (int)m_rowWidgets.size())
        return;

    TitleRow* rowPtr = m_rows[row].get();
    auto& rw = m_rowWidgets[row];

    if (rowPtr->dataSourceId.empty()) {
        rw.broken = false;
        rw.brokenReason.clear();
    } else {
        // Pool().Get() is individually thread-safe; no g_scene_mutex needed.
        auto health = checkDataSourceHealth(g_scene.Pool().Get(rowPtr->dataSourceId));
        rw.broken = health.broken;
        rw.brokenReason = health.reason;
    }

    if (auto* nameItem = m_table->item(row, 0)) {
        nameItem->setIcon(rw.broken ? themedIcon(Icons16::Misc_Warning) : QIcon());
        nameItem->setToolTip(rw.broken ? rw.brokenReason : QString());
    }

    // Only the idle "In" look is gated: "Out" must always be clickable
    // regardless of source health (hiding never fetches), and a fetch
    // already in flight (applyRowLoading) owns the button's look for its own
    // span — stomping it here would fight that transient state.
    if (!rw.isIn && !rw.fetching) {
        rw.toggleBtn->setEnabled(!rw.broken);
        rw.toggleBtn->setStyleSheet(rw.broken ? kStyleLoading : kStyleIn);
    }
}

void GraphicsDockWidget::refreshAllRowBrokenStates()
{
    for (int r = 0; r < (int)m_rows.size(); ++r)
        refreshRowBrokenState(r);
}

void GraphicsDockWidget::reassignZOrders()
{
    std::lock_guard<std::mutex> lock(g_scene_mutex);
    for (size_t i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i]->title)
            m_rows[i]->title->zOrder = static_cast<int>(i);
    }
}

void GraphicsDockWidget::onAddTitleClicked()
{
    QString path =
        QFileDialog::getOpenFileName(this, "Load Title", QString(), "StreamCanvas Title (*.ogt)");
    if (path.isEmpty())
        return;

    std::string pathStr = path.toStdString();
    std::unique_ptr<Title> t;
    try {
        t = std::make_unique<Title>(Title::Load(pathStr));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Load Title",
                              QString("Could not load title:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Load Title",
                              "Could not load title: unknown error.");
        return;
    }

    reportLoadDiagnostic(this, *t, /*allowModal=*/true);

    auto row = std::make_unique<TitleRow>();
    row->path = pathStr;
    row->id = uuid::GenerateV4();
    t->id = row->id;

    TitleRow* rowPtr = row.get();
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        rowPtr->title = g_scene.AddTitle(std::move(t));
        // AddTitle regenerates a colliding id (e.g. the same .ogt opened
        // twice); persist whatever it actually assigned rather than the id
        // minted above.
        rowPtr->id = rowPtr->title->id;
        registerTriggerCallbacks(rowPtr);
    }

    addTitleRow(std::move(row));
    reassignZOrders();
    saveConfig();
}

void GraphicsDockWidget::onToggle(int row)
{
    if (row < 0 || row >= (int)m_rows.size())
        return;

    TitleRow* rowPtr = m_rows[row].get();
    auto& rw = m_rowWidgets[row];

    // Don't paint the In/Out state here — Title::TriggerIn/TriggerOut below
    // synchronously fires the onTriggerIn/onTriggerOut callback registered
    // in registerTriggerCallbacks(), which applies the row state via a
    // queued UI update. Doing it again here would apply it twice per click.
    // (The transient "Loading..." look further down is not that state; it
    // covers the window before any trigger has happened at all.)
    bool nowIn = !rw.isIn;
    if (!nowIn) {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        rowPtr->title->TriggerOut();
        return;
    }

    // Defense-in-depth: toggleBtn should already be disabled for a broken
    // row (refreshRowBrokenState), but a click can slip in between the
    // 250ms timer ticks. Harmless to skip either way — see
    // checkDataSourceHealth for what "broken" means.
    if (rw.broken)
        return;

    size_t recIdx = 0;
    if (rw.settingsDialog) {
        int sel = rw.settingsDialog->selectedRecord();
        if (sel >= 0)
            recIdx = static_cast<size_t>(sel);
    }

    if (rowPtr->dataSourceId.empty()) {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        rowPtr->title->TriggerIn(recIdx, rowPtr->duration);
        return;
    }

    // A DataPool::DataBlocking() fetch can take seconds for a network-backed
    // ScriptDataSource, and doing it while holding g_scene_mutex would freeze
    // OBS rendering outright — Scene::Tick/Render need that same lock every
    // frame. So the fetch happens here, off-thread with NO scene lock held,
    // and the records are carried to the three-argument
    // Title::TriggerIn(recordIndex, duration, records), which applies what
    // it's given and fetches nothing. The two-argument overload would re-fetch
    // under the lock and make this whole detour pointless.
    //
    // Show the fetching state for the duration: greyed out and unclickable, so
    // the operator can see the title is on its way and a double-click can't
    // queue two triggers. The queued continuation clears the fetching flag
    // and re-applies the row's normal look via refreshRowBrokenState() (or
    // leaves it be if the row was removed while the fetch was in flight), and
    // the " Out" look is painted by the applyRowState that TriggerIn's own
    // onTriggerIn callback posts immediately after — so the button reads
    // In -> Loading... -> Out with no flicker back through " In".
    const std::string dsId = rowPtr->dataSourceId;
    applyRowLoading(row);
    // The QFuture is deliberately dropped (hence the cast past [[nodiscard]]):
    // the worker hands its result back by posting a queued call to this
    // widget, so there is nothing here to wait on or collect.
    (void)QtConcurrent::run([this, rowPtr, dsId, recIdx]() {
        std::vector<Record> records = g_scene.Pool().DataBlocking(dsId); // no scene lock held
        QMetaObject::invokeMethod(this, [this, rowPtr, dsId, recIdx,
                                         records = std::move(records)]() mutable {
            int r = rowIndexFor(rowPtr);
            if (r < 0)
                return; // row removed while the fetch was in flight
            m_rowWidgets[r].fetching = false;
            refreshRowBrokenState(r); // re-applies enabled/style now that fetching is done

            // The row may have been pointed at a different source while the
            // fetch was in flight — showing the old source's records would be
            // wrong. Take the new source's cache instead: it may be up to one
            // poll interval stale, but reading it can't block, and the title
            // catches up on its next Tick once Visible.
            if (rowPtr->dataSourceId != dsId)
                records = g_scene.Pool().Data(rowPtr->dataSourceId);

            std::lock_guard<std::mutex> lock(g_scene_mutex);
            rowPtr->title->TriggerIn(recIdx, rowPtr->duration, records);
        }, Qt::QueuedConnection);
    });
}

void GraphicsDockWidget::onDataSource(int row)
{
    if (row < 0 || row >= (int)m_rows.size())
        return;

    auto& rw = m_rowWidgets[row];
    if (!rw.settingsDialog) {
        QString name = m_table->item(row, 0) ? m_table->item(row, 0)->text() : "Title";
        rw.settingsDialog = new TitleSettingsDialog(name, this);
        rw.settingsDialog->setRow(m_rows[row].get());
        TitleRow* rowPtr = m_rows[row].get();
        connect(rw.settingsDialog, &TitleSettingsDialog::configChanged,
                this, [this, rowPtr]() {
                    saveConfig();
                    int r = rowIndexFor(rowPtr);
                    if (r >= 0)
                        refreshRowBrokenState(r);
                });
    }

    rw.settingsDialog->show();
    rw.settingsDialog->raise();
    rw.settingsDialog->activateWindow();
}

void GraphicsDockWidget::onReloadTitle(int row)
{
    if (row < 0 || row >= (int)m_rows.size())
        return;
    if (m_rowWidgets[row].isIn)
        return; // blocked while visible; button should already be disabled

    TitleRow* rowPtr = m_rows[row].get();

    std::unique_ptr<Title> freshTitle;
    try {
        freshTitle = std::make_unique<Title>(Title::Load(rowPtr->path));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Failed to Reload Title",
                              QString("Could not reload title:\n%1").arg(e.what()));
        return;
    } catch (...) {
        QMessageBox::critical(this, "Failed to Reload Title",
                              "Could not reload title: unknown error.");
        return;
    }

    reportLoadDiagnostic(this, *freshTitle, /*allowModal=*/true);

    // Reapply the persisted uuid before AddTitle, so a running Lua script's
    // trigger_out(id) keeps targeting the same title across this reload.
    freshTitle->id = rowPtr->id;

    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        // RemoveTitle destroys the old Title outright — rowPtr->title must
        // not be dereferenced again until AddTitle below replaces it.
        g_scene.RemoveTitle(rowPtr->title);
        rowPtr->title = g_scene.AddTitle(std::move(freshTitle));
        // AddTitle only regenerates the id on a collision, which can't
        // happen here (the one Title that could collide was just removed),
        // but resync anyway so the config always matches what Scene assigned.
        rowPtr->id = rowPtr->title->id;
        rowPtr->title->dataSourceId = rowPtr->dataSourceId;
        registerTriggerCallbacks(rowPtr);
    }

    // The open dialog (if any) caches rowPtr but derives its combo/table
    // state from rowPtr->title, which just became a different object.
    if (m_rowWidgets[row].settingsDialog)
        m_rowWidgets[row].settingsDialog->setRow(rowPtr);

    reassignZOrders();
    refreshRowBrokenState(row);
}

void GraphicsDockWidget::onRemoveTitle(int row)
{
    if (row < 0 || row >= (int)m_rows.size())
        return;

    QString name = rowDisplayName(m_rows[row].get());
    auto answer = QMessageBox::question(
        this, "Remove Title",
        QString("Remove \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::Cancel);
    if (answer != QMessageBox::Yes)
        return;

    if (m_rowWidgets[row].settingsDialog) {
        delete m_rowWidgets[row].settingsDialog;
        m_rowWidgets[row].settingsDialog = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        g_scene.RemoveTitle(m_rows[row]->title);
    }

    m_table->removeRow(row);
    m_rows.erase(m_rows.begin() + row);
    m_rowWidgets.erase(m_rowWidgets.begin() + row);

    reassignZOrders();
    saveConfig();
}

void GraphicsDockWidget::saveConfig()
{
    if (m_loading)
        return;

    AppConfig cfg;
    for (auto& id : g_scene.Pool().Ids()) {
        if (auto* src = g_scene.Pool().Get(id))
            cfg.dataSources.push_back({id, src->GetFilePath()});
    }
    for (auto& row : m_rows)
        cfg.titles.push_back({row->id, row->path, row->dataSourceId, row->duration});
    cfg.Save(m_configPath);
}

std::string GraphicsDockWidget::computeConfigPath() const
{
    char* profileRaw = obs_frontend_get_current_profile();
    char* collectionRaw = obs_frontend_get_current_scene_collection();
    std::string profile = sanitizeForPath(profileRaw ? profileRaw : "");
    std::string collection = sanitizeForPath(collectionRaw ? collectionRaw : "");
    bfree(profileRaw);
    bfree(collectionRaw);

    auto dir = std::filesystem::path(m_configDir) / "profiles" / profile / collection;
    std::filesystem::create_directories(dir);
    return (dir / "config.json").string();
}

void GraphicsDockWidget::clearTitles()
{
    for (auto& rw : m_rowWidgets) {
        if (rw.settingsDialog) {
            delete rw.settingsDialog;
            rw.settingsDialog = nullptr;
        }
    }

    // Scene::Clear() drops every Title and Pool().Clear() drops every data
    // source, in one lock scope. Unlike the old TitleSlot/g_data_pool design
    // there is no separate teardown-ordering dance to get right here — Scene
    // owns both halves, so nothing can observe a Title with a dangling
    // dataSourceId or a source outliving the Scene in between.
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        g_scene.Clear();
        g_scene.Pool().Clear();
    }

    // Refresh the app SettingsDialog's Data Sources tab now, while the pool
    // is already empty and "stale" is unambiguous. Left stale, its Reload/
    // Remove buttons capture the old id by value and would silently
    // re-populate the pool under whatever context loadConfig() switches to
    // next — inserted there as if it were a legitimate entry of the new
    // context, then written into that context's config on save.
    refreshDataSourceUi();

    m_table->setRowCount(0);
    m_rows.clear();
    m_rowWidgets.clear();
}

void GraphicsDockWidget::reloadForCurrentContext()
{
    clearTitles();

    m_configPath = computeConfigPath();
    loadConfig();
}

void GraphicsDockWidget::loadConfig()
{
    AppConfig cfg = AppConfig::Load(m_configPath);

    // One-time migration: pre-4.x versions kept a single config shared by
    // every profile/scene collection at m_configDir/config.json. Adopt it
    // into whichever context is active on first run, then retire it so it
    // isn't re-adopted by the next profile/collection the user switches to.
    // Gated on the *current* context having nothing at all yet — a context
    // that already has data sources (even with zero titles: e.g. Settings ->
    // Data Sources was used before any title was ever added) must never have
    // that already-parsed state discarded by an `cfg = ...` wholesale
    // replacement below.
    if (cfg.titles.empty() && cfg.dataSources.empty()) {
        auto legacyPath = std::filesystem::path(m_configDir) / "config.json";
        if (std::filesystem::exists(legacyPath)) {
            AppConfig legacy = AppConfig::Load(legacyPath.string());
            if (!legacy.titles.empty() || !legacy.dataSources.empty()) {
                cfg = std::move(legacy);
                cfg.Save(m_configPath);
            }
            std::error_code ec;
            std::filesystem::rename(legacyPath, legacyPath.string() + ".migrated", ec);
        }
    }

    if (cfg.titles.empty() && cfg.dataSources.empty())
        return;

    m_loading = true;

    // Register sources before the title loop below, so each title's
    // dataSourceId has something to resolve against. Registered
    // independently of whether cfg.titles is empty — a data source added via
    // Settings before any title exists must survive. Not gated on
    // std::filesystem::exists: JsonFileDataSource/CsvFileDataSource/
    // ScriptDataSource's constructors only store the path and never read the
    // file, so a source that's temporarily unreachable (unmounted network
    // share, etc.) still registers successfully and only fails later, in
    // GetData() — dropping it here on a transient exists() miss is what used
    // to erase the entry (and any title's binding to it) on the next save.
    for (auto& de : cfg.dataSources) {
        if (de.path.empty())
            continue;
        try {
            auto src = makeDataSource(de.path);
            src->SetId(de.id); // restore the persisted id before Add() keys the registry by it
            g_scene.Pool().Add(std::move(src));
        } catch (...) {
            // silently skip unloadable data sources on startup
        }
    }

    for (auto& entry : cfg.titles) {
        if (entry.path.empty() || !std::filesystem::exists(entry.path))
            continue;

        std::unique_ptr<Title> t;
        try {
            t = std::make_unique<Title>(Title::Load(entry.path));
        } catch (...) {
            continue;
        }
        // Startup must never block on a modal — log both severities instead.
        reportLoadDiagnostic(this, *t, /*allowModal=*/false);

        auto row = std::make_unique<TitleRow>();
        row->path = entry.path;
        row->duration = entry.duration;
        row->id = entry.id; // AppConfig::Load guarantees this is a non-empty, minted uuid
        t->id = entry.id;
        if (!entry.dataSourceId.empty() && g_scene.Pool().Has(entry.dataSourceId)) {
            row->dataSourceId = entry.dataSourceId;
            t->dataSourceId = entry.dataSourceId;
        }

        TitleRow* rowPtr = row.get();
        {
            std::lock_guard<std::mutex> lock(g_scene_mutex);
            rowPtr->title = g_scene.AddTitle(std::move(t));
            // Resync in case AddTitle had to regenerate on an id collision
            // (e.g. a hand-edited config duplicating an id).
            rowPtr->id = rowPtr->title->id;
            registerTriggerCallbacks(rowPtr);
        }

        addTitleRow(std::move(row));
    }

    m_loading = false;
    reassignZOrders();

    // Reflect whatever this context just registered — if the app
    // SettingsDialog was left open across a profile/scene-collection switch,
    // it was already cleared to empty by clearTitles(); this repopulates it
    // with the new context's actual data sources instead of leaving it blank.
    refreshDataSourceUi();
}

void GraphicsDockWidget::updateOpenEditorEnabled()
{
    bool enabled = !m_settings.editorPath.empty() &&
                   std::filesystem::exists(m_settings.editorPath);
    m_openEditorBtn->setEnabled(enabled);
}

void GraphicsDockWidget::refreshDataSourceUi()
{
    if (m_settingsDialog) {
        QList<DataSourceRow> rows;
        for (auto& id : g_scene.Pool().Ids()) {
            auto* src = g_scene.Pool().Get(id);
            if (!src)
                continue;
            // The pool key is a uuid now, so both the display path and the
            // health check come from the source itself rather than the key.
            // A broken source (file gone, or a script that failed to load)
            // doesn't unregister — it just can't fetch, so flag it rather
            // than letting it look healthy while its titles quietly stop
            // updating.
            auto health = checkDataSourceHealth(src);

            DataSourceRow row;
            row.id = QString::fromStdString(id);
            row.path = QString::fromStdString(src->GetFilePath());
            row.broken = health.broken;
            row.brokenReason = health.reason;
            rows.push_back(row);
        }
        m_settingsDialog->setDataSources(rows);
    }

    for (auto& rw : m_rowWidgets) {
        if (rw.settingsDialog)
            rw.settingsDialog->refreshDataSourceList();
    }

    // Every pool mutation (add/reload/remove a data source) and every
    // profile/scene-collection switch routes through here — the single hook
    // that keeps the dock's own title-row warning icons in sync with the
    // pool, without needing separate call sites at each of those places.
    for (int r = 0; r < (int)m_rows.size(); ++r)
        refreshRowBrokenState(r);
}

void GraphicsDockWidget::onAppSettingsClicked()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
        connect(m_settingsDialog, &SettingsDialog::editorPathChanged, this, [this](const QString& path) {
            m_settings.editorPath = path.toStdString();
            m_settings.Save(m_settingsPath);
            updateOpenEditorEnabled();
        });

        connect(m_settingsDialog, &SettingsDialog::dataSourceAddRequested, this, [this](const QString& path) {
            std::string pathStr = path.toStdString();
            auto ds = tryMakeDataSource(this, pathStr, "load");
            if (!ds)
                return;

            g_scene.Pool().Add(std::move(ds)); // the source's id self-generates
            saveConfig();
            refreshDataSourceUi();
        });

        connect(m_settingsDialog, &SettingsDialog::dataSourceReloadRequested, this, [this](const QString& id) {
            std::string idStr = id.toStdString();
            auto* existing = g_scene.Pool().Get(idStr);
            if (!existing)
                return; // removed from under us; nothing to reload

            auto ds = tryMakeDataSource(this, existing->GetFilePath(), "reload");
            if (!ds)
                return;

            // Add() over an existing id replaces the source in place, so
            // every title's dataSourceId keeps resolving to it — there is no
            // separate re-bind step any more. Titles pick the new cache up
            // on their next DataIfChanged: the fresh source's cache version
            // restarts at 1, and DataIfChanged compares versions for
            // inequality (not ordering), so it always reads as changed.
            ds->SetId(idStr);
            g_scene.Pool().Add(std::move(ds));
            refreshDataSourceUi();
        });

        connect(m_settingsDialog, &SettingsDialog::dataSourceRemoveRequested, this, [this](const QString& id) {
            std::string idStr = id.toStdString();

            // BoundTitleNames is gone; build the "Used by:" list ourselves by
            // scanning m_rows, reading each matching Title's name under the
            // scene lock (falling back to the file stem when the .ogt has no
            // name of its own).
            QStringList names;
            {
                std::lock_guard<std::mutex> lock(g_scene_mutex);
                for (auto& row : m_rows) {
                    if (row->dataSourceId != idStr)
                        continue;
                    std::string titleName = row->title ? row->title->name : std::string();
                    if (!titleName.empty())
                        names << QString::fromStdString(titleName);
                    else
                        names << displayNameForPath(QString::fromStdString(row->path), "Title");
                }
            }

            QString detail = names.isEmpty() ? QString("No titles currently use this data source.")
                                              : QString("Used by: %1").arg(names.join(", "));

            auto answer = QMessageBox::question(
                this, "Remove Data Source",
                QString("Remove this data source?\n\n%1").arg(detail),
                QMessageBox::Yes | QMessageBox::Cancel);
            if (answer != QMessageBox::Yes)
                return;

            g_scene.Pool().Remove(idStr);

            {
                std::lock_guard<std::mutex> lock(g_scene_mutex);
                for (auto& row : m_rows) {
                    if (row->dataSourceId != idStr)
                        continue;
                    row->dataSourceId.clear();
                    if (row->title)
                        row->title->dataSourceId.clear();
                }
            }

            saveConfig();
            refreshDataSourceUi();
        });
    }

    m_settingsDialog->setEditorPath(QString::fromStdString(m_settings.editorPath));
    refreshDataSourceUi();
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void GraphicsDockWidget::onOpenEditorClicked()
{
    if (m_settings.editorPath.empty())
        return;
    QProcess::startDetached(QString::fromStdString(m_settings.editorPath));
}
