#include "graphics-dock.h"
#include "shared-scene.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>

GraphicsDockWidget::GraphicsDockWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    auto *toolbar = new QHBoxLayout();
    m_loadBtn = new QPushButton("Load JSON...");
    connect(m_loadBtn, &QPushButton::clicked, this, &GraphicsDockWidget::onLoadClicked);
    toolbar->addWidget(m_loadBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_table = new QTableWidget(0, 2);
    m_table->setHorizontalHeaderLabels({"Name", "Action"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->horizontalHeader()->resizeSection(1, 80);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    setLayout(layout);
}

void GraphicsDockWidget::onLoadClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this, "Load Scene JSON", QString(), "JSON Files (*.json)");
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
    std::lock_guard<std::mutex> lock(g_scene_mutex);

    m_table->setRowCount(0);

    for (int i = 0; i < (int)g_active_scene.graphics.size(); ++i) {
        const Graphic &g = g_active_scene.graphics[i];

        int row = m_table->rowCount();
        m_table->insertRow(row);

        m_table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(g.id)));

        bool isVisible = (g.state == GraphicState::Visible ||
                          g.state == GraphicState::AnimatingIn);
        auto *btn = new QPushButton(isVisible ? "Hide" : "Show");
        int capturedIndex = i;
        connect(btn, &QPushButton::clicked, this, [this, capturedIndex]() {
            onToggleGraphic(capturedIndex);
        });
        m_table->setCellWidget(row, 1, btn);
    }
}

void GraphicsDockWidget::onToggleGraphic(int graphicIndex)
{
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
            g.TriggerIn();
            nowVisible = true;
        }
    }

    if (auto *btn = qobject_cast<QPushButton *>(m_table->cellWidget(graphicIndex, 1)))
        btn->setText(nowVisible ? "Hide" : "Show");
}
