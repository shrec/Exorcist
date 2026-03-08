#include "DockToolBarManager.h"
#include "DockToolBar.h"
#include "DockToolBarArea.h"

#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QApplication>
#include <QEvent>

namespace exdock {

DockToolBarManager::DockToolBarManager(QMainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    installAreas();
}

void DockToolBarManager::installAreas()
{
    // Create one DockToolBarArea per edge as child widgets of the main window.
    // They are positioned manually via repositionAreas().
    m_areas[ToolBarEdge::Top]    = new DockToolBarArea(ToolBarEdge::Top, m_mainWindow);
    m_areas[ToolBarEdge::Bottom] = new DockToolBarArea(ToolBarEdge::Bottom, m_mainWindow);
    m_areas[ToolBarEdge::Left]   = new DockToolBarArea(ToolBarEdge::Left, m_mainWindow);
    m_areas[ToolBarEdge::Right]  = new DockToolBarArea(ToolBarEdge::Right, m_mainWindow);

    // Track main window resize to reposition
    m_mainWindow->installEventFilter(this);
}

void DockToolBarManager::repositionAreas()
{
    const int menuH = m_mainWindow->menuBar()
                      ? m_mainWindow->menuBar()->height() : 0;
    const int statusH = m_mainWindow->statusBar()
                        ? m_mainWindow->statusBar()->height() : 0;
    const int winW = m_mainWindow->width();
    const int winH = m_mainWindow->height();

    const int barH = 24; // single-band toolbar height

    // Top toolbar area: full width below menu bar
    auto *topArea = m_areas[ToolBarEdge::Top];
    if (topArea->isVisible()) {
        topArea->setGeometry(0, menuH, winW, topArea->sizeHint().height());
        topArea->raise();
    }

    // Bottom toolbar area: full width above status bar
    auto *botArea = m_areas[ToolBarEdge::Bottom];
    if (botArea->isVisible()) {
        const int h = botArea->sizeHint().height();
        botArea->setGeometry(0, winH - statusH - h, winW, h);
        botArea->raise();
    }

    // Left/Right positioned between top toolbar area and bottom toolbar area
    const int topH = topArea->isVisible() ? topArea->height() : 0;
    const int botH = botArea->isVisible() ? botArea->height() : 0;
    const int contentTop = menuH + topH;
    const int contentH   = winH - contentTop - statusH - botH;

    auto *leftArea = m_areas[ToolBarEdge::Left];
    if (leftArea->isVisible()) {
        leftArea->setGeometry(0, contentTop,
                              leftArea->sizeHint().width(), contentH);
        leftArea->raise();
    }

    auto *rightArea = m_areas[ToolBarEdge::Right];
    if (rightArea->isVisible()) {
        const int w = rightArea->sizeHint().width();
        rightArea->setGeometry(winW - w, contentTop, w, contentH);
        rightArea->raise();
    }
}

DockToolBar *DockToolBarManager::createToolBar(const QString &id,
                                                const QString &title,
                                                ToolBarEdge edge,
                                                int band, int position)
{
    if (m_toolBars.contains(id))
        return nullptr;

    auto *bar = new DockToolBar(id, title);
    m_toolBars.insert(id, bar);

    connect(bar, &DockToolBar::dragStarted,
            this, &DockToolBarManager::onToolBarDragStarted);
    connect(bar, &DockToolBar::dropped,
            this, &DockToolBarManager::onToolBarDropped);

    bar->setLocked(m_allLocked);

    m_areas[edge]->addToolBar(bar, band, position);

    emit toolBarCreated(bar);
    return bar;
}

DockToolBar *DockToolBarManager::toolBar(const QString &id) const
{
    return m_toolBars.value(id, nullptr);
}

bool DockToolBarManager::removeToolBar(const QString &id)
{
    auto *bar = m_toolBars.value(id, nullptr);
    if (!bar)
        return false;

    // Remove from its current area
    for (auto *area : m_areas) {
        if (area->removeToolBar(bar))
            break;
    }

    m_toolBars.remove(id);
    emit toolBarRemoved(id);
    delete bar;
    return true;
}

QStringList DockToolBarManager::toolBarIds() const
{
    return m_toolBars.keys();
}

QList<DockToolBar *> DockToolBarManager::toolBars() const
{
    return m_toolBars.values();
}

void DockToolBarManager::moveToolBar(const QString &id, ToolBarEdge edge,
                                      int band, int position)
{
    auto *bar = m_toolBars.value(id, nullptr);
    if (!bar)
        return;

    const ToolBarEdge oldEdge = bar->edge();

    // Remove from current area
    for (auto *area : m_areas)
        area->removeToolBar(bar);

    // Add to new area
    m_areas[edge]->addToolBar(bar, band, position);

    if (oldEdge != edge)
        emit toolBarMoved(bar, oldEdge, edge);
}

void DockToolBarManager::setToolBarVisible(const QString &id, bool visible)
{
    auto *bar = m_toolBars.value(id, nullptr);
    if (!bar)
        return;

    bar->setVisible(visible);
    m_areas[bar->edge()]->updateVisibility();
}

void DockToolBarManager::setAllLocked(bool locked)
{
    m_allLocked = locked;
    for (auto *bar : m_toolBars)
        bar->setLocked(locked);
}

DockToolBarArea *DockToolBarManager::area(ToolBarEdge edge) const
{
    return m_areas.value(edge, nullptr);
}

QMenu *DockToolBarManager::createContextMenu(QWidget *parent) const
{
    auto *menu = new QMenu(tr("Toolbars"), parent);

    // Each toolbar gets a toggle action
    QList<DockToolBar *> bars = m_toolBars.values();
    std::sort(bars.begin(), bars.end(),
              [](const DockToolBar *a, const DockToolBar *b) {
                  return a->title() < b->title();
              });

    for (auto *bar : bars) {
        auto *action = bar->toggleViewAction();
        menu->addAction(action);
    }

    if (!bars.isEmpty())
        menu->addSeparator();

    // Lock all
    auto *lockAction = menu->addAction(tr("Lock Toolbars"));
    lockAction->setCheckable(true);
    lockAction->setChecked(m_allLocked);
    connect(lockAction, &QAction::toggled,
            const_cast<DockToolBarManager *>(this),
            &DockToolBarManager::setAllLocked);

    return menu;
}

// ── State save/restore ───────────────────────────────────────────────────────

QJsonObject DockToolBarManager::saveState() const
{
    QJsonObject root;
    QJsonArray barsArray;

    for (auto it = m_toolBars.cbegin(); it != m_toolBars.cend(); ++it) {
        const auto *bar = it.value();
        QJsonObject barObj;
        barObj[QStringLiteral("id")]       = bar->toolBarId();
        barObj[QStringLiteral("edge")]     = static_cast<int>(bar->edge());
        barObj[QStringLiteral("band")]     = bar->band();
        barObj[QStringLiteral("position")] = bar->bandPosition();
        barObj[QStringLiteral("visible")]  = bar->isVisible();
        barsArray.append(barObj);
    }

    root[QStringLiteral("toolbars")] = barsArray;
    root[QStringLiteral("locked")]   = m_allLocked;
    return root;
}

void DockToolBarManager::restoreState(const QJsonObject &state)
{
    m_allLocked = state.value(QStringLiteral("locked")).toBool(false);

    const auto barsArray = state.value(QStringLiteral("toolbars")).toArray();
    for (const auto &val : barsArray) {
        const auto barObj = val.toObject();
        const QString id = barObj.value(QStringLiteral("id")).toString();
        auto *bar = m_toolBars.value(id, nullptr);
        if (!bar)
            continue;

        const auto edge     = static_cast<ToolBarEdge>(barObj.value(QStringLiteral("edge")).toInt(0));
        const int  band     = barObj.value(QStringLiteral("band")).toInt(0);
        const int  position = barObj.value(QStringLiteral("position")).toInt(-1);
        const bool visible  = barObj.value(QStringLiteral("visible")).toBool(true);

        moveToolBar(id, edge, band, position);
        bar->setVisible(visible);
        bar->setLocked(m_allLocked);
    }

    for (auto *area : m_areas)
        area->updateVisibility();
}

// ── Event filter ─────────────────────────────────────────────────────────────

bool DockToolBarManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_mainWindow && event->type() == QEvent::Resize) {
        repositionAreas();
    }
    return QObject::eventFilter(watched, event);
}

