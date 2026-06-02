#include "GradientStopListWidget.h"
#include "ColorPickerButton.h"

#include <QTableWidget>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

GradientStopListWidget::GradientStopListWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({"Offset", "Color"});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setMinimumHeight(80);
    layout->addWidget(m_table);

    auto* btnLayout = new QHBoxLayout;
    m_addBtn    = new QPushButton("Add Stop", this);
    m_removeBtn = new QPushButton("Remove Stop", this);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    connect(m_addBtn,    &QPushButton::clicked, this, &GradientStopListWidget::onAddStop);
    connect(m_removeBtn, &QPushButton::clicked, this, &GradientStopListWidget::onRemoveStop);
}

void GradientStopListWidget::setStops(const std::vector<Paint::Stop>& stops)
{
    rebuildTable(stops);
}

std::vector<Paint::Stop> GradientStopListWidget::getStops() const
{
    std::vector<Paint::Stop> stops;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        auto* sb = qobject_cast<QDoubleSpinBox*>(m_table->cellWidget(row, 0));
        auto* cp = qobject_cast<ColorPickerButton*>(m_table->cellWidget(row, 1));
        if (!sb || !cp) continue;

        Paint::Stop s;
        s.offset = sb->value();
        cp->getColor(s.r, s.g, s.b, s.a);
        stops.push_back(s);
    }
    return stops;
}

void GradientStopListWidget::rebuildTable(const std::vector<Paint::Stop>& stops)
{
    m_updating = true;
    m_table->setRowCount(0);
    for (const auto& stop : stops) {
        int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* sb = new QDoubleSpinBox(m_table);
        sb->setRange(0.0, 1.0);
        sb->setDecimals(3);
        sb->setSingleStep(0.05);
        sb->setValue(stop.offset);
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { emitStops(); });
        m_table->setCellWidget(row, 0, sb);

        auto* cp = new ColorPickerButton(m_table);
        cp->setColor(stop.r, stop.g, stop.b, stop.a);
        connect(cp, &ColorPickerButton::colorChanged,
                this, [this](double, double, double, double) { emitStops(); });
        m_table->setCellWidget(row, 1, cp);

        m_table->setRowHeight(row, 30);
    }
    m_updating = false;
}

void GradientStopListWidget::emitStops()
{
    if (m_updating) return;
    emit stopsChanged(getStops());
}

void GradientStopListWidget::onAddStop()
{
    auto stops = getStops();

    // Determine a good offset: midpoint between last two, or 0.5 if empty
    double newOffset = 0.5;
    if (stops.size() == 1) {
        newOffset = stops[0].offset < 0.5 ? 0.75 : 0.25;
    } else if (stops.size() >= 2) {
        // Find the largest gap
        std::sort(stops.begin(), stops.end(),
                  [](const Paint::Stop& a, const Paint::Stop& b){ return a.offset < b.offset; });
        double maxGap = -1.0;
        int    gapIdx = 0;
        for (int i = 0; i + 1 < static_cast<int>(stops.size()); ++i) {
            double gap = stops[i + 1].offset - stops[i].offset;
            if (gap > maxGap) { maxGap = gap; gapIdx = i; }
        }
        newOffset = (stops[gapIdx].offset + stops[gapIdx + 1].offset) * 0.5;
    }

    Paint::Stop s;
    s.offset = newOffset;
    s.r = 1.0; s.g = 1.0; s.b = 1.0; s.a = 1.0;
    stops.push_back(s);

    std::sort(stops.begin(), stops.end(),
              [](const Paint::Stop& a, const Paint::Stop& b){ return a.offset < b.offset; });

    rebuildTable(stops);
    emit stopsChanged(stops);
}

void GradientStopListWidget::onRemoveStop()
{
    int row = m_table->currentRow();
    if (row < 0 || m_table->rowCount() == 0) return;

    auto stops = getStops();
    if (row < static_cast<int>(stops.size()))
        stops.erase(stops.begin() + row);

    rebuildTable(stops);
    emit stopsChanged(stops);
}

void GradientStopListWidget::onCellChanged(int /*row*/, int /*col*/)
{
    emitStops();
}
