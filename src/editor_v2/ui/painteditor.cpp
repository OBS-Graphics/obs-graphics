#include "painteditor.h"
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <qcoreevent.h>

PaintEditor::PaintEditor(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QGridLayout(this);
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* typeLabel = new QLabel("Type");
    typeComboBox = new QComboBox;
    typeComboBox->addItems({"None", "Solid", "Linear Gradient", "Radial Gradient"});
    typeComboBox->installEventFilter(this);
    layout->addWidget(typeLabel, 0, 0);
    layout->addWidget(typeComboBox, 0, 1);

    connect(typeComboBox, &QComboBox::currentIndexChanged, this, &PaintEditor::onTypeChanged);

    mainSelector = new AdaptiveStack;

    // Page 0: solid
    m_cpSolid = new ColorPicker();
    m_cpSolid->setGeometry(0, 0, 80, 80);
    m_cpSolid->setColor(Qt::black);
    mainSelector->addWidget(m_cpSolid);

    connect(m_cpSolid, &ColorPicker::colorChanged, this, &PaintEditor::onColorChanged);

    // Page 1: Linear
    {
        auto* page = new QGroupBox();
        auto* layout = new QGridLayout(page);
        layout->setColumnStretch(0, 0);
        layout->setColumnStretch(1, 1);
        layout->setRowStretch(0, 0);

        m_linearEditor = new LinearGradientEditor();
        m_linearEditor->setGeometry(0, 0, 1, 64);
        layout->addWidget(m_linearEditor, 0, 0, 1, 2);

        QLabel* label = new QLabel("Position");
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(label, 1, 0);

        m_lnStopPosition = new QDoubleSpinBox();
        m_lnStopPosition->setMinimum(0.0);
        m_lnStopPosition->setMaximum(1.0);
        m_lnStopPosition->setDecimals(2);
        m_lnStopPosition->setSingleStep(0.1);
        layout->addWidget(m_lnStopPosition, 1, 1);

        m_cpLinear = new ColorPicker();
        m_cpLinear->setGeometry(0, 0, 80, 80);
        m_cpLinear->setColor(Qt::black);
        layout->addWidget(m_cpLinear, 2, 0, 1, 2);

        mainSelector->addWidget(page);

        connect(m_cpLinear, &ColorPicker::colorChanged, this, &PaintEditor::onColorChanged);
        connect(m_lnStopPosition, &QDoubleSpinBox::valueChanged, this,
                &PaintEditor::onStopPosChanged);
        connect(m_linearEditor, &LinearGradientEditor::stopsChanged, this,
                &PaintEditor::onStopsChanged);
        connect(m_linearEditor, &LinearGradientEditor::stopSelected, this,
                &PaintEditor::onStopSelected);

        m_linearEditor->setStops({{0.0, Qt::black}, {1.0, Qt::white}});
    }

    // Page 2: Radial
    {
        auto* page = new QGroupBox();
        auto* layout = new QGridLayout(page);
        layout->setColumnStretch(0, 0);
        layout->setColumnStretch(1, 1);
        layout->setRowStretch(0, 0);

        m_radialEditor = new RadialGradientEditor();
        m_radialEditor->setGeometry(0, 0, 1, 64);
        layout->addWidget(m_radialEditor, 0, 0, 1, 2);

        QLabel* label = new QLabel("Position");
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(label, 1, 0);

        m_radStopPosition = new QDoubleSpinBox();
        m_radStopPosition->setMinimum(0.0);
        m_radStopPosition->setMaximum(1.0);
        m_radStopPosition->setDecimals(2);
        m_radStopPosition->setSingleStep(0.1);
        layout->addWidget(m_radStopPosition, 1, 1);

        m_cpRadial = new ColorPicker();
        m_cpRadial->setGeometry(0, 0, 80, 80);
        m_cpRadial->setColor(Qt::black);
        layout->addWidget(m_cpRadial, 2, 0, 1, 2);

        mainSelector->addWidget(page);

        connect(m_cpRadial, &ColorPicker::colorChanged, this, &PaintEditor::onColorChanged);
        connect(m_radStopPosition, &QDoubleSpinBox::valueChanged, this,
                &PaintEditor::onStopPosChanged);
        connect(m_radialEditor, &RadialGradientEditor::stopsChanged, this,
                &PaintEditor::onStopsChanged);
        connect(m_radialEditor, &RadialGradientEditor::stopSelected, this,
                &PaintEditor::onStopSelected);

        m_radialEditor->setStops({{0.0, Qt::black}, {1.0, Qt::white}});
    }

    mainSelector->hide();

    layout->addWidget(mainSelector, 1, 0, 1, 2);
}

Paint PaintEditor::getPaint() const
{
    int idx = typeComboBox->currentIndex();
    Paint paint{};

    switch (idx) {
    case 0: // None
        break;
    case 1: // Solid
    {
        auto col = m_cpSolid->color();
        paint = Paint::Solid(col.redF(), col.greenF(), col.blueF(), col.alphaF());
        break;
    }
    case 2: // Linear
    {
        auto p1 = m_linearEditor->p1();
        auto p2 = m_linearEditor->p2();
        paint = Paint::Linear(p1.x(), p1.y(), p2.x(), p2.y());
        for (auto st : m_linearEditor->stops()) {
            Paint::Stop est{};
            est.offset = st.position;
            est.r = st.color.redF();
            est.g = st.color.greenF();
            est.b = st.color.blueF();
            est.a = st.color.alphaF();
            paint.stops.push_back(est);
        }
    } break;
    case 3: // Radial
    {
        auto p = m_radialEditor->center();
        paint = Paint::Radial(p.x(), p.y(), 0.0, p.x(), p.y(), m_radialEditor->radius());
        for (auto st : m_radialEditor->stops()) {
            Paint::Stop est{};
            est.offset = st.position;
            est.r = st.color.redF();
            est.g = st.color.greenF();
            est.b = st.color.blueF();
            est.a = st.color.alphaF();
            paint.stops.push_back(est);
        }
    } break;
    }

    return paint;
}

