#pragma once

#include <QHash>
#include <QObject>
#include <QWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDockWidget>
#include <QEvent>
#include <QPainter>
#include <QStyleOption>

class QMainWindow;

// ── AutoHideTab ───────────────────────────────────────────────────────────────
//
// A single tab button on the sidebar. Text is drawn vertically for left/right
// bars, horizontally for bottom bar.

class AutoHideTab : public QToolButton
{
    Q_OBJECT

public:
    explicit AutoHideTab(const QString &text, Qt::DockWidgetArea area,
                         QWidget *parent = nullptr)
        : QToolButton(parent), m_area(area)
    {
        setText(text);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);

        const bool vertical = (area == Qt::LeftDockWidgetArea
                            || area == Qt::RightDockWidgetArea);
        if (vertical) {
            // Vertical tabs: fixed width, dynamic height
            setFixedWidth(22);
        } else {
            setFixedHeight(22);
        }

        setStyleSheet(
            QStringLiteral(
                "QToolButton { background:#252526; color:#858585; border:none;"
                " padding:6px 2px; font-size:11px; }"
                "QToolButton:hover { color:#e0e0e0; background:#2a2d2e; }"
                "QToolButton:checked { color:#e0e0e0; background:#37373d; }"));
    }

    QSize sizeHint() const override
    {
        const QFontMetrics fm(font());
        const int textLen = fm.horizontalAdvance(text()) + 16;
        const bool vertical = (m_area == Qt::LeftDockWidgetArea
                            || m_area == Qt::RightDockWidgetArea);
        if (vertical)
            return {22, textLen};
        return {textLen, 22};
    }

    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Background
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

        // Active indicator
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

        // Text
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

private:
    Qt::DockWidgetArea m_area;
};

// ── AutoHideSideBar ───────────────────────────────────────────────────────────
//
// A narrow bar that hosts AutoHideTab buttons for a single edge.

class AutoHideSideBar : public QWidget
{
    Q_OBJECT

public:
    explicit AutoHideSideBar(Qt::DockWidgetArea area, QWidget *parent = nullptr)
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
        hide(); // hidden until tabs are added
    }

    AutoHideTab *addTab(const QString &title)
    {
        auto *tab = new AutoHideTab(title, m_area, this);
        // Insert before the stretch
        m_layout->insertWidget(m_layout->count() - 1, tab);
        m_tabs.append(tab);
        show();
        return tab;
    }

    void removeTab(AutoHideTab *tab)
    {
        m_tabs.removeAll(tab);
        m_layout->removeWidget(tab);
        tab->deleteLater();
        if (m_tabs.isEmpty())
            hide();
    }

    const QList<AutoHideTab *> &tabs() const { return m_tabs; }
    Qt::DockWidgetArea area() const { return m_area; }

private:
    Qt::DockWidgetArea   m_area;
    QBoxLayout          *m_layout;
    QList<AutoHideTab *> m_tabs;
};

// ── DockAutoHideManager ───────────────────────────────────────────────────────
//
// Coordinates auto-hide behavior for all dock widgets in a QMainWindow.
// When a dock is "unpinned":
//   - It is hidden from the normal dock layout
//   - A tab appears on the corresponding sidebar
//   - Clicking the tab shows the dock as a floating overlay
//   - Moving focus away auto-hides the dock again
//
// Persistence: saves auto-hidden dock names to QSettings.

class DockAutoHideManager : public QObject
{
    Q_OBJECT

public:
    explicit DockAutoHideManager(QMainWindow *mainWindow);

    /// Register a dock widget for auto-hide support.
    /// Call this for every dock that should have a pin/unpin button.
    void registerDock(QDockWidget *dock);

    /// Unpin a dock (collapse to sidebar tab).
    void unpinDock(QDockWidget *dock);

    /// Pin a dock (restore to normal dock layout).
    void pinDock(QDockWidget *dock);

    /// Is this dock currently auto-hidden?
    bool isAutoHidden(QDockWidget *dock) const;

    /// Save/restore auto-hide state to QSettings.
    void saveState();
    void restoreState();

    /// Access sidebars (for layout integration).
    AutoHideSideBar *sidebar(Qt::DockWidgetArea area) const;

    /// Reposition sidebars to match main window geometry.
    void repositionSidebars();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showOverlay(QDockWidget *dock);
    void hideOverlay();
    void uncheckAllTabs();
    Qt::DockWidgetArea dockArea(QDockWidget *dock) const;

    struct AutoHideEntry {
        QDockWidget     *dock  = nullptr;
        AutoHideTab     *tab   = nullptr;
        Qt::DockWidgetArea area = Qt::LeftDockWidgetArea;
    };

    QMainWindow      *m_mainWindow;
    AutoHideSideBar  *m_leftBar;
    AutoHideSideBar  *m_rightBar;
    AutoHideSideBar  *m_bottomBar;

    QHash<QDockWidget *, AutoHideEntry> m_entries;
    QDockWidget      *m_activeOverlay = nullptr;
    QTimer            m_hideTimer;
};
