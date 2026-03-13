#include "dockautohidemanager.h"

#include <QApplication>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QToolButton>

// ── AutoHideTab implementation ────────────────────────────────────────────

AutoHideTab::AutoHideTab(const QString &text, Qt::DockWidgetArea area,
                         QWidget *parent)
    : QToolButton(parent), m_area(area)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);

    const bool vertical = (area == Qt::LeftDockWidgetArea
                        || area == Qt::RightDockWidgetArea);
    if (vertical)
        setFixedWidth(22);
    else
        setFixedHeight(22);

    setStyleSheet(
        QStringLiteral(
            "QToolButton { background:#252526; color:#858585; border:none;"
            " padding:6px 2px; font-size:11px; }"
            "QToolButton:hover { color:#e0e0e0; background:#2a2d2e; }"
            "QToolButton:checked { color:#e0e0e0; background:#37373d; }"));
}

QSize AutoHideTab::sizeHint() const
{
    const QFontMetrics fm(font());
    const int textLen = fm.horizontalAdvance(text()) + 16;
    const bool vertical = (m_area == Qt::LeftDockWidgetArea
                        || m_area == Qt::RightDockWidgetArea);
    if (vertical)
        return {22, textLen};
    return {textLen, 22};
}

void AutoHideTab::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const bool checked = opt.state & QStyle::State_On;

    if (checked)
        p.fillRect(rect(), QColor(0x37, 0x37, 0x3d));
    else if (hovered)
        p.fillRect(rect(), QColor(0x2a, 0x2d, 0x2e));
    else
        p.fillRect(rect(), QColor(0x25, 0x25, 0x26));

    if (checked) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x00, 0x7a, 0xcc));
        if (m_area == Qt::LeftDockWidgetArea)
            p.drawRect(0, 0, 2, height());
        else if (m_area == Qt::RightDockWidgetArea)
            p.drawRect(width() - 2, 0, 2, height());
        else
            p.drawRect(0, 0, width(), 2);
    }

    p.setPen(checked || hovered ? QColor(0xe0, 0xe0, 0xe0)
                                : QColor(0x85, 0x85, 0x85));
    p.setFont(font());

    const bool vertical = (m_area == Qt::LeftDockWidgetArea
                        || m_area == Qt::RightDockWidgetArea);
    if (vertical) {
        p.save();
        if (m_area == Qt::LeftDockWidgetArea) {
            p.translate(0, height());
            p.rotate(-90);
        } else {
            p.translate(width(), 0);
            p.rotate(90);
        }
        p.drawText(QRect(4, 0, height() - 8, width()),
                   Qt::AlignCenter, text());
        p.restore();
    } else {
        p.drawText(rect(), Qt::AlignCenter, text());
    }
}

// ── AutoHideSideBar implementation ────────────────────────────────────────

AutoHideSideBar::AutoHideSideBar(Qt::DockWidgetArea area, QWidget *parent)
    : QWidget(parent), m_area(area)
{
    const bool vertical = (area == Qt::LeftDockWidgetArea
                        || area == Qt::RightDockWidgetArea);
    if (vertical) {
        m_layout = new QVBoxLayout(this);
        setFixedWidth(22);
    } else {
        m_layout = new QHBoxLayout(this);
        setFixedHeight(22);
    }
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(1);
    m_layout->addStretch();

    setStyleSheet(QStringLiteral("background:#252526;"));
    hide();
}

AutoHideTab *AutoHideSideBar::addTab(const QString &title)
{
    auto *tab = new AutoHideTab(title, m_area, this);
    m_layout->insertWidget(m_layout->count() - 1, tab);
    m_tabs.append(tab);
    show();
    return tab;
}

void AutoHideSideBar::removeTab(AutoHideTab *tab)
{
    m_tabs.removeAll(tab);
    m_layout->removeWidget(tab);
    tab->deleteLater();
    if (m_tabs.isEmpty())
        hide();
}

// ── Custom title-bar widget with pin/unpin toggle ─────────────────────────

