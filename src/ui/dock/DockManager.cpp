#include "DockManager.h"
#include "DockArea.h"
#include "DockDropOverlay.h"
#include "DockOverlayPanel.h"
#include "DockSideBar.h"
#include "DockSideTab.h"
#include "DockSplitter.h"
#include "ExDockWidget.h"

#include <QApplication>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStatusBar>
#include <QToolBar>

namespace exdock {

DockManager::DockManager(QMainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    setupSideBars();

    // Root splitter — horizontal: [Left areas | Center splitter | Right areas]
    m_rootSplitter = new DockSplitter(Qt::Horizontal, nullptr);
    m_rootSplitter->setObjectName(QStringLiteral("exdock-root-splitter"));

    // Center splitter — vertical: [Top areas | Editor | Bottom areas]
    m_centerSplitter = new DockSplitter(Qt::Vertical, nullptr);
    m_centerSplitter->setObjectName(QStringLiteral("exdock-center-splitter"));

    m_rootSplitter->insertChildWidget(0, m_centerSplitter);
    m_rootSplitter->setChildStretchFactor(0, 3);

    mainWindow->setCentralWidget(m_rootSplitter);

    m_overlay = new DockOverlayPanel(mainWindow);
    connect(m_overlay, &DockOverlayPanel::pinRequested,
            this, &DockManager::onOverlayPinRequested);
    connect(m_overlay, &DockOverlayPanel::closeRequested,
            this, &DockManager::onOverlayCloseRequested);
    connect(m_overlay, &DockOverlayPanel::overlayHidden,
            this, &DockManager::onOverlayHidden);

    // Drop overlay for drag-and-drop
    m_dropOverlay = new DockDropOverlay(m_rootSplitter);

    // Track main window resize to reposition sidebars
    mainWindow->installEventFilter(this);
}

DockManager::~DockManager() = default;

// ── Sidebar setup ────────────────────────────────────────────────────────────

void DockManager::setupSideBars()
{
    m_leftBar   = new DockSideBar(SideBarArea::Left,   m_mainWindow);
    m_rightBar  = new DockSideBar(SideBarArea::Right,  m_mainWindow);
    m_bottomBar = new DockSideBar(SideBarArea::Bottom, m_mainWindow);

    auto connectBar = [this](DockSideBar *bar) {
        connect(bar, &DockSideBar::tabClicked,
                this, &DockManager::onSideBarTabClicked);
    };
    connectBar(m_leftBar);
    connectBar(m_rightBar);
    connectBar(m_bottomBar);

    applyDockStyleSheet();
    repositionSideBars();
}

void DockManager::applyDockStyleSheet()
{
    const QPalette &pal = qApp->palette();
    const QColor window     = pal.color(QPalette::Window);
    const QColor base       = pal.color(QPalette::Base);
    const QColor text       = pal.color(QPalette::WindowText);
    const QColor mid        = pal.color(QPalette::Mid);
    const QColor button     = pal.color(QPalette::Button);
    const QColor highlight  = pal.color(QPalette::Highlight);
    const QColor dimText    = pal.color(QPalette::PlaceholderText);

    // Slightly lighter/darker than window for subtle contrast
    const bool isDark = window.lightnessF() < 0.5;
    const QColor titleBg    = isDark ? window.lighter(120) : window.darker(105);
    const QColor tabActiveBg = window;
    const QColor tabInactiveBg = isDark ? window.lighter(110) : window.darker(103);
    const QColor hoverBg    = isDark ? window.lighter(140) : window.darker(110);
    const QColor border     = isDark ? window.lighter(160) : window.darker(115);
    const QColor closeHover = QColor(232, 17, 35);  // VS red

    auto c = [](const QColor &col) {
        return QString("rgba(%1,%2,%3,%4)")
            .arg(col.red()).arg(col.green()).arg(col.blue()).arg(col.alpha());
    };

    const QString css = QStringLiteral(
        /* ── Dock sidebar strips ─────────────────────────────── */
        "QWidget[objectName^=\"exdock-sidebar\"] {"
        "  background: transparent;"
        "  border: none;"
        "}"

        /* ── Dock side tabs ──────────────────────────────────── */
        "QToolButton#exdock-side-tab {"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0;"
        "  margin: 0;"
        "}"

        /* ── DockArea container ──────────────────────────────── */
        "QWidget#exdock-area {"
        "  background: %1;"
        "}"

        /* ── DockTitleBar ────────────────────────────────────── */
        "QWidget#exdock-title-bar {"
        "  background: %2;"
        "  border-bottom: 1px solid %3;"
        "}"
        "QLabel#exdock-title-label {"
        "  color: %4;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  padding-left: 2px;"
        "  background: transparent;"
        "}"

        /* ── TitleBar / Overlay buttons ──────────────────────── */
        "QToolButton#exdock-pin-btn,"
        "QToolButton#exdock-close-btn,"
        "QToolButton#exdock-overlay-pin,"
        "QToolButton#exdock-overlay-close {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 2px;"
        "  color: %5;"
        "  font-size: 14px;"
        "}"
        "QToolButton#exdock-pin-btn:hover,"
        "QToolButton#exdock-overlay-pin:hover {"
        "  background: %6;"
        "}"
        "QToolButton#exdock-close-btn:hover,"
        "QToolButton#exdock-overlay-close:hover {"
        "  background: %7;"
        "  color: white;"
        "}"

        /* ── DockTabBar (QTabBar in dock areas) ─────────────── */
        "QTabBar#exdock-tabbar {"
        "  background: %2;"
        "  border: none;"
        "  qproperty-drawBase: false;"
        "}"
        "QTabBar#exdock-tabbar::tab {"
        "  background: %8;"
        "  color: %5;"
        "  padding: 5px 14px 5px 10px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  margin-right: 0px;"
        "  min-width: 50px;"
        "  max-height: 18px;"
        "}"
        "QTabBar#exdock-tabbar::tab:selected {"
        "  background: %1;"
        "  color: %4;"
        "  border-bottom: 2px solid %9;"
        "}"
        "QTabBar#exdock-tabbar::tab:hover:!selected {"
        "  background: %6;"
        "  color: %4;"
        "}"
        "QTabBar#exdock-tabbar::close-button {"
        "  image: none;"
        "  border: none;"
        "  background: transparent;"
        "  width: 0px;"
        "  height: 0px;"
        "  padding: 0px;"
        "  margin: 0px;"
        "}"
        "QTabBar#exdock-tabbar::close-button:hover {"
        "  image: none;"
        "  width: 0px;"
        "  height: 0px;"
        "}"

        /* ── Overlay panel ──────────────────────────────────── */
        "QFrame#exdock-overlay-panel {"
        "  background: %1;"
        "  border: 1px solid %3;"
        "}"
        "QWidget#exdock-overlay-titlebar {"
        "  background: %2;"
        "  border-bottom: 1px solid %3;"
        "}"
        "QLabel#exdock-overlay-title {"
        "  color: %4;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "  background: transparent;"
        "}"

        /* ── Splitter handles ───────────────────────────────── */
        "QSplitter#exdock-splitter::handle {"
        "  background: %3;"
        "}"
        "QSplitter#exdock-splitter::handle:hover {"
        "  background: %9;"
        "}"
    ).arg(
        c(window),       // %1 — main background
        c(titleBg),      // %2 — title bar / tab bar bg
        c(border),       // %3 — borders
        c(text),         // %4 — active text
        c(dimText),      // %5 — dim text
        c(hoverBg),      // %6 — hover bg
        c(closeHover),   // %7 — close hover (red)
        c(tabInactiveBg),// %8 — inactive tab bg
        c(highlight)     // %9 — accent / highlight
    );

    m_mainWindow->setStyleSheet(css);
    if (m_overlay)
        m_overlay->setStyleSheet(css);
}

DockSideBar *DockManager::sideBar(SideBarArea area) const
{
    switch (area) {
    case SideBarArea::Left:   return m_leftBar;
    case SideBarArea::Right:  return m_rightBar;
    case SideBarArea::Top:    return m_bottomBar;  // No top sidebar; redirect to bottom
    case SideBarArea::Bottom: return m_bottomBar;
    default:                  return m_leftBar;
    }
}

// ── Dock widget management ───────────────────────────────────────────────────

void DockManager::addDockWidget(ExDockWidget *dock, SideBarArea preferredSide,
                                 bool startPinned)
{
    if (!dock || dock->dockId().isEmpty())
        return;
    if (m_docks.contains(dock->dockId()))
        return;

    DockEntry entry;
    entry.dock = dock;
    entry.preferredSide = preferredSide;
    entry.state = DockState::Closed;
    m_docks.insert(dock->dockId(), entry);

    if (startPinned)
        pinDock(dock);
    else
        unpinDock(dock, preferredSide);
}

void DockManager::removeDockWidget(ExDockWidget *dock)
{
    if (!dock) return;
    const QString id = dock->dockId();
    if (!m_docks.contains(id))
        return;

    // Close and clean up
    closeDock(dock);
    m_docks.remove(id);
}

ExDockWidget *DockManager::dockWidget(const QString &id) const
{
    auto it = m_docks.find(id);
    if (it != m_docks.end())
        return it->dock;
    return nullptr;
}

QStringList DockManager::dockWidgetIds() const
{
    return m_docks.keys();
}

QList<ExDockWidget *> DockManager::dockWidgets() const
{
    QList<ExDockWidget *> result;
    result.reserve(m_docks.size());
    for (auto it = m_docks.cbegin(); it != m_docks.cend(); ++it)
        result.append(it->dock);
    return result;
}

// ── Pin / Unpin / Close / Show ───────────────────────────────────────────────

void DockManager::pinDock(ExDockWidget *dock)
{
    if (!dock) return;
    const QString id = dock->dockId();
    auto it = m_docks.find(id);
    if (it == m_docks.end())
        return;

    // Already docked — nothing to do
    if (it->state == DockState::Docked)
        return;

    // If currently auto-hidden, remove sidebar tab and hide overlay
    if (it->state == DockState::AutoHidden) {
        DockSideBar *bar = sideBar(it->preferredSide);
        bar->removeTab(dock);

        if (m_overlay->activeDock() == dock)
            m_overlay->forceHideOverlay();
    }

    // Add to the correct DockArea
    DockArea *area = createDockArea(it->preferredSide);

    // Set state BEFORE adding to area to prevent reentrant duplicates.
    // addDockWidget → insertDockWidget → setState → stateChanged signal
    // can re-enter showDock/pinDock; the guard checks it->state.
    it->state = DockState::Docked;
    dock->setState(DockState::Docked);

    area->addDockWidget(dock);
    dock->show();

    repositionSideBars();
    emit dockPinned(dock);
}

void DockManager::unpinDock(ExDockWidget *dock, SideBarArea side)
{
    if (!dock) return;
    const QString id = dock->dockId();
    auto it = m_docks.find(id);
    if (it == m_docks.end()) return;

    if (side == SideBarArea::None)
        side = it->preferredSide;
    it->preferredSide = side;

    // Remove from DockArea if currently docked
    if (it->state == DockState::Docked && dock->dockArea()) {
        dock->dockArea()->removeDockWidget(dock);
    }

    // Ensure dock is a child of main window (not top-level) and hidden
    dock->setParent(m_mainWindow);
    dock->hide();

    // Add a tab on the sidebar
    DockSideBar *bar = sideBar(side);
    bar->addTab(dock);

    it->state = DockState::AutoHidden;
    dock->setState(DockState::AutoHidden);

    repositionSideBars();

    emit dockUnpinned(dock);
}

void DockManager::closeDock(ExDockWidget *dock)
{
    if (!dock) return;
    const QString id = dock->dockId();
    auto it = m_docks.find(id);
    if (it == m_docks.end()) return;

    // Remove from DockArea
    if (it->state == DockState::Docked && dock->dockArea()) {
        dock->dockArea()->removeDockWidget(dock);
    }

    // Remove sidebar tab
    if (it->state == DockState::AutoHidden) {
        DockSideBar *bar = sideBar(it->preferredSide);
        bar->removeTab(dock);
        if (m_overlay->activeDock() == dock)
            m_overlay->hideOverlay();
    }

    // Ensure dock is a child of main window (not top-level) and hidden
    dock->setParent(m_mainWindow);
    dock->hide();
    it->state = DockState::Closed;
    dock->setState(DockState::Closed);

    repositionSideBars();
    emit dockClosed(dock);
}

void DockManager::showDock(ExDockWidget *dock, SideBarArea preferredSide)
{
    if (!dock) return;
    const QString id = dock->dockId();
    auto it = m_docks.find(id);
    if (it == m_docks.end()) return;

    // Already visible — nothing to do
    if (it->state == DockState::Docked)
        return;

    it->preferredSide = preferredSide;
    pinDock(dock);
    emit dockShown(dock);
}

void DockManager::toggleDock(ExDockWidget *dock)
{
    if (!dock) return;
    const QString id = dock->dockId();
    auto it = m_docks.find(id);
    if (it == m_docks.end()) return;

    if (it->state == DockState::Closed)
        showDock(dock, it->preferredSide);
    else
        closeDock(dock);
}

bool DockManager::isPinned(ExDockWidget *dock) const
{
    if (!dock) return false;
    auto it = m_docks.find(dock->dockId());
    return it != m_docks.end() && it->state == DockState::Docked;
}

bool DockManager::isAutoHidden(ExDockWidget *dock) const
{
    if (!dock) return false;
    auto it = m_docks.find(dock->dockId());
    return it != m_docks.end() && it->state == DockState::AutoHidden;
}

// ── Sidebar slots ────────────────────────────────────────────────────────────

void DockManager::onSideBarTabClicked(ExDockWidget *dock, bool checked)
{
    if (checked) {
        // Show the overlay panel for this dock
        DockSideBar *bar = sideBar(inferSide(dock));
        const QRect barGeo(bar->mapToGlobal(QPoint(0, 0)), bar->size());
        m_overlay->showForDock(dock, bar->area(), barGeo);
    } else {
        m_overlay->hideOverlay();
    }
}

void DockManager::onOverlayPinRequested(ExDockWidget *dock)
{
    m_overlay->forceHideOverlay();
    pinDock(dock);
}

void DockManager::onOverlayCloseRequested(ExDockWidget *dock)
{
    m_overlay->forceHideOverlay();
    closeDock(dock);
}

void DockManager::onOverlayHidden()
{
    // Uncheck all sidebar tabs
    m_leftBar->uncheckAll();
    m_rightBar->uncheckAll();
    m_bottomBar->uncheckAll();
}

// ── Event filter ─────────────────────────────────────────────────────────────

bool DockManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mainWindow && event->type() == QEvent::Resize) {
        repositionSideBars();
    }

    // Track mouse during drag-and-drop
    if (m_dragDock) {
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            m_dropOverlay->updateDropZone(me->globalPosition().toPoint());
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            finishDrag(me->globalPosition().toPoint());
            return true;  // consume the release
        }
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape) {
                cancelDrag();
                return true;
            }
        }
    }

    return QObject::eventFilter(watched, event);
}

