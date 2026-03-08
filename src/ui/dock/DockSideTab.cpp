#include "DockSideTab.h"
#include "ExDockWidget.h"

#include <QPainter>
#include <QStyleOption>

namespace exdock {

DockSideTab::DockSideTab(ExDockWidget *dock, SideBarArea area,
                         QWidget *parent)
    : QToolButton(parent)
    , m_dock(dock)
    , m_area(area)
{
    setText(dock->title());
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setObjectName(QStringLiteral("exdock-side-tab"));
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_Hover, true);  // reliable hover paint updates
    setMouseTracking(true);

    const bool vertical = (area == SideBarArea::Left || area == SideBarArea::Right);
    if (vertical)
        setFixedWidth(34);
    else
        setFixedHeight(34);

    // Track title changes
    connect(dock, &ExDockWidget::titleChanged,
            this, &QToolButton::setText);
}

QSize DockSideTab::sizeHint() const
{
    const QFontMetrics fm(font());
    const int textLen = fm.horizontalAdvance(text()) + 28;
    const bool vertical = (m_area == SideBarArea::Left ||
                           m_area == SideBarArea::Right);
    if (vertical)
        return {34, textLen};
    return {textLen, 34};
}

void DockSideTab::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // VS-style colors from palette
    const bool hovered = underMouse();
    const bool checked = isChecked();

    const QColor window = palette().color(QPalette::Window);
    const bool isDark = window.lightnessF() < 0.5;

    // Background: transparent normally, prominent highlight on hover/active
    if (checked) {
        p.fillRect(rect(), isDark ? window.lighter(140) : window.darker(112));
    } else if (hovered) {
        // Obvious hover highlight — user must see this is interactive
        p.fillRect(rect(), isDark ? window.lighter(160) : window.darker(115));
    } else {
        p.fillRect(rect(), window);
    }

    // Hover accent border — thin accent stripe appears on hover to signal interactivity
    if (hovered && !checked) {
        const QColor accent = palette().color(QPalette::Highlight);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        switch (m_area) {
        case SideBarArea::Left:
            p.drawRect(0, 0, 2, height());
            break;
        case SideBarArea::Right:
            p.drawRect(width() - 2, 0, 2, height());
            break;
        case SideBarArea::Top:
            p.drawRect(0, 0, width(), 2);
            break;
        case SideBarArea::Bottom:
            p.drawRect(0, height() - 2, width(), 2);
            break;
        default:
            break;
        }
    }

    // Active indicator — VS-style accent stripe (2px)
    if (checked) {
        const QColor accent = palette().color(QPalette::Highlight);
        p.setPen(Qt::NoPen);
        p.setBrush(accent);
        switch (m_area) {
        case SideBarArea::Left:
            p.drawRect(0, 0, 2, height());
            break;
        case SideBarArea::Right:
            p.drawRect(width() - 2, 0, 2, height());
            break;
        case SideBarArea::Top:
            p.drawRect(0, 0, width(), 2);
            break;
        case SideBarArea::Bottom:
            p.drawRect(0, height() - 2, width(), 2);
            break;
        default:
            break;
        }
    }

    // Text — bright when active/hovered, clearly visible when inactive
    QColor textColor;
    if (checked || hovered) {
        textColor = palette().color(QPalette::WindowText);  // #dcdcdc
    } else {
        // Much brighter than PlaceholderText — VS uses ~#cccccc for sidebar text
        textColor = isDark ? QColor(200, 200, 200) : QColor(60, 60, 60);
    }
    p.setPen(textColor);
    QFont f = font();
    f.setPointSize(10);
    p.setFont(f);

    const bool vertical = (m_area == SideBarArea::Left ||
                           m_area == SideBarArea::Right);
    if (vertical) {
        p.save();
        if (m_area == SideBarArea::Left) {
            p.translate(0, height());
            p.rotate(-90);
        } else {
            p.translate(width(), 0);
            p.rotate(90);
        }
        // In rotated coords: x = along text direction, y = across tab width
        // Add horizontal padding (6px each side) so text doesn't sit on the edge
        p.drawText(QRect(10, 6, height() - 20, width() - 12),
                   Qt::AlignCenter, text());
        p.restore();
    } else {
        p.drawText(rect().adjusted(8, 0, -8, 0), Qt::AlignCenter, text());
    }
}

void DockSideTab::enterEvent(QEnterEvent *event)
{
    QToolButton::enterEvent(event);
    update();  // force repaint for hover effect
}

void DockSideTab::leaveEvent(QEvent *event)
{
    QToolButton::leaveEvent(event);
    update();  // force repaint to clear hover effect
}

} // namespace exdock