class DockTitleBar : public QWidget
{
    Q_OBJECT

public:
    DockTitleBar(QDockWidget *dock, DockAutoHideManager *manager)
        : QWidget(dock), m_dock(dock), m_manager(manager)
    {
        auto *lay = new QHBoxLayout(this);
        lay->setContentsMargins(6, 2, 2, 2);
        lay->setSpacing(2);

        m_titleLabel = new QLabel(dock->windowTitle(), this);
        m_titleLabel->setStyleSheet(QStringLiteral(
            "color:#cccccc; font-size:11px; font-weight:bold;"));
        lay->addWidget(m_titleLabel, 1);

        // Pin / unpin button (📌)
        m_pinBtn = new QToolButton(this);
        m_pinBtn->setCheckable(true);
        m_pinBtn->setChecked(true); // pinned by default
        m_pinBtn->setFixedSize(20, 20);
        m_pinBtn->setToolTip(tr("Pin / Unpin panel"));
        updatePinIcon();
        connect(m_pinBtn, &QToolButton::toggled, this, [this](bool pinned) {
            updatePinIcon();
            if (pinned)
                m_manager->pinDock(m_dock);
            else
                m_manager->unpinDock(m_dock);
        });
        lay->addWidget(m_pinBtn);

        // Close button
        auto *closeBtn = new QToolButton(this);
        closeBtn->setFixedSize(20, 20);
        closeBtn->setText(QStringLiteral("\u00D7")); // ×
        closeBtn->setStyleSheet(QStringLiteral(
            "QToolButton{color:#999;border:none;font-size:14px;}"
            "QToolButton:hover{color:#e0e0e0;background:#c42b1c;border-radius:2px;}"));
        connect(closeBtn, &QToolButton::clicked, m_dock, &QDockWidget::close);
        lay->addWidget(closeBtn);

        setStyleSheet(QStringLiteral("background:#252526;"));
        setFixedHeight(24);

        connect(dock, &QDockWidget::windowTitleChanged,
                m_titleLabel, &QLabel::setText);
    }

    void setPinned(bool pinned)
    {
        m_pinBtn->blockSignals(true);
        m_pinBtn->setChecked(pinned);
        updatePinIcon();
        m_pinBtn->blockSignals(false);
    }

    bool isPinned() const { return m_pinBtn->isChecked(); }

private:
    void updatePinIcon()
    {
        // Pinned: vertical pushpin (📌), Unpinned: horizontal pushpin (rotated)
        if (m_pinBtn->isChecked()) {
            m_pinBtn->setText(QStringLiteral("\U0001F4CC")); // 📌
            m_pinBtn->setStyleSheet(QStringLiteral(
                "QToolButton{color:#cccccc;border:none;font-size:12px;}"
                "QToolButton:hover{background:#3e3e42;border-radius:2px;}"));
        } else {
            m_pinBtn->setText(QStringLiteral("\U0001F4CC")); // 📌 (rotated via transform)
            m_pinBtn->setStyleSheet(QStringLiteral(
                "QToolButton{color:#858585;border:none;font-size:12px;"
                "/* unpinned indicator */}"
                "QToolButton:hover{background:#3e3e42;border-radius:2px;}"));
        }
    }

    QDockWidget          *m_dock;
    DockAutoHideManager  *m_manager;
    QLabel               *m_titleLabel;
    QToolButton          *m_pinBtn;
};

// ── DockAutoHideManager implementation ────────────────────────────────────

DockAutoHideManager::DockAutoHideManager(QMainWindow *mainWindow)
    : QObject(mainWindow)
    , m_mainWindow(mainWindow)
{
    m_leftBar   = new AutoHideSideBar(Qt::LeftDockWidgetArea, mainWindow);
    m_rightBar  = new AutoHideSideBar(Qt::RightDockWidgetArea, mainWindow);
    m_bottomBar = new AutoHideSideBar(Qt::BottomDockWidgetArea, mainWindow);

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(350);
    connect(&m_hideTimer, &QTimer::timeout, this, &DockAutoHideManager::hideOverlay);

    // Install event filter on main window for global focus tracking
    mainWindow->installEventFilter(this);
    qApp->installEventFilter(this);
}

void DockAutoHideManager::registerDock(QDockWidget *dock)
{
    if (!dock) return;

    // Install a custom title bar with pin/unpin button
    auto *titleBar = new DockTitleBar(dock, this);
    dock->setTitleBarWidget(titleBar);
}

void DockAutoHideManager::unpinDock(QDockWidget *dock)
{
    if (!dock || m_entries.contains(dock))
        return;

    // Determine which area this dock belongs to
    const Qt::DockWidgetArea area = dockArea(dock);

    AutoHideSideBar *bar = sidebar(area);
    if (!bar) return;

    // Create tab on sidebar
    AutoHideTab *tab = bar->addTab(dock->windowTitle());

    AutoHideEntry entry;
    entry.dock = dock;
    entry.tab  = tab;
    entry.area = area;
    m_entries.insert(dock, entry);

    // Hide the dock from normal layout
    dock->hide();

    // Reposition sidebars so the new tab is visible
    repositionSidebars();

    // Tab click → show overlay
    connect(tab, &QToolButton::clicked, this, [this, dock](bool checked) {
        if (checked) {
            // Uncheck all other tabs first
            for (auto &e : m_entries) {
                if (e.dock != dock && e.tab)
                    e.tab->setChecked(false);
            }
            showOverlay(dock);
        } else {
            hideOverlay();
        }
    });

    // Track title changes
    connect(dock, &QDockWidget::windowTitleChanged, tab, &QToolButton::setText);
}

