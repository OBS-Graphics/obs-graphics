#include "PaintEditorWidget.h"
#include "ColorPickerButton.h"
#include "GradientStopListWidget.h"

#include <QComboBox>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>

PaintEditorWidget::PaintEditorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Type selector
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("None",            QVariant::fromValue(static_cast<int>(Paint::Type::None)));
    m_typeCombo->addItem("Solid",           QVariant::fromValue(static_cast<int>(Paint::Type::Solid)));
    m_typeCombo->addItem("Linear Gradient", QVariant::fromValue(static_cast<int>(Paint::Type::Linear)));
    m_typeCombo->addItem("Radial Gradient", QVariant::fromValue(static_cast<int>(Paint::Type::Radial)));
    mainLayout->addWidget(m_typeCombo);

    // Stacked widget
    m_stack = new QStackedWidget(this);
    mainLayout->addWidget(m_stack);

    // Page 0: None
    m_stack->addWidget(new QWidget(this));

    // Page 1: Solid
    {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(0, 0, 0, 0);
        m_solidColor = new ColorPickerButton(page);
        m_solidColor->setColor(1.0, 1.0, 1.0, 1.0);
        form->addRow("Color:", m_solidColor);
        connect(m_solidColor, &ColorPickerButton::colorChanged,
                this, &PaintEditorWidget::onSolidColorChanged);
        m_stack->addWidget(page);
    }

    // Page 2: Linear
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* form = new QFormLayout;
        m_linX0 = makeCoordSpinBox(); form->addRow("X0:", m_linX0);
        m_linY0 = makeCoordSpinBox(); form->addRow("Y0:", m_linY0);
        m_linX1 = makeCoordSpinBox(); m_linX1->setValue(1.0); form->addRow("X1:", m_linX1);
        m_linY1 = makeCoordSpinBox(); form->addRow("Y1:", m_linY1);

        for (auto* sb : {m_linX0, m_linY0, m_linX1, m_linY1}) {
            connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &PaintEditorWidget::onLinearParamChanged);
        }
        layout->addLayout(form);

        m_linearStops = new GradientStopListWidget(page);
        // Default stops
        std::vector<Paint::Stop> defaultStops = {{0.0, 0.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 1.0, 1.0, 1.0}};
        m_linearStops->setStops(defaultStops);
        connect(m_linearStops, &GradientStopListWidget::stopsChanged,
                this, &PaintEditorWidget::onLinearStopsChanged);
        layout->addWidget(m_linearStops);

        m_stack->addWidget(page);
    }

    // Page 3: Radial
    {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* form = new QFormLayout;
        m_radCx0 = makeCoordSpinBox(); m_radCx0->setValue(0.5); form->addRow("Cx0:", m_radCx0);
        m_radCy0 = makeCoordSpinBox(); m_radCy0->setValue(0.5); form->addRow("Cy0:", m_radCy0);
        m_radR0  = makeCoordSpinBox(); form->addRow("R0:", m_radR0);
        m_radCx1 = makeCoordSpinBox(); m_radCx1->setValue(0.5); form->addRow("Cx1:", m_radCx1);
        m_radCy1 = makeCoordSpinBox(); m_radCy1->setValue(0.5); form->addRow("Cy1:", m_radCy1);
        m_radR1  = makeCoordSpinBox(); m_radR1->setValue(0.5); form->addRow("R1:", m_radR1);

        for (auto* sb : {m_radCx0, m_radCy0, m_radR0, m_radCx1, m_radCy1, m_radR1}) {
            connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &PaintEditorWidget::onRadialParamChanged);
        }
        layout->addLayout(form);

        m_radialStops = new GradientStopListWidget(page);
        std::vector<Paint::Stop> defaultStops = {{0.0, 0.0, 0.0, 0.0, 1.0}, {1.0, 1.0, 1.0, 1.0, 1.0}};
        m_radialStops->setStops(defaultStops);
        connect(m_radialStops, &GradientStopListWidget::stopsChanged,
                this, &PaintEditorWidget::onRadialStopsChanged);
        layout->addWidget(m_radialStops);

        m_stack->addWidget(page);
    }

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PaintEditorWidget::onTypeChanged);
}

QDoubleSpinBox* PaintEditorWidget::makeCoordSpinBox()
{
    auto* sb = new QDoubleSpinBox(this);
    sb->setRange(-2.0, 3.0);
    sb->setDecimals(3);
    sb->setSingleStep(0.1);
    return sb;
}

void PaintEditorWidget::setPaint(const Paint& paint)
{
    m_updating = true;

    // Set combo to matching type
    int idx = 0;
    switch (paint.type) {
    case Paint::Type::None:   idx = 0; break;
    case Paint::Type::Solid:  idx = 1; break;
    case Paint::Type::Linear: idx = 2; break;
    case Paint::Type::Radial: idx = 3; break;
    }
    m_typeCombo->setCurrentIndex(idx);
    m_stack->setCurrentIndex(idx);

    switch (paint.type) {
    case Paint::Type::None:
        break;
    case Paint::Type::Solid:
        m_solidColor->setColor(paint.params[0], paint.params[1], paint.params[2], paint.params[3]);
        break;
    case Paint::Type::Linear:
        m_linX0->setValue(paint.params[0]);
        m_linY0->setValue(paint.params[1]);
        m_linX1->setValue(paint.params[2]);
        m_linY1->setValue(paint.params[3]);
        m_linearStops->setStops(paint.stops);
        break;
    case Paint::Type::Radial:
        m_radCx0->setValue(paint.params[0]);
        m_radCy0->setValue(paint.params[1]);
        m_radR0->setValue(paint.params[2]);
        m_radCx1->setValue(paint.params[3]);
        m_radCy1->setValue(paint.params[4]);
        m_radR1->setValue(paint.params[5]);
        m_radialStops->setStops(paint.stops);
        break;
    }

    m_updating = false;
}

Paint PaintEditorWidget::getPaint() const
{
    int idx = m_typeCombo->currentIndex();
    Paint paint;
    switch (idx) {
    case 0: // None
        break;
    case 1: // Solid
    {
        double r, g, b, a;
        m_solidColor->getColor(r, g, b, a);
        paint = Paint::Solid(r, g, b, a);
        break;
    }
    case 2: // Linear
        paint = Paint::Linear(m_linX0->value(), m_linY0->value(),
                              m_linX1->value(), m_linY1->value());
        paint.stops = m_linearStops->getStops();
        break;
    case 3: // Radial
        paint = Paint::Radial(m_radCx0->value(), m_radCy0->value(), m_radR0->value(),
                              m_radCx1->value(), m_radCy1->value(), m_radR1->value());
        paint.stops = m_radialStops->getStops();
        break;
    }
    return paint;
}

void PaintEditorWidget::onTypeChanged(int index)
{
    m_stack->setCurrentIndex(index);
    if (!m_updating)
        emitPaint();
}

void PaintEditorWidget::onSolidColorChanged(double, double, double, double)
{
    if (!m_updating) emitPaint();
}

void PaintEditorWidget::onLinearParamChanged()
{
    if (!m_updating) emitPaint();
}

void PaintEditorWidget::onRadialParamChanged()
{
    if (!m_updating) emitPaint();
}

void PaintEditorWidget::onLinearStopsChanged(std::vector<Paint::Stop>)
{
    if (!m_updating) emitPaint();
}

void PaintEditorWidget::onRadialStopsChanged(std::vector<Paint::Stop>)
{
    if (!m_updating) emitPaint();
}

void PaintEditorWidget::emitPaint()
{
    emit paintChanged(getPaint());
}