// ── Drag-and-drop ────────────────────────────────────────────────────────────

void DockManager::startDrag(ExDockWidget *dock, const QPoint & /*globalPos*/)
{
    if (m_dragDock) return;  // already dragging
    m_dragDock = dock;

    // Install event filter on the application to capture ALL mouse events
    qApp->installEventFilter(this);

    // Show drop overlay
    m_dropOverlay->showOverlay();

    // Change cursor to indicate drag
    QApplication::setOverrideCursor(Qt::DragMoveCursor);
}

void DockManager::finishDrag(const QPoint &globalPos)
{
    if (!m_dragDock) return;

    const SideBarArea dropZone = m_dropOverlay->updateDropZone(globalPos);

    m_dropOverlay->hideOverlay();
    QApplication::restoreOverrideCursor();
    qApp->removeEventFilter(this);
    // Re-install on mainWindow for resize tracking
    m_mainWindow->installEventFilter(this);

    ExDockWidget *dock = m_dragDock;
    m_dragDock = nullptr;

    if (dropZone == SideBarArea::None)
        return;  // dropped outside valid zone — no change

    // Move the dock widget to the new side
    // First: find current state
    auto it = m_docks.find(dock->dockId());
    if (it == m_docks.end()) return;

    const SideBarArea oldSide = it->preferredSide;
    if (dropZone == oldSide && it->state == DockState::Docked)
        return;  // same side, no-op

    // Remove from current location
    if (it->state == DockState::Docked) {
        auto *area = dock->dockArea();
        if (area)
            area->removeDockWidget(dock);
    } else if (it->state == DockState::AutoHidden) {
        // Remove sidebar tab
        DockSideBar *bar = sideBar(oldSide);
        if (bar) bar->removeTab(dock);
        if (m_overlay && m_overlay->activeDock() == dock)
            m_overlay->forceHideOverlay();
    }

    // Update preferred side
    it->preferredSide = dropZone;

    // Pin to new side
    auto *newArea = createDockArea(dropZone);
    newArea->addDockWidget(dock);
    it->state = DockState::Docked;

    emit dockPinned(dock);
}

