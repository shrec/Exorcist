#include "ExDockWidget.h"

#include <QAction>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace exdock {

ExDockWidget::ExDockWidget(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
{
    // Give this widget a stable native HWND so that when DockManager
    // reparents it into a QStackedWidget (via insertWidget), Qt uses
    // Win32 SetParent() on our HWND rather than traversing our children
    // and reparenting their HWNDs individually. Without this, any
    // WA_NativeWindow child (e.g. UltralightWidget) would have its HWND
    // moved separately, which corrupts Qt's internal widget state on
    // Windows (RBP=1 / READ at 0xFFFF… crash).
    setAttribute(Qt::WA_NativeWindow);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Default size policy: dock panels can shrink and expand,
    // but prefer their preferred size. Central editor areas
    // should override with Expanding.
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumSize(m_minimumDockSize);

    m_toggleAction = new QAction(title, this);
    m_toggleAction->setCheckable(true);
    m_toggleAction->setChecked(false);
    // NOTE: No show()/closeDock() connection here. DockManager is the
    // sole authority on dock visibility. Connecting toggleAction directly
    // to show() caused dock widgets to appear as top-level windows on the
    // Windows taskbar when setState() was called during unpinDock/pinDock.
}

QSize ExDockWidget::sizeHint() const
{
    // If the content widget has its own sizeHint, respect it
    if (m_content && m_content->sizeHint().isValid())
        return m_content->sizeHint().expandedTo(m_minimumDockSize);
    return m_preferredSize;
}

QSize ExDockWidget::minimumSizeHint() const
{
    if (m_content && m_content->minimumSizeHint().isValid())
        return m_content->minimumSizeHint().expandedTo(m_minimumDockSize);
    return m_minimumDockSize;
}

void ExDockWidget::setContentWidget(QWidget *widget)
{
    if (m_content) {
        layout()->removeWidget(m_content);
        m_content->setParent(nullptr);
    }
    m_content = widget;
    if (m_content) {
        m_content->setParent(this);
        layout()->addWidget(m_content);
    }
}

void ExDockWidget::setTitle(const QString &title)
{
    if (m_title == title) return;
    m_title = title;
    m_toggleAction->setText(title);
    emit titleChanged(title);
}

void ExDockWidget::setState(DockState s)
{
    if (m_state == s) return;
    m_state = s;
    // Block toggleAction signals to prevent reentrant show/hide calls.
    // The stateChanged signal below is what DockManager and UI sync on.
    {
        QSignalBlocker blocker(m_toggleAction);
        m_toggleAction->setChecked(s != DockState::Closed);
    }
    emit stateChanged(s);
}

void ExDockWidget::closeDock()
{
    setState(DockState::Closed);
    hide();
    emit closed();
}

} // namespace exdock
