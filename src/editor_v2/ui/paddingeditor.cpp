#include "paddingeditor.h"
#include "icons.h"
#include <QGridLayout>
#include <QPainter>

static QDoubleSpinBox* makeSpin()
{
    auto* s = new QDoubleSpinBox;
    s->setDecimals(1);
    s->setRange(0, 9999);
    s->setSingleStep(1.0);
    s->setMaximumWidth(70);
    return s;
}

PaddingEditor::PaddingEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_top    = makeSpin();
    m_right  = makeSpin();
    m_bottom = makeSpin();
    m_left   = makeSpin();

    // row 0 col 1: top
    layout->addWidget(m_top,    0, 1, Qt::AlignHCenter);
    // row 1 col 0: left, col 2: right
    layout->addWidget(m_left,   1, 0, Qt::AlignVCenter);
    layout->addWidget(m_right,  1, 2, Qt::AlignVCenter);
    // row 2 col 1: bottom
    layout->addWidget(m_bottom, 2, 1, Qt::AlignHCenter);

    m_link = new QPushButton(qta()->icon(fa::fa_solid, fa::fa_link), "");
    m_link->setToolTip("Link");
    m_link->setCheckable(true);
    layout->addWidget(m_link, 1, 1, Qt::AlignCenter);

    layout->setColumnStretch(1, 1);
    layout->setRowStretch(1, 1);

    connect(m_top,    &QDoubleSpinBox::valueChanged, this, &PaddingEditor::onSpinChange);
    connect(m_right,  &QDoubleSpinBox::valueChanged, this, &PaddingEditor::onSpinChange);
    connect(m_bottom, &QDoubleSpinBox::valueChanged, this, &PaddingEditor::onSpinChange);
    connect(m_left,   &QDoubleSpinBox::valueChanged, this, &PaddingEditor::onSpinChange);
    connect(m_link, &QPushButton::toggled, this, &PaddingEditor::onLinkClick);
}

void PaddingEditor::setValues(float top, float right, float bottom, float left)
{
    m_updating = true;
    m_top->setValue(top);
    m_right->setValue(right);
    m_bottom->setValue(bottom);
    m_left->setValue(left);
    m_updating = false;
}

void PaddingEditor::getValues(float& top, float& right, float& bottom, float& left) const
{
    top    = static_cast<float>(m_top->value());
    right  = static_cast<float>(m_right->value());
    bottom = static_cast<float>(m_bottom->value());
    left   = static_cast<float>(m_left->value());
}

void PaddingEditor::onSpinChange(double value)
{
    if (m_updating) return;

    if (m_link->isChecked()) {
        setValues(value, value, value, value);
    }

    emit paddingChanged(
        static_cast<float>(m_top->value()),
        static_cast<float>(m_right->value()),
        static_cast<float>(m_bottom->value()),
        static_cast<float>(m_left->value())
    );
}

void PaddingEditor::onLinkClick(bool linked)
{
    m_left->setEnabled(!linked);
    m_right->setEnabled(!linked);
    m_bottom->setEnabled(!linked);
    m_link->setToolTip(linked ? "Unlink" : "Link");
    m_link->setIcon(qta()->icon(fa::fa_solid, linked ? fa::fa_link_slash : fa::fa_link));

    if (!linked) return;

    // Sync all to TOP value
    double val = m_top->value();
    m_updating = true;
    m_left->setValue(val);
    m_right->setValue(val);
    m_bottom->setValue(val);
    m_updating = false;

    emit paddingChanged(
        static_cast<float>(val),
        static_cast<float>(val),
        static_cast<float>(val),
        static_cast<float>(val)
    );
}

void PaddingEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    const QPoint pMin(m_left->geometry().center().x(), m_top->geometry().center().y());
    const QPoint pMax(m_right->geometry().center().x(), m_bottom->geometry().center().y());
    const QRect rect(pMin, pMax);

    p.setPen(QPen(QColor::fromString("#777"), 1.0));
    p.drawRect(rect);
}