void DockManager::cancelDrag()
{
    if (!m_dragDock) return;

    m_dropOverlay->hideOverlay();
    QApplication::restoreOverrideCursor();
    qApp->removeEventFilter(this);
    m_mainWindow->installEventFilter(this);
    m_dragDock = nullptr;
}

// ── Layout helpers ───────────────────────────────────────────────────────────

void DockManager::repositionSideBars()
{
    // Calculate content area (below menu bar and toolbars, above status bar)
    int contentTop = 0;
    if (m_mainWindow->menuBar() && m_mainWindow->menuBar()->isVisible())
        contentTop += m_mainWindow->menuBar()->height();

    // Account for all toolbars at the top
    for (auto *tb : m_mainWindow->findChildren<QToolBar *>()) {
        if (m_mainWindow->toolBarArea(tb) == Qt::TopToolBarArea && tb->isVisible())
            contentTop = qMax(contentTop,
                              tb->mapTo(m_mainWindow, QPoint(0, 0)).y() + tb->height());
    }

    const int statusH = (m_mainWindow->statusBar() && m_mainWindow->statusBar()->isVisible())
                        ? m_mainWindow->statusBar()->height() : 0;
    const int winW = m_mainWindow->width();
    const int winH = m_mainWindow->height();

    const int contentBottom = winH - statusH;
    const int contentHeight = contentBottom - contentTop;

    const int sideW = 22;
    const int barH  = 22;

    // Left sidebar — full content height
    m_leftBar->setGeometry(0, contentTop, sideW, contentHeight);

    // Right sidebar — full content height
    m_rightBar->setGeometry(winW - sideW, contentTop, sideW, contentHeight);

    // Bottom sidebar — between left and right sidebars
    const int leftW  = m_leftBar->isVisible()  ? sideW : 0;
    const int rightW = m_rightBar->isVisible() ? sideW : 0;
    m_bottomBar->setGeometry(leftW, contentBottom - barH,
                             winW - leftW - rightW, barH);

    m_leftBar->raise();
    m_rightBar->raise();
    m_bottomBar->raise();

    // Update root splitter margins to leave room for visible sidebars.
    // This prevents the dock areas and editor from being hidden behind
    // the sidebar strips.
    if (m_rootSplitter) {
        const int ml = m_leftBar->isVisible()   ? sideW : 0;
        const int mr = m_rightBar->isVisible()  ? sideW : 0;
        const int mb = m_bottomBar->isVisible() ? barH  : 0;
        m_rootSplitter->setContentsMargins(ml, 0, mr, mb);
    }
}

