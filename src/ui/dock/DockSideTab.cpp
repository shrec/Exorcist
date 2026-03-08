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

    // Background — use palette for theme integration
    QStyleOptionToolButton opt;
    initStyleOption(&opt);
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const bool checked = opt.state & QStyle::State_On;

    const QColor bgNormal  = palette().color(QPalette::Window);
    const QColor bgHover   = palette().color(QPalette::Midlight);
    const QColor bgChecked = palette().color(QPalette::Mid);

    if (checked)
        p.fillRect(rect(), bgChecked);
    else if (hovered)
        p.fillRect(rect(), bgHover);
    else
        p.fillRect(rect(), bgNormal);

    // Active indicator (accent stripe)
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

    // Text — palette-based
    const QColor textColor = (checked || hovered)
        ? palette().color(QPalette::WindowText)
        : palette().color(QPalette::PlaceholderText);
    p.setPen(textColor);
    p.setFont(font());

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
