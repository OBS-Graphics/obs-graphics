#include "adaptivestack.h"

AdaptiveStack::AdaptiveStack(QWidget* parent) : QWidget(parent) {}

void AdaptiveStack::addWidget(QWidget* w)
{
    w->setParent(this);
    w->hide();
    m_pages.append(w);
}

void AdaptiveStack::setCurrentIndex(int index)
{
    if (index == m_current)
        return;
    if (m_current >= 0 && m_current < m_pages.size())
        m_pages[m_current]->hide();
    m_current = index;
    if (m_current >= 0 && m_current < m_pages.size()) {
        m_pages[m_current]->setGeometry(rect());
        m_pages[m_current]->show();
    }
    updateGeometry();
    emit currentChanged(m_current);
}

int AdaptiveStack::currentIndex() const
{
    return m_current;
}

QWidget* AdaptiveStack::currentWidget() const
{
    if (m_current >= 0 && m_current < m_pages.size())
        return m_pages[m_current];
    return nullptr;
}

QSize AdaptiveStack::sizeHint() const
{
    auto* w = currentWidget();
    return w ? w->sizeHint() : QSize(0, 0);
}

QSize AdaptiveStack::minimumSizeHint() const
{
    auto* w = currentWidget();
    return w ? w->minimumSizeHint() : QSize(0, 0);
}

void AdaptiveStack::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (auto* w = currentWidget())
        w->setGeometry(rect());
}