SideBarArea DockManager::inferSide(ExDockWidget *dock) const
{
    auto it = m_docks.find(dock->dockId());
    if (it != m_docks.end())
        return it->preferredSide;
    return SideBarArea::Left;
}

DockArea *DockManager::createDockArea(SideBarArea side)
{
    // Reuse existing area for this side if it exists
    auto it = m_sideAreas.find(side);
    if (it != m_sideAreas.end())
        return it.value();

    auto *area = new DockArea(this, nullptr);
    m_sideAreas.insert(side, area);

    // Connect area signals
    connect(area, &DockArea::pinRequested, this, [this, area]() {
        auto *currentDock = area->currentDockWidget();
        if (currentDock)
            unpinDock(currentDock);
    });
    connect(area, &DockArea::closeRequested, this, [this, area]() {
        auto *currentDock = area->currentDockWidget();
        if (currentDock)
            closeDock(currentDock);
    });

    // Clean up empty areas — when the last tab is removed/closed/unpinned,
    // remove the area from the splitter tree and free it.
    connect(area, &DockArea::areaEmpty, this, [this, side](DockArea *emptyArea) {
        m_sideAreas.remove(side);

        // Remove from whichever splitter owns it
        auto *parentSplitter = qobject_cast<DockSplitter *>(emptyArea->parentWidget());
        if (parentSplitter)
            parentSplitter->removeChild(emptyArea);

        emptyArea->deleteLater();
    });

    // Drag-and-drop: connect drag signals for dock relocation
    connect(area, &DockArea::titleDragStarted,
            this, [this, area](const QPoint &globalPos) {
        auto *dock = area->currentDockWidget();
        if (dock)
            startDrag(dock, globalPos);
    });
    connect(area, &DockArea::tabDragStarted,
            this, [this](ExDockWidget *dock, const QPoint &globalPos) {
        if (dock)
            startDrag(dock, globalPos);
    });

    // VS-style layout:
    //   m_rootSplitter (horizontal): [Left | m_centerSplitter | Right]
    //   m_centerSplitter (vertical): [Top | Editor | Bottom]
    switch (side) {
    case SideBarArea::Left: {
        // Insert before the center splitter in the root
        int centerIdx = m_rootSplitter->indexOf(m_centerSplitter);
        if (centerIdx < 0) centerIdx = 0;
        m_rootSplitter->insertChildWidget(centerIdx, area);
        m_rootSplitter->setChildStretchFactor(
            m_rootSplitter->indexOf(area), 1);
        m_rootSplitter->setChildMinimumSize(
            m_rootSplitter->indexOf(area), 150);
        break;
    }
    case SideBarArea::Right: {
        // Insert after the center splitter in the root
        int centerIdx = m_rootSplitter->indexOf(m_centerSplitter);
        if (centerIdx < 0) centerIdx = m_rootSplitter->count() - 1;
        m_rootSplitter->insertChildWidget(centerIdx + 1, area);
        m_rootSplitter->setChildStretchFactor(
            m_rootSplitter->indexOf(area), 1);
        m_rootSplitter->setChildMinimumSize(
            m_rootSplitter->indexOf(area), 150);
        break;
    }
    case SideBarArea::Top: {
        // Insert before the central widget in the center splitter
        int editorIdx = m_centralWidget
                        ? m_centerSplitter->indexOf(m_centralWidget) : 0;
        if (editorIdx < 0) editorIdx = 0;
        m_centerSplitter->insertChildWidget(editorIdx, area);
        m_centerSplitter->setChildStretchFactor(
            m_centerSplitter->indexOf(area), 1);
        m_centerSplitter->setChildMinimumSize(
            m_centerSplitter->indexOf(area), 100);
        break;
    }
    case SideBarArea::Bottom:
    case SideBarArea::None: {
        // Insert after the central widget in the center splitter
        int editorIdx = m_centralWidget
                        ? m_centerSplitter->indexOf(m_centralWidget)
                        : m_centerSplitter->count() - 1;
        if (editorIdx < 0) editorIdx = m_centerSplitter->count() - 1;
        m_centerSplitter->insertChildWidget(editorIdx + 1, area);
        m_centerSplitter->setChildStretchFactor(
            m_centerSplitter->indexOf(area), 1);
        m_centerSplitter->setChildMinimumSize(
            m_centerSplitter->indexOf(area), 100);
        break;
    }
    }

    m_rootSplitter->distributeByStretch();
    if (side == SideBarArea::Top || side == SideBarArea::Bottom
        || side == SideBarArea::None) {
        m_centerSplitter->distributeByStretch();
    }

    return area;
}

