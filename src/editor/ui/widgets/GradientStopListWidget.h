#pragma once

#include <vector>
#include <QWidget>
#include "engine/types.hpp"

class QTableWidget;
class QPushButton;

class GradientStopListWidget : public QWidget {
    Q_OBJECT
public:
    explicit GradientStopListWidget(QWidget* parent = nullptr);

    void setStops(const std::vector<Paint::Stop>& stops);
    std::vector<Paint::Stop> getStops() const;

signals:
    void stopsChanged(std::vector<Paint::Stop> stops);

private slots:
    void onAddStop();
    void onRemoveStop();
    void onCellChanged(int row, int col);

private:
    void rebuildTable(const std::vector<Paint::Stop>& stops);
    void emitStops();

    QTableWidget* m_table;
    QPushButton*  m_addBtn;
    QPushButton*  m_removeBtn;
    bool          m_updating{false};
};
