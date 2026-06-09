#pragma once

#include "engine/data-source.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class GraphicsDockWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDockWidget(QWidget* parent, std::string configPath);

private slots:
    void onLoadClicked();
    void onToggleGraphic(int graphicIndex);

private:
    void rebuildTable();
    void rebuildDataSourceCell(int row, int graphicIndex);
    void onLoadDataSource(int graphicIndex);
    void onRemoveDataSource(int graphicIndex);
    void saveConfig();
    void loadConfig();

    QPushButton* m_loadBtn;
    QTableWidget* m_table;

    std::vector<std::unique_ptr<IDataSource>> m_dataSources;

    std::string m_configPath;
    std::string m_scenePath;
    bool m_loading{false};
};
