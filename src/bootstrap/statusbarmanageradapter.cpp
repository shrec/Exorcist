#include "statusbarmanageradapter.h"

#include <QLabel>
#include <QStatusBar>
#include <QWidget>

StatusBarManagerAdapter::StatusBarManagerAdapter(QStatusBar *statusBar,
                                                 QObject *parent)
    : QObject(parent)
    , m_statusBar(statusBar)
{
}

void StatusBarManagerAdapter::addWidget(const QString &id, QWidget *widget,
                                         Alignment alignment, int priority)
{
    if (m_items.contains(id))
        return;

    if (alignment == Right)
        m_statusBar->addPermanentWidget(widget);
    else
        m_statusBar->addWidget(widget);

    m_items.insert(id, {widget, alignment, priority});
}

bool StatusBarManagerAdapter::removeWidget(const QString &id)
{
    auto it = m_items.find(id);
    if (it == m_items.end())
        return false;

    m_statusBar->removeWidget(it->widget);
    m_items.erase(it);
    return true;
}

void StatusBarManagerAdapter::setText(const QString &id, const QString &text)
{
    auto it = m_items.find(id);
    if (it == m_items.end())
        return;

    auto *label = qobject_cast<QLabel *>(it->widget);
    if (label)
        label->setText(text);
}

void StatusBarManagerAdapter::setTooltip(const QString &id, const QString &tooltip)
{
    auto it = m_items.find(id);
    if (it != m_items.end())
        it->widget->setToolTip(tooltip);
}

void StatusBarManagerAdapter::setVisible(const QString &id, bool visible)
{
    auto it = m_items.find(id);
    if (it != m_items.end())
        it->widget->setVisible(visible);
}

void StatusBarManagerAdapter::showMessage(const QString &text, int timeoutMs)
{
    m_statusBar->showMessage(text, timeoutMs);
}

QStringList StatusBarManagerAdapter::itemIds() const
{
    return m_items.keys();
}

QWidget *StatusBarManagerAdapter::widget(const QString &id) const
{
    auto it = m_items.find(id);
    return (it != m_items.end()) ? it->widget : nullptr;
}
