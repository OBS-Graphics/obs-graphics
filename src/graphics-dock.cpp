#include "graphics-dock.h"
#include "shared-scene.h"
#include "engine/data-source.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QStyle>

GraphicsDockWidget::GraphicsDockWidget(QWidget *parent)
    : QWidget(parent)
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

    rebuildTable();
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
        // compute text concatenating all values in the record like so: key: value, ...
        QString text;
        for (auto&& rec : records[i]) {
            if (!text.isEmpty())
                text += ", ";
            text += QString::fromStdString(rec.first) + ": " + QString::fromStdString(rec.second);
        }
        combo->addItem(text);
    }
    hbox->addWidget(combo, 1);

    auto *removeBtn = new QPushButton();  // UTF-8 for ×
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
