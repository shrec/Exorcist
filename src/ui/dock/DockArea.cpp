#include "DockArea.h"
#include "DockManager.h"
#include "DockTabBar.h"
#include "DockTitleBar.h"
#include "ExDockWidget.h"

#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace exdock {

DockArea::DockArea(DockManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Title bar — shown when single widget
    m_titleBar = new DockTitleBar(this, this);
    lay->addWidget(m_titleBar);

    // Tab bar row — tab bar + collapse button (VS2022 style close panel ∧)
    m_tabRow = new QWidget(this);
    m_tabRow->setObjectName(QStringLiteral("exdock-tabrow"));
    auto *tabRowLayout = new QHBoxLayout(m_tabRow);
    tabRowLayout->setContentsMargins(0, 0, 0, 0);
    tabRowLayout->setSpacing(0);

    m_tabBar = new DockTabBar(this, m_tabRow);
    tabRowLayout->addWidget(m_tabBar, 1);

    m_collapseBtn = new QToolButton(m_tabRow);
    m_collapseBtn->setObjectName(QStringLiteral("exdock-collapse-btn"));
    m_collapseBtn->setText(QStringLiteral("∧"));
    m_collapseBtn->setToolTip(tr("Hide Panel"));
    m_collapseBtn->setFixedSize(24, 24);
    m_collapseBtn->setAutoRaise(true);
    m_collapseBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #858585; font-size: 10px; border: none; background: transparent; }"
        "QToolButton:hover { color: #cccccc; background: #3c3c3c; border-radius: 2px; }"));
    connect(m_collapseBtn, &QToolButton::clicked, this, &DockArea::closeAllRequested);
    tabRowLayout->addWidget(m_collapseBtn);

    lay->addWidget(m_tabRow);

    // Content stack
    m_stack = new QStackedWidget(this);
    lay->addWidget(m_stack, 1);

    setObjectName(QStringLiteral("exdock-area"));

    // Size policy: can grow and shrink with splitter handles
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMinimumSize(100, 60);

    // Tab bar signals
    connect(m_tabBar, &DockTabBar::tabCloseRequested,
            this, &DockArea::onTabCloseRequested);
    connect(m_tabBar, &DockTabBar::tabDragStarted,
            this, &DockArea::onTabDragStarted);
    connect(m_tabBar, &DockTabBar::tabContextMenu,
            this, &DockArea::onTabContextMenu);
    connect(m_tabBar, &QTabBar::currentChanged,
            this, &DockArea::onTabChanged);

    // Title bar signals
    connect(m_titleBar, &DockTitleBar::pinRequested,
            this, &DockArea::pinRequested);
    connect(m_titleBar, &DockTitleBar::closeRequested,
            this, &DockArea::closeRequested);
    connect(m_titleBar, &DockTitleBar::floatRequested,
            this, &DockArea::floatRequested);
    connect(m_titleBar, &DockTitleBar::dragStarted,
            this, &DockArea::titleDragStarted);

    updateTitleBarVisibility();
}

DockArea::~DockArea()
{
    // Detach all widgets so they aren't destroyed with us
    for (auto *w : m_widgets)
        w->setDockArea(nullptr);
}

void DockArea::addDockWidget(ExDockWidget *widget)
{
    insertDockWidget(m_widgets.size(), widget);
}

void DockArea::insertDockWidget(int index, ExDockWidget *widget)
{
    if (!widget || m_widgets.contains(widget))
        return;

    index = qBound(0, index, static_cast<int>(m_widgets.size()));

    widget->setDockArea(this);

    // Guard against reentrant insertions BEFORE any signals fire.
    // setState (below) emits stateChanged which can trigger signal chains
    // that re-enter pinDock/addDockWidget; the m_widgets.contains() guard
    // at the top only works if the widget is already in the list.
    m_widgets.insert(index, widget);

    // Let QStackedWidget own the reparenting — calling setParent(m_stack)
    // manually before insertWidget caused a show() on a widget that had not
    // yet been registered with the layout, which produced a second native
    // HWND reconstruction and corrupted Qt's widget state on Windows.
    m_stack->insertWidget(index, widget);
    m_tabBar->insertTab(index, widget->title());

    widget->setState(DockState::Docked);

    // Connect title changes
    connect(widget, &ExDockWidget::titleChanged,
            this, [this, widget](const QString &title) {
        int idx = m_widgets.indexOf(widget);
        if (idx >= 0)
            m_tabBar->setTabText(idx, title);
    });

    setCurrentIndex(index);
    updateTitleBarVisibility();
}