// ── Central content ──────────────────────────────────────────────────────────

void DockManager::setCentralContent(QWidget *widget)
{
    if (m_centralWidget) {
        m_centerSplitter->removeChild(m_centralWidget);
    }
    m_centralWidget = widget;
    if (!widget)
        return;

    // Insert the editor in the center of the vertical splitter.
    // Any Top areas should already be at index 0..n, so we append.
    m_centerSplitter->insertChildWidget(m_centerSplitter->count(), widget);
    int idx = m_centerSplitter->indexOf(widget);
    m_centerSplitter->setChildStretchFactor(idx, 5);   // editor gets most space
    m_centerSplitter->setChildMinimumSize(idx, 200);
    m_centerSplitter->distributeByStretch();
}

// ── State save/restore ───────────────────────────────────────────────────────

QJsonObject DockManager::saveState() const
{
    QJsonObject root;
    QJsonArray docksArray;

    for (auto it = m_docks.cbegin(); it != m_docks.cend(); ++it) {
        const DockEntry &entry = it.value();
        QJsonObject obj;
        obj[QStringLiteral("id")]    = entry.dock->dockId();
        obj[QStringLiteral("state")] = static_cast<int>(entry.state);
        obj[QStringLiteral("side")]  = static_cast<int>(entry.preferredSide);
        docksArray.append(obj);
    }

    root[QStringLiteral("docks")] = docksArray;

    // Persist splitter ratios
    if (m_rootSplitter && m_rootSplitter->count() > 0) {
        QJsonArray ratioArr;
        for (double r : m_rootSplitter->sizeRatios())
            ratioArr.append(r);
        root[QStringLiteral("splitterRatios")] = ratioArr;
    }

    if (m_centerSplitter && m_centerSplitter->count() > 0) {
        QJsonArray centerArr;
        for (double r : m_centerSplitter->sizeRatios())
            centerArr.append(r);
        root[QStringLiteral("centerSplitterRatios")] = centerArr;
    }

    return root;
}