void DockAutoHideManager::pinDock(QDockWidget *dock)
{
    if (!dock || !m_entries.contains(dock))
        return;

    const AutoHideEntry &entry = m_entries.value(dock);

    // Remove the tab from sidebar
    AutoHideSideBar *bar = sidebar(entry.area);
    if (bar)
        bar->removeTab(entry.tab);

    // If this is the active overlay, clear it
    if (m_activeOverlay == dock) {
        m_activeOverlay = nullptr;
        m_hideTimer.stop();
    }

    m_entries.remove(dock);

    // Restore dock as a floating/docked panel
    dock->setFloating(false);
    m_mainWindow->addDockWidget(entry.area, dock);
    dock->show();

    // Reposition sidebars (may need to hide if empty)
    repositionSidebars();
}

bool DockAutoHideManager::isAutoHidden(QDockWidget *dock) const
{
    return m_entries.contains(dock);
}

void DockAutoHideManager::saveState()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    QStringList autoHidden;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        const QString name = it.key()->objectName();
        if (!name.isEmpty())
            autoHidden.append(name);
    }
    s.setValue(QStringLiteral("autoHiddenDocks"), autoHidden);
}

void DockAutoHideManager::restoreState()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    const QStringList autoHidden =
        s.value(QStringLiteral("autoHiddenDocks")).toStringList();
    if (autoHidden.isEmpty())
        return;

    // Find docks by objectName and unpin them
    const auto docks = m_mainWindow->findChildren<QDockWidget *>();
    for (QDockWidget *dock : docks) {
        if (autoHidden.contains(dock->objectName())) {
            // Set the title bar pin state to unpinned
            if (auto *tb = qobject_cast<DockTitleBar *>(dock->titleBarWidget()))
                tb->setPinned(false);
            unpinDock(dock);
        }
    }
}

AutoHideSideBar *DockAutoHideManager::sidebar(Qt::DockWidgetArea area) const
{
    switch (area) {
    case Qt::LeftDockWidgetArea:   return m_leftBar;
    case Qt::RightDockWidgetArea:  return m_rightBar;
    case Qt::BottomDockWidgetArea: return m_bottomBar;
    default:                       return m_leftBar; // fallback
    }
}

