#pragma once

#include <QWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

class GraphicsDockWidget : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsDockWidget(QWidget *parent = nullptr);

private slots:
    void onLoadClicked();
    void onToggleGraphic(int graphicIndex);

private:
    void rebuildTable();

    QPushButton  *m_loadBtn;
    QTableWidget *m_table;
};