void DockManager::restoreState(const QJsonObject &state)
{
    const auto docksArray = state.value(QStringLiteral("docks")).toArray();

    for (const auto &val : docksArray) {
        const auto obj = val.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        const auto dockState = static_cast<DockState>(
            obj.value(QStringLiteral("state")).toInt(0));
        const auto side = static_cast<SideBarArea>(
            obj.value(QStringLiteral("side")).toInt(0));

        auto it = m_docks.find(id);
        if (it == m_docks.end())
            continue;

        it->preferredSide = side;

        switch (dockState) {
        case DockState::Docked:
            pinDock(it->dock);
            break;
        case DockState::AutoHidden:
            unpinDock(it->dock, side);
            break;
        case DockState::Closed:
            closeDock(it->dock);
            break;
        case DockState::Floating:
            // TODO: Floating support in a future phase
            pinDock(it->dock);
            break;
        }
    }

    // Restore splitter ratios
    const auto ratioArr = state.value(QStringLiteral("splitterRatios")).toArray();
    if (!ratioArr.isEmpty() && m_rootSplitter) {
        QList<double> ratios;
        for (const auto &v : ratioArr)
            ratios.append(v.toDouble());
        m_rootSplitter->restoreFromRatios(ratios);
    }

    const auto centerArr = state.value(QStringLiteral("centerSplitterRatios")).toArray();
    if (!centerArr.isEmpty() && m_centerSplitter) {
        QList<double> ratios;
        for (const auto &v : centerArr)
            ratios.append(v.toDouble());
        m_centerSplitter->restoreFromRatios(ratios);
    }

    repositionSideBars();
}

} // namespace exdock