bool DockArea::removeDockWidget(ExDockWidget *widget)
{
    const int idx = m_widgets.indexOf(widget);
    if (idx < 0) return m_widgets.isEmpty();

    disconnect(widget, &ExDockWidget::titleChanged, this, nullptr);

    m_tabBar->removeTab(idx);
    m_stack->removeWidget(widget);
    m_widgets.removeAt(idx);

    widget->setDockArea(nullptr);
    // Reparent to main window instead of nullptr to prevent the widget
    // from becoming a top-level window (which causes taskbar entries).
    widget->setParent(m_manager->mainWindow());
    widget->hide();

    updateTitleBarVisibility();

    if (m_widgets.isEmpty()) {
        emit areaEmpty(this);
        return true;
    }
    return false;
}

int DockArea::count() const
{
    return m_widgets.size();
}

QSize DockArea::sizeHint() const
{
    // Use the current dock widget's preferred size if available
    if (auto *cw = currentDockWidget())
        return cw->sizeHint();
    return {250, 200};
}

QSize DockArea::minimumSizeHint() const
{
    if (auto *cw = currentDockWidget())
        return cw->minimumSizeHint();
    return {100, 60};
}

ExDockWidget *DockArea::dockWidget(int index) const
{
    if (index < 0 || index >= m_widgets.size())
        return nullptr;
    return m_widgets.at(index);
}

int DockArea::indexOf(ExDockWidget *widget) const
{
    return m_widgets.indexOf(widget);
}

ExDockWidget *DockArea::currentDockWidget() const
{
    const int idx = m_tabBar->currentIndex();
    return dockWidget(idx);
}

void DockArea::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_widgets.size())
        return;
    m_tabBar->setCurrentIndex(index);
    m_stack->setCurrentIndex(index);
    m_titleBar->setTitle(m_widgets.at(index)->title());
    emit currentChanged(index);
}

// ── Private slots ─────────────────────────────────────────────────────────────

void DockArea::onTabChanged(int index)
{
    if (index < 0 || index >= m_widgets.size())
        return;
    m_stack->setCurrentIndex(index);
    m_titleBar->setTitle(m_widgets.at(index)->title());
    emit currentChanged(index);
}

void DockArea::onTabCloseRequested(int index)
{
    // Switch to the requested tab so DockManager's closeRequested handler
    // picks up the correct widget (it closes currentDockWidget()).
    setCurrentIndex(index);
    emit closeRequested();
}

void DockArea::onTabDragStarted(int index, const QPoint &globalPos)
{
    auto *w = dockWidget(index);
    if (w) emit tabDragStarted(w, globalPos);
}

void DockArea::onTabContextMenu(int index, const QPoint &globalPos)
{
    auto *w = dockWidget(index);
    if (!w) return;

    // Ensure the right-clicked tab is selected
    setCurrentIndex(index);

    QMenu menu;
    menu.addAction(tr("Close"), this, [this]() { emit closeRequested(); });
    menu.addAction(tr("Auto Hide"), this, [this]() { emit pinRequested(); });
    menu.addSeparator();
    menu.addAction(tr("Close All"), this, [this]() {
        // Close all by emitting closeRequested for each widget.
        // Safety bound prevents infinite loop if a close fails to remove.
        int remaining = m_widgets.size();
        while (!m_widgets.isEmpty() && remaining-- > 0)
            emit closeRequested();
    });
    menu.exec(globalPos);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void DockArea::updateTitleBarVisibility()
{
    const bool multi = m_widgets.size() > 1;
    m_tabRow->setVisible(multi);
    m_titleBar->setVisible(!multi && !m_widgets.isEmpty());
}

} // namespace exdock
