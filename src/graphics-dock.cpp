#include "graphics-dock.h"
#include "shared-scene.h"
#include "app-config.h"
#include "engine/data-source.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QStyle>

#include <filesystem>

GraphicsDockWidget::GraphicsDockWidget(QWidget *parent, std::string configPath)
    : QWidget(parent)
    , m_configPath(std::move(configPath))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *toolbar = new QHBoxLayout();
    m_loadBtn = new QPushButton("Load Scene...");
    connect(m_loadBtn, &QPushButton::clicked, this, &GraphicsDockWidget::onLoadClicked);
    toolbar->addWidget(m_loadBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_table = new QTableWidget(0, 3);
    m_table->setHorizontalHeaderLabels({"Name", "Data Source", "Action"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(1, 220);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(2, 80);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    setLayout(layout);

    loadConfig();
}

void GraphicsDockWidget::onLoadClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Load Scene", QString(), "JSON Files (*.json)");
    if (path.isEmpty())
        return;

    Scene loaded = Scene::Load(path.toStdString());

    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        g_active_scene  = std::move(loaded);
        g_scene_loaded  = true;
    }

    m_scenePath = path.toStdString();
    rebuildTable();
    saveConfig();
}

void GraphicsDockWidget::rebuildTable()
{
    struct GraphicInfo {
        std::string id;
        bool isVisible;
    };

    std::vector<GraphicInfo> infos;
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        infos.reserve(g_active_scene.graphics.size());
        for (auto& g : g_active_scene.graphics) {
            bool vis = (g.state == GraphicState::Visible || g.state == GraphicState::AnimatingIn);
            infos.push_back({g.id, vis});
        }
    }

    m_dataSources.clear();
    m_dataSources.resize(infos.size());

    m_table->setRowCount(0);

    for (int i = 0; i < (int)infos.size(); ++i) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        m_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(infos[i].id)));

        rebuildDataSourceCell(row, i);

        bool isVisible = infos[i].isVisible;
        auto *btn = new QPushButton(isVisible ? "Hide" : "Show");
        if (!isVisible) {
            btn->setStyleSheet("QPushButton { background-color: #28752a; color: white; font-weight: bold; }");
            btn->setIcon(this->style()->standardIcon(QStyle::SP_MediaPlay));
        } else {
            btn->setStyleSheet("QPushButton { background-color: #b42218; color: white; font-weight: bold; }");
            btn->setIcon(this->style()->standardIcon(QStyle::SP_MediaStop));
        }

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            onToggleGraphic(i);
        });
        m_table->setCellWidget(row, 2, btn);
    }
}

void GraphicsDockWidget::rebuildDataSourceCell(int row, int graphicIndex)
{
    if (!m_dataSources[graphicIndex]) {
        auto *btn = new QPushButton();
        btn->setText("Load");
        btn->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
        btn->setToolTip("Load data source...");
        connect(btn, &QPushButton::clicked, this, [this, graphicIndex]() {
            onLoadDataSource(graphicIndex);
        });
        m_table->setCellWidget(row, 1, btn);
        return;
    }

    auto records = m_dataSources[graphicIndex]->GetData();

    auto *container = new QWidget();
    auto *hbox = new QHBoxLayout(container);
    hbox->setContentsMargins(2, 2, 2, 2);
    hbox->setSpacing(4);

    auto *combo = new QComboBox();
    for (int i = 0; i < (int)records.size(); ++i) {
        QString text;
        for (auto&& rec : records[i]) {
            if (!text.isEmpty())
                text += ", ";
            text += QString::fromStdString(rec.first) + ": " + QString::fromStdString(rec.second);
        }
        combo->addItem(text);
    }
    connect(combo, &QComboBox::currentIndexChanged, this, [this](int) { saveConfig(); });
    hbox->addWidget(combo, 1);

    auto *removeBtn = new QPushButton();
    removeBtn->setFixedWidth(24);
    removeBtn->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    removeBtn->setToolTip("Remove data source");
    connect(removeBtn, &QPushButton::clicked, this, [this, graphicIndex]() {
        onRemoveDataSource(graphicIndex);
    });
    hbox->addWidget(removeBtn);

    m_table->setCellWidget(row, 1, container);
}

void GraphicsDockWidget::onLoadDataSource(int graphicIndex)
{
    QString path = QFileDialog::getOpenFileName(
        this, "Load Data Source", QString(),
        "Data Files (*.json *.csv);;JSON Files (*.json);;CSV Files (*.csv)");
    if (path.isEmpty())
        return;

    std::unique_ptr<IDataSource> ds;
    if (path.endsWith(".json", Qt::CaseInsensitive))
        ds = std::make_unique<JsonFileDataSource>(path.toStdString());
    else
        ds = std::make_unique<CsvFileDataSource>(path.toStdString());

    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        if (graphicIndex >= (int)g_active_scene.graphics.size())
            return;
        g_active_scene.graphics[graphicIndex].dataSource = ds.get();
    }

    m_dataSources[graphicIndex] = std::move(ds);
    rebuildDataSourceCell(graphicIndex, graphicIndex);
    saveConfig();
}

