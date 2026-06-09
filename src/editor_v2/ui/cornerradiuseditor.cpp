#include "cornerradiuseditor.h"
#include <QGridLayout>
#include <QStyle>

#include "../icons.h"

CornerRadiusEditor::CornerRadiusEditor(QWidget* parent) : QWidget{parent}
{
    auto* layout = new QGridLayout(this);
    layout->setColumnStretch(0, 0);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(2, 0);
    layout->setRowStretch(0, 0);
    layout->setRowStretch(1, 1);
    layout->setRowStretch(2, 0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_tl = new QDoubleSpinBox();
    m_tl->setDecimals(1);
    m_tl->setRange(0, 9999);
    m_tl->setSingleStep(0.1);
    layout->addWidget(m_tl, 0, 0);

    m_tr = new QDoubleSpinBox();
    m_tr->setDecimals(1);
    m_tr->setRange(0, 9999);
    m_tr->setSingleStep(0.1);
    layout->addWidget(m_tr, 0, 2);

    m_br = new QDoubleSpinBox();
    m_br->setDecimals(1);
    m_br->setRange(0, 9999);
    m_br->setSingleStep(0.1);
    layout->addWidget(m_br, 2, 2);

    m_bl = new QDoubleSpinBox();
    m_bl->setDecimals(1);
    m_bl->setRange(0, 9999);
    m_bl->setSingleStep(0.1);
    layout->addWidget(m_bl, 2, 0);

    m_link = new QPushButton(qta()->icon(fa::fa_solid, fa::fa_link), "");
    m_link->setToolTip("Link");
    m_link->setCheckable(true);
    layout->addWidget(m_link, 1, 1, Qt::AlignCenter);

    connect(m_tl, &QDoubleSpinBox::valueChanged, this, &CornerRadiusEditor::onSpinChange);
    connect(m_tr, &QDoubleSpinBox::valueChanged, this, &CornerRadiusEditor::onSpinChange);
    connect(m_br, &QDoubleSpinBox::valueChanged, this, &CornerRadiusEditor::onSpinChange);
    connect(m_bl, &QDoubleSpinBox::valueChanged, this, &CornerRadiusEditor::onSpinChange);
    connect(m_link, &QPushButton::toggled, this, &CornerRadiusEditor::onLinkClick);
}

void CornerRadiusEditor::setValues(float tl, float tr, float br, float bl)
{
    m_updating = true;
    m_tl->setValue(static_cast<double>(tl));
    m_tr->setValue(static_cast<double>(tr));
    m_br->setValue(static_cast<double>(br));
    m_bl->setValue(static_cast<double>(bl));
    m_updating = false;
}

void CornerRadiusEditor::getValues(float& tl, float& tr, float& br, float& bl) const
{
    tl = static_cast<float>(m_tl->value());
    tr = static_cast<float>(m_tr->value());
    br = static_cast<float>(m_br->value());
    bl = static_cast<float>(m_bl->value());
}

void CornerRadiusEditor::onSpinChange(double value)
{
    if (m_updating)
        return;

    if (m_link->isChecked()) {
        setValues(value, value, value, value);
    }

    emit valuesChanged(static_cast<float>(m_tl->value()), static_cast<float>(m_tr->value()),
                       static_cast<float>(m_br->value()), static_cast<float>(m_bl->value()));
}

void CornerRadiusEditor::onLinkClick(bool checked)
{
    m_tr->setEnabled(!checked);
    m_br->setEnabled(!checked);
    m_bl->setEnabled(!checked);
    m_link->setToolTip(checked ? "Unlink" : "Link");
    m_link->setIcon(qta()->icon(fa::fa_solid, checked ? fa::fa_link_slash : fa::fa_link));

    if (!checked)
        return;

    // Sync all to TL value
    double val = m_tl->value();
    m_updating = true;
    m_tr->setValue(val);
    m_br->setValue(val);
    m_bl->setValue(val);
    m_updating = false;

    emit valuesChanged(static_cast<float>(val), static_cast<float>(val), static_cast<float>(val),
                       static_cast<float>(val));
}

void CornerRadiusEditor::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    auto pMin = m_tl->geometry().center();
    auto pMax = m_br->geometry().center();
    QRect rect(pMin, pMax);

    p.setPen(QPen(QColor::fromString("#777"), 1.0));
    p.drawRect(rect);
}
