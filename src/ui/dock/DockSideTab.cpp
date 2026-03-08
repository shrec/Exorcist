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

    const bool vertical = (area == SideBarArea::Left || area == SideBarArea::Right);
    if (vertical)
        setFixedWidth(22);
    else
        setFixedHeight(22);

    // Track title changes
    connect(dock, &ExDockWidget::titleChanged,
            this, &QToolButton::setText);
}

QSize DockSideTab::sizeHint() const
{
    const QFontMetrics fm(font());
    const int textLen = fm.horizontalAdvance(text()) + 16;
    const bool vertical = (m_area == SideBarArea::Left ||
                           m_area == SideBarArea::Right);
    if (vertical)
        return {22, textLen};
    return {textLen, 22};
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

    // Background: transparent normally, subtle highlight on hover/active
    if (checked) {
        p.fillRect(rect(), isDark ? window.lighter(130) : window.darker(108));
    } else if (hovered) {
        p.fillRect(rect(), isDark ? window.lighter(140) : window.darker(110));
    } else {
        p.fillRect(rect(), window);
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

    // Text — white when active, dim when inactive
    const QColor textColor = (checked || hovered)
        ? palette().color(QPalette::WindowText)
        : palette().color(QPalette::PlaceholderText);
    p.setPen(textColor);
    QFont f = font();
    f.setPointSize(9);
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
        p.drawText(QRect(4, 0, height() - 8, width()),
                   Qt::AlignCenter, text());
        p.restore();
    } else {
        p.drawText(rect(), Qt::AlignCenter, text());
    }
}

} // namespace exdock