void GraphicsDockWidget::onRemoveDataSource(int graphicIndex)
{
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        if (graphicIndex < (int)g_active_scene.graphics.size())
            g_active_scene.graphics[graphicIndex].dataSource = nullptr;
    }

    m_dataSources[graphicIndex].reset();
    rebuildDataSourceCell(graphicIndex, graphicIndex);
    saveConfig();
}

void GraphicsDockWidget::onToggleGraphic(int graphicIndex)
{
    size_t recordIndex = 0;
    if (auto *cell = m_table->cellWidget(graphicIndex, 1)) {
        if (auto *combo = cell->findChild<QComboBox*>())
            recordIndex = static_cast<size_t>(std::max(0, combo->currentIndex()));
    }

    bool nowVisible = false;
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        if (graphicIndex < 0 || graphicIndex >= (int)g_active_scene.graphics.size())
            return;

        Graphic &g = g_active_scene.graphics[graphicIndex];
        bool isVisible = (g.state == GraphicState::Visible ||
                          g.state == GraphicState::AnimatingIn);
        if (isVisible) {
            g.TriggerOut();
            nowVisible = false;
        } else {
            g.TriggerIn(recordIndex);
            nowVisible = true;
        }
    }

    if (auto *btn = qobject_cast<QPushButton *>(m_table->cellWidget(graphicIndex, 2))) {
        btn->setText(nowVisible ? "Hide" : "Show");
        if (nowVisible) {
            btn->setStyleSheet("QPushButton { background-color: #b42218; color: white; font-weight: bold; }");
            btn->setIcon(this->style()->standardIcon(QStyle::SP_MediaStop));
        } else {
            btn->setStyleSheet("QPushButton { background-color: #28752a; color: white; font-weight: bold; }");
            btn->setIcon(this->style()->standardIcon(QStyle::SP_MediaPlay));
        }
    }
}

void GraphicsDockWidget::saveConfig()
{
    if (m_loading)
        return;

    AppConfig cfg;
    cfg.scenePath = m_scenePath;

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *item = m_table->item(row, 0);
        if (!item) continue;
        std::string id = item->text().toStdString();

        if (row < (int)m_dataSources.size() && m_dataSources[row]) {
            std::string path = m_dataSources[row]->GetFilePath();
            std::string type = path.ends_with(".csv") ? "csv" : "json";
            cfg.dataSources[id] = {path, type};
        }

        if (auto *cell = m_table->cellWidget(row, 1)) {
            if (auto *combo = cell->findChild<QComboBox*>())
                cfg.selectedRecords[id] = combo->currentIndex();
        }
    }

    cfg.Save(m_configPath);
}

void GraphicsDockWidget::loadConfig()
{
    AppConfig cfg = AppConfig::Load(m_configPath);
    if (cfg.scenePath.empty() || !std::filesystem::exists(cfg.scenePath))
        return;

    m_loading = true;

    try {
        Scene loaded = Scene::Load(cfg.scenePath);
        {
            std::lock_guard<std::mutex> lock(g_scene_mutex);
            g_active_scene = std::move(loaded);
            g_scene_loaded = true;
        }
        m_scenePath = cfg.scenePath;
    } catch (...) {
        m_loading = false;
        return;
    }

    rebuildTable();

    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto *item = m_table->item(row, 0);
        if (!item) continue;
        std::string id = item->text().toStdString();

        auto dsIt = cfg.dataSources.find(id);
        if (dsIt == cfg.dataSources.end()) continue;
        if (!std::filesystem::exists(dsIt->second.path)) continue;

        std::unique_ptr<IDataSource> ds;
        if (dsIt->second.type == "csv")
            ds = std::make_unique<CsvFileDataSource>(dsIt->second.path);
        else
            ds = std::make_unique<JsonFileDataSource>(dsIt->second.path);

        {
            std::lock_guard<std::mutex> lock(g_scene_mutex);
            if (row < (int)g_active_scene.graphics.size())
                g_active_scene.graphics[row].dataSource = ds.get();
        }
        m_dataSources[row] = std::move(ds);
        rebuildDataSourceCell(row, row);

        auto recIt = cfg.selectedRecords.find(id);
        if (recIt != cfg.selectedRecords.end()) {
            if (auto *cell = m_table->cellWidget(row, 1)) {
                if (auto *combo = cell->findChild<QComboBox*>())
                    combo->setCurrentIndex(recIt->second);
            }
        }
    }

    m_loading = false;
}
