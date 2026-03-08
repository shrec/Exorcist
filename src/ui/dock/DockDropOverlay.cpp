#include "DockDropOverlay.h"

#include <QPainter>
#include <QPainterPath>

namespace exdock {

DockDropOverlay::DockDropOverlay(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("exdock-drop-overlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowFlags(Qt::Widget);
    hide();
}

void DockDropOverlay::showOverlay()
{
    // Cover entire parent widget
    if (auto *p = parentWidget()) {
        setGeometry(p->rect());
        raise();
    }
    m_currentZone = SideBarArea::None;
    computeZones();
    show();
}

void DockDropOverlay::hideOverlay()
{
    m_currentZone = SideBarArea::None;
    hide();
}

SideBarArea DockDropOverlay::updateDropZone(const QPoint &globalPos)
{
    const QPoint local = mapFromGlobal(globalPos);

    SideBarArea zone = SideBarArea::None;
    if (m_leftZone.contains(local))
        zone = SideBarArea::Left;
    else if (m_rightZone.contains(local))
        zone = SideBarArea::Right;
    else if (m_topZone.contains(local))
        zone = SideBarArea::Top;
    else if (m_bottomZone.contains(local))
        zone = SideBarArea::Bottom;

    if (zone != m_currentZone) {
        m_currentZone = zone;
        update();
    }
    return zone;
}

void DockDropOverlay::computeZones()
{
    const int w = width();
    const int h = height();
    const int edgeW = w / 5;   // 20% width for left/right zones
    const int edgeH = h / 5;   // 20% height for top/bottom zones

    m_leftZone   = QRect(0, edgeH, edgeW, h - 2 * edgeH);
    m_rightZone  = QRect(w - edgeW, edgeH, edgeW, h - 2 * edgeH);
    m_topZone    = QRect(edgeW, 0, w - 2 * edgeW, edgeH);
    m_bottomZone = QRect(edgeW, h - edgeH, w - 2 * edgeW, edgeH);
}

void DockDropOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor accent = palette().color(QPalette::Highlight);
    const QColor zoneFill = QColor(accent.red(), accent.green(), accent.blue(), 40);
    const QColor zoneActiveFill = QColor(accent.red(), accent.green(), accent.blue(), 80);
    const QColor zoneBorder = QColor(accent.red(), accent.green(), accent.blue(), 160);

    auto drawZone = [&](const QRect &r, SideBarArea zone) {
        if (!r.isValid()) return;
        const bool active = (zone == m_currentZone);

        p.setPen(Qt::NoPen);
        p.setBrush(active ? zoneActiveFill : zoneFill);
        p.drawRect(r);

        if (active) {
            p.setPen(QPen(zoneBorder, 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r.adjusted(1, 1, -1, -1));
        }
    };

    drawZone(m_leftZone,   SideBarArea::Left);
    drawZone(m_rightZone,  SideBarArea::Right);
    drawZone(m_topZone,    SideBarArea::Top);
    drawZone(m_bottomZone, SideBarArea::Bottom);
}

} // namespace exdock