// ── Drag handlers ────────────────────────────────────────────────────────────

void DockToolBarManager::onToolBarDragStarted(DockToolBar *bar,
                                               const QPoint &globalPos)
{
    Q_UNUSED(globalPos)
    Q_UNUSED(bar)
    // Future: show drop indicator overlay
}

void DockToolBarManager::onToolBarDropped(DockToolBar *bar)
{
    // Determine which area the cursor is nearest to
    const QPoint pos = QCursor::pos();
    auto *target = areaAt(pos);
    if (!target)
        return;

    const ToolBarEdge newEdge = edgeForArea(target);
    if (newEdge != bar->edge()) {
        moveToolBar(bar->toolBarId(), newEdge);
    }
}

DockToolBarArea *DockToolBarManager::areaAt(const QPoint &globalPos) const
{
    const QRect windowRect = m_mainWindow->geometry();
    const QPoint local = m_mainWindow->mapFromGlobal(globalPos);

    // Determine nearest edge by proximity
    const int distTop    = local.y();
    const int distBottom = windowRect.height() - local.y();
    const int distLeft   = local.x();
    const int distRight  = windowRect.width() - local.x();

    const int minDist = std::min({distTop, distBottom, distLeft, distRight});

    if (minDist == distTop)    return m_areas.value(ToolBarEdge::Top);
    if (minDist == distLeft)   return m_areas.value(ToolBarEdge::Left);
    if (minDist == distRight)  return m_areas.value(ToolBarEdge::Right);
    return m_areas.value(ToolBarEdge::Bottom);
}

ToolBarEdge DockToolBarManager::edgeForArea(DockToolBarArea *area) const
{
    for (auto it = m_areas.cbegin(); it != m_areas.cend(); ++it) {
        if (it.value() == area)
            return it.key();
    }
    return ToolBarEdge::Top;
}

} // namespace exdock