bool DockAutoHideManager::eventFilter(QObject *watched, QEvent *event)
{
    // Reposition sidebars when main window is resized
    if (watched == m_mainWindow && event->type() == QEvent::Resize) {
        repositionSidebars();
    }

    // When an overlay is active, check if focus/mouse leaves it
    if (m_activeOverlay && event->type() == QEvent::MouseButtonPress) {
        auto *w = qobject_cast<QWidget *>(watched);
        if (w) {
            // Check if click is inside the overlay dock or its sidebar tab
            const AutoHideEntry &entry = m_entries.value(m_activeOverlay);
            const bool insideDock = m_activeOverlay->isAncestorOf(w) ||
                                    w == m_activeOverlay;
            const bool insideTab  = entry.tab &&
                                    (entry.tab->isAncestorOf(w) || w == entry.tab);
            const bool insideBar  = sidebar(entry.area) &&
                                    (sidebar(entry.area)->isAncestorOf(w) ||
                                     w == sidebar(entry.area));

            if (!insideDock && !insideTab && !insideBar) {
                m_hideTimer.start();
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void DockAutoHideManager::showOverlay(QDockWidget *dock)
{
    if (m_activeOverlay && m_activeOverlay != dock)
        hideOverlay();

    m_hideTimer.stop();
    m_activeOverlay = dock;

    const AutoHideEntry &entry = m_entries.value(dock);

    // Position the dock as floating overlay next to the sidebar
    dock->setFloating(true);
    dock->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);

    // Calculate position using the main window's content rect
    const QWidget *cw = m_mainWindow->centralWidget();
    const QRect cwGlobal(cw->mapToGlobal(QPoint(0,0)), cw->size());
    const AutoHideSideBar *bar = sidebar(entry.area);
    const int barW = bar->isVisible() ? bar->width()  : 22;
    const int barH = bar->isVisible() ? bar->height() : 22;
    const int overlayWidth  = qMin(350, cwGlobal.width() / 3);
    const int overlayHeight = qMin(400, cwGlobal.height() / 2);

    QRect geo;
    switch (entry.area) {
    case Qt::LeftDockWidgetArea:
        geo = QRect(cwGlobal.left() - overlayWidth - barW,
                    cwGlobal.top(),
                    overlayWidth,
                    cwGlobal.height());
        // Actually position to the right of the sidebar
        geo.moveLeft(bar->mapToGlobal(QPoint(barW, 0)).x());
        break;
    case Qt::RightDockWidgetArea:
        geo = QRect(cwGlobal.right() - overlayWidth + 1,
                    cwGlobal.top(),
                    overlayWidth,
                    cwGlobal.height());
        // Position to the left of the sidebar
        geo.moveRight(bar->mapToGlobal(QPoint(0, 0)).x() - 1);
        break;
    case Qt::BottomDockWidgetArea:
        geo = QRect(cwGlobal.left(),
                    cwGlobal.bottom() - overlayHeight + 1,
                    cwGlobal.width(),
                    overlayHeight);
        // Position above the sidebar
        geo.moveBottom(bar->mapToGlobal(QPoint(0, 0)).y() - 1);
        break;
    default:
        geo = QRect(cwGlobal.left() + 22, cwGlobal.top(), overlayWidth, overlayHeight);
        break;
    }
    dock->setGeometry(geo);
    dock->show();
    dock->raise();
}

void DockAutoHideManager::hideOverlay()
{
    if (!m_activeOverlay)
        return;

    // Don't hide if mouse is still inside the overlay or its contents have focus
    if (m_activeOverlay->underMouse() ||
        m_activeOverlay->isAncestorOf(QApplication::focusWidget())) {
        m_hideTimer.start(); // retry later
        return;
    }

    m_activeOverlay->hide();
    uncheckAllTabs();
    m_activeOverlay = nullptr;
}

void DockAutoHideManager::uncheckAllTabs()
{
    for (auto &entry : m_entries) {
        if (entry.tab)
            entry.tab->setChecked(false);
    }
}

Qt::DockWidgetArea DockAutoHideManager::dockArea(QDockWidget *dock) const
{
    const Qt::DockWidgetArea area = m_mainWindow->dockWidgetArea(dock);
    if (area != Qt::NoDockWidgetArea)
        return area;

    // Fallback: infer from allowedAreas
    const Qt::DockWidgetAreas allowed = dock->allowedAreas();
    if (allowed & Qt::LeftDockWidgetArea)    return Qt::LeftDockWidgetArea;
    if (allowed & Qt::RightDockWidgetArea)   return Qt::RightDockWidgetArea;
    if (allowed & Qt::BottomDockWidgetArea)  return Qt::BottomDockWidgetArea;
    return Qt::LeftDockWidgetArea;
}

void DockAutoHideManager::repositionSidebars()
{
    // Sidebars are children of the main window.
    // Position them at the edges of the content area (between menu bar and status bar).
    const int menuH  = m_mainWindow->menuBar() ? m_mainWindow->menuBar()->height() : 0;
    const int toolH  = 0; // toolbar is at the top, accounted for in content area
    const int statusH = m_mainWindow->statusBar() ? m_mainWindow->statusBar()->height() : 0;
    const int winW   = m_mainWindow->width();
    const int winH   = m_mainWindow->height();

    // Content area starts below menubar+toolbar, ends above statusbar
    const int contentTop    = menuH + toolH;
    const int contentBottom = winH - statusH;
    const int contentHeight = contentBottom - contentTop;

    const int sideWidth  = 22;
    const int botHeight  = 22;

    // Always set geometry — even if the bar just became visible.
    // Left sidebar: full content height on left edge
    m_leftBar->setGeometry(0, contentTop, sideWidth, contentHeight);

    // Right sidebar: full content height on right edge
    m_rightBar->setGeometry(winW - sideWidth, contentTop, sideWidth, contentHeight);

    // Bottom sidebar: full width at bottom (between left and right sidebars)
    const int leftW  = m_leftBar->isVisible()  ? sideWidth : 0;
    const int rightW = m_rightBar->isVisible() ? sideWidth : 0;
    m_bottomBar->setGeometry(leftW, contentBottom - botHeight,
                             winW - leftW - rightW, botHeight);

    // Raise sidebars above dock widgets so they're always visible
    m_leftBar->raise();
    m_rightBar->raise();
    m_bottomBar->raise();
}

#include "dockautohidemanager.moc"
