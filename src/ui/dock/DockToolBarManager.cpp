#include "DockToolBarManager.h"
#include "DockToolBar.h"
#include "DockToolBarArea.h"

#include <QMainWindow>
#include <QMenu>
#include <QJsonArray>
#include <QJsonObject>
#include <QApplication>
#include <QEvent>
#include <QTimer>

namespace exdock {

// ── Helpers ───────────────────────────────────────────────────────────────────

static Qt::ToolBarArea toQtArea(ToolBarEdge edge)
{
    switch (edge) {
    case ToolBarEdge::Top:    return Qt::TopToolBarArea;
    case ToolBarEdge::Bottom: return Qt::BottomToolBarArea;
    case ToolBarEdge::Left:   return Qt::LeftToolBarArea;
    case ToolBarEdge::Right:  return Qt::RightToolBarArea;
    }
    return Qt::TopToolBarArea;
}

static ToolBarEdge fromQtArea(Qt::ToolBarArea area)
{
    switch (area) {
    case Qt::TopToolBarArea:    return ToolBarEdge::Top;
    case Qt::BottomToolBarArea: return ToolBarEdge::Bottom;
    case Qt::LeftToolBarArea:   return ToolBarEdge::Left;
    case Qt::RightToolBarArea:  return ToolBarEdge::Right;
    default:                    return ToolBarEdge::Top;
    }
}

// ── Construction ──────────────────────────────────────────────────────────────

DockToolBarManager::DockToolBarManager(QMainWindow *mainWindow, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
    // DockToolBarArea instances are kept as metadata holders and for context-
    // menu generation; actual toolbar placement uses QMainWindow::addToolBar().
    m_areas[ToolBarEdge::Top]    = new DockToolBarArea(ToolBarEdge::Top,    m_mainWindow);
    m_areas[ToolBarEdge::Bottom] = new DockToolBarArea(ToolBarEdge::Bottom, m_mainWindow);
    m_areas[ToolBarEdge::Left]   = new DockToolBarArea(ToolBarEdge::Left,   m_mainWindow);
    m_areas[ToolBarEdge::Right]  = new DockToolBarArea(ToolBarEdge::Right,  m_mainWindow);
}

// repositionAreas() is a no-op with native placement — Qt handles geometry.
void DockToolBarManager::installAreas()  {}
void DockToolBarManager::repositionAreas() {}

// ── Toolbar lifecycle ─────────────────────────────────────────────────────────

DockToolBar *DockToolBarManager::createToolBar(const QString &id,
                                                const QString &title,
                                                ToolBarEdge edge,
                                                int band, int /*position*/)
{
    if (m_toolBars.contains(id))
        return nullptr;

    auto *bar = new DockToolBar(id, title, m_mainWindow);
    m_toolBars.insert(id, bar);

    connect(bar, &DockToolBar::dragStarted,
            this, &DockToolBarManager::onToolBarDragStarted);
    connect(bar, &DockToolBar::dropped,
            this, &DockToolBarManager::onToolBarDropped);

    bar->setLocked(m_allLocked);
    bar->setEdge(edge);
    bar->setBand(band);

    // Use Qt's native toolbar layout — this properly inserts the toolbar
    // between the menu bar and central widget, pushing content down.
    m_mainWindow->addToolBar(toQtArea(edge), bar);
    bar->show();

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

    m_mainWindow->removeToolBar(bar);
    m_toolBars.remove(id);

    emit toolBarRemoved(id);
    bar->deleteLater();
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
                                      int band, int /*position*/)
{
    auto *bar = m_toolBars.value(id, nullptr);
    if (!bar)
        return;

    const ToolBarEdge oldEdge = bar->edge();
    bar->setEdge(edge);
    bar->setBand(band);

    m_mainWindow->addToolBar(toQtArea(edge), bar);

    if (oldEdge != edge)
        emit toolBarMoved(bar, oldEdge, edge);
}

void DockToolBarManager::setToolBarVisible(const QString &id, bool visible)
{
    auto *bar = m_toolBars.value(id, nullptr);
    if (bar)
        bar->setVisible(visible);
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

    QList<DockToolBar *> bars = m_toolBars.values();
    std::sort(bars.begin(), bars.end(),
              [](const DockToolBar *a, const DockToolBar *b) {
                  return a->title() < b->title();
              });

    for (auto *bar : bars)
        menu->addAction(bar->toggleViewAction());

    if (!bars.isEmpty())
        menu->addSeparator();

    auto *lockAction = menu->addAction(tr("Lock Toolbars"));
    lockAction->setCheckable(true);
    lockAction->setChecked(m_allLocked);
    connect(lockAction, &QAction::toggled,
            const_cast<DockToolBarManager *>(this),
            &DockToolBarManager::setAllLocked);

    return menu;
}

// ── State save/restore ────────────────────────────────────────────────────────

QJsonObject DockToolBarManager::saveState() const
{
    QJsonObject root;
    QJsonArray barsArray;

    for (auto it = m_toolBars.cbegin(); it != m_toolBars.cend(); ++it) {
        const auto *bar = it.value();
        const Qt::ToolBarArea qtArea = m_mainWindow->toolBarArea(bar);
        QJsonObject barObj;
        barObj[QStringLiteral("id")]      = bar->toolBarId();
        barObj[QStringLiteral("edge")]    = static_cast<int>(fromQtArea(qtArea));
        barObj[QStringLiteral("band")]    = bar->band();
        barObj[QStringLiteral("visible")] = bar->isVisible();
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

        const auto edge    = static_cast<ToolBarEdge>(barObj.value(QStringLiteral("edge")).toInt(0));
        const int  band    = barObj.value(QStringLiteral("band")).toInt(0);
        const bool visible = barObj.value(QStringLiteral("visible")).toBool(true);

        moveToolBar(id, edge, band);
        bar->setVisible(visible);
        bar->setLocked(m_allLocked);
    }
}

// ── Event filter (no-op with native placement) ────────────────────────────────

bool DockToolBarManager::eventFilter(QObject *watched, QEvent *event)
{
    return QObject::eventFilter(watched, event);
}

// ── Drag handlers ─────────────────────────────────────────────────────────────

void DockToolBarManager::onToolBarDragStarted(DockToolBar *bar,
                                               const QPoint &globalPos)
{
    Q_UNUSED(globalPos)
    Q_UNUSED(bar)
}

void DockToolBarManager::onToolBarDropped(DockToolBar *bar)
{
    Q_UNUSED(bar)
}

DockToolBarArea *DockToolBarManager::areaAt(const QPoint &globalPos) const
{
    Q_UNUSED(globalPos)
    return nullptr;
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