void PaintEditor::setPaint(const Paint& paint)
{
    m_updating = true;

    // Set combo to matching type
    int idx = 0;
    switch (paint.type) {
    case Paint::Type::None:
        idx = 0;
        break;
    case Paint::Type::Solid:
        idx = 1;
        break;
    case Paint::Type::Linear:
        idx = 2;
        break;
    case Paint::Type::Radial:
        idx = 3;
        break;
    }
    typeComboBox->setCurrentIndex(idx);

    if (idx == 0) {
        mainSelector->hide();
    } else {
        mainSelector->show();
        mainSelector->setCurrentIndex(idx - 1);
    }

    switch (paint.type) {
    case Paint::Type::None:
        break;
    case Paint::Type::Solid:
        m_cpSolid->setColor(
            QColor::fromRgbF(paint.params[0], paint.params[1], paint.params[2], paint.params[3]));
        break;
    case Paint::Type::Linear: {
        m_linearEditor->setP1(QPointF(paint.params[0], paint.params[1]));
        m_linearEditor->setP2(QPointF(paint.params[2], paint.params[3]));
        QVector<GradientStop> stops;
        for (auto st : paint.stops) {
            stops.push_back(GradientStop{
                .position = st.offset,
                .color = QColor::fromRgbF(st.r, st.g, st.b, st.a),
            });
        }
        m_linearEditor->setStops(stops);
    } break;
    case Paint::Type::Radial: {
        m_radialEditor->setCenter(QPointF(paint.params[0], paint.params[1]));
        m_radialEditor->setRadius(paint.params[2]);
        QVector<GradientStop> stops;
        for (auto st : paint.stops) {
            stops.push_back(GradientStop{
                .position = st.offset,
                .color = QColor::fromRgbF(st.r, st.g, st.b, st.a),
            });
        }
        m_radialEditor->setStops(stops);
    } break;
    }

    m_updating = false;
}

void PaintEditor::onTypeChanged(int index)
{
    if (index == 0) {
        mainSelector->hide();
    } else {
        mainSelector->setCurrentIndex(index - 1);
        mainSelector->show();
    }

    emitPaint();
}

void PaintEditor::onColorChanged(const QColor& color)
{
    auto* cp = qobject_cast<ColorPicker*>(sender());
    if (!cp)
        return;

    if (cp == m_cpLinear) {
        int stop = m_linearEditor->selectedStop();
        m_linearEditor->stop(stop).color = color;
        m_linearEditor->updateStops();
    } else if (cp == m_cpRadial) {
        int stop = m_radialEditor->selectedStop();
        m_radialEditor->stop(stop).color = color;
        m_radialEditor->updateStops();
    }

    emitPaint();
}

void PaintEditor::onStopPosChanged(double value)
{
    auto* pos = qobject_cast<QDoubleSpinBox*>(sender());
    if (!pos)
        return;

    if (pos == m_lnStopPosition) {
        int stop = m_linearEditor->selectedStop();
        m_linearEditor->stop(stop).position = std::clamp(value, 0.0, 1.0);
        m_linearEditor->updateStops();
    } else if (pos == m_radStopPosition) {
        int stop = m_radialEditor->selectedStop();
        m_radialEditor->stop(stop).position = std::clamp(value, 0.0, 1.0);
        m_radialEditor->updateStops();
    }

    emitPaint();
}

void PaintEditor::onStopsChanged(const QVector<GradientStop>& stops)
{
    emitPaint();
}

void PaintEditor::onStopSelected(int index)
{
    auto* widget = qobject_cast<QWidget*>(sender());
    if (!widget)
        return;

    if (auto* edt = dynamic_cast<LinearGradientEditor*>(widget)) {
        auto stop = m_linearEditor->stop(index);
        m_lnStopPosition->setValue(stop.position);
        m_lnStopPosition->setEnabled(index > 0 && index < m_linearEditor->stops().size() - 1);
        m_cpLinear->setColor(stop.color);
    } else if (auto* edt = dynamic_cast<RadialGradientEditor*>(widget)) {
        auto stop = m_radialEditor->stop(index);
        m_radStopPosition->setValue(stop.position);
        m_radStopPosition->setEnabled(index > 0 && index < m_radialEditor->stops().size() - 1);
        m_cpRadial->setColor(stop.color);
    }
}

void PaintEditor::emitPaint()
{
    if (m_updating)
        return;
    emit paintChanged(getPaint());
}

bool PaintEditor::filterScroll(QObject* obj, QEvent* event)
{
    if (obj == typeComboBox && event->type() == QEvent::Wheel) {
        event->ignore();
        return true; // consumed — parent won't scroll either
    }
    return QWidget::eventFilter(obj, event);
}
