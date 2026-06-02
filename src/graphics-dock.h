#pragma once

#include "engine/data-source.h"

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <memory>
#include <vector>

class GraphicsDockWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDockWidget(QWidget *parent = nullptr);

private slots:
    void onLoadClicked();
    void onToggleGraphic(int graphicIndex);

private:
    void rebuildTable();
    void rebuildDataSourceCell(int row, int graphicIndex);
    void onLoadDataSource(int graphicIndex);
    void onRemoveDataSource(int graphicIndex);

    QPushButton  *m_loadBtn;
    QTableWidget *m_table;

    std::vector<std::unique_ptr<IDataSource>> m_dataSources;
};
