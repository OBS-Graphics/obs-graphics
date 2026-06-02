#include "CornerRadiusWidget.h"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>

CornerRadiusWidget::CornerRadiusWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto makeSpinBox = [this]() -> QDoubleSpinBox* {
        auto* sb = new QDoubleSpinBox(this);
        sb->setRange(0.0, 2000.0);
        sb->setDecimals(1);
        sb->setSingleStep(1.0);
        sb->setSuffix(" px");
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &CornerRadiusWidget::onSpinChanged);
        return sb;
    };

    m_tl = makeSpinBox();
    m_tr = makeSpinBox();
    m_br = makeSpinBox();
    m_bl = makeSpinBox();

    m_link = new QCheckBox("Link", this);
    connect(m_link, &QCheckBox::toggled, this, &CornerRadiusWidget::onLinkToggled);

    // Grid:  TL label | TL spin | TR label | TR spin
    //        BL label | BL spin | BR label | BR spin
    //        link checkbox (span 4)
    layout->addWidget(new QLabel("TL", this), 0, 0);
    layout->addWidget(m_tl,                   0, 1);
    layout->addWidget(new QLabel("TR", this), 0, 2);
    layout->addWidget(m_tr,                   0, 3);

    layout->addWidget(new QLabel("BL", this), 1, 0);
    layout->addWidget(m_bl,                   1, 1);
    layout->addWidget(new QLabel("BR", this), 1, 2);
    layout->addWidget(m_br,                   1, 3);

    layout->addWidget(m_link, 2, 0, 1, 4);
}

void CornerRadiusWidget::setValues(float tl, float tr, float br, float bl)
{
    m_updating = true;
    m_tl->setValue(static_cast<double>(tl));
    m_tr->setValue(static_cast<double>(tr));
    m_br->setValue(static_cast<double>(br));
    m_bl->setValue(static_cast<double>(bl));
    m_updating = false;
}

void CornerRadiusWidget::getValues(float& tl, float& tr, float& br, float& bl) const
{
    tl = static_cast<float>(m_tl->value());
    tr = static_cast<float>(m_tr->value());
    br = static_cast<float>(m_br->value());
    bl = static_cast<float>(m_bl->value());
}

void CornerRadiusWidget::onSpinChanged(double value)
{
    if (m_updating) return;

    if (m_link->isChecked()) {
        m_updating = true;
        m_tl->setValue(value);
        m_tr->setValue(value);
        m_br->setValue(value);
        m_bl->setValue(value);
        m_updating = false;
    }

    emit valuesChanged(
        static_cast<float>(m_tl->value()),
        static_cast<float>(m_tr->value()),
        static_cast<float>(m_br->value()),
        static_cast<float>(m_bl->value())
    );
}

void CornerRadiusWidget::onLinkToggled(bool checked)
{
    if (checked) {
        // Sync all to TL value
        double val = m_tl->value();
        m_updating = true;
        m_tr->setValue(val);
        m_br->setValue(val);
        m_bl->setValue(val);
        m_updating = false;

        emit valuesChanged(
            static_cast<float>(val),
            static_cast<float>(val),
            static_cast<float>(val),
            static_cast<float>(val)
        );
    }
}
