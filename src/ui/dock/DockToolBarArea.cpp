#include "DockToolBarArea.h"
#include "DockToolBar.h"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace exdock {

DockToolBarArea::DockToolBarArea(ToolBarEdge edge, QWidget *parent)
    : QWidget(parent)
    , m_edge(edge)
{
    // Top/Bottom edges stack bands vertically (each band is a horizontal row).
    // Left/Right edges stack bands horizontally (each band is a vertical column).
    const bool vertical = (edge == ToolBarEdge::Left || edge == ToolBarEdge::Right);
    m_rootLayout = vertical
        ? static_cast<QBoxLayout *>(new QHBoxLayout(this))
        : static_cast<QBoxLayout *>(new QVBoxLayout(this));

    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    setObjectName(QStringLiteral("exdock-toolbar-area-%1")
                      .arg(static_cast<int>(edge)));

    // Start hidden — becomes visible when toolbars are added
    hide();
}

Qt::Orientation DockToolBarArea::bandOrientation() const
{
    // Bands at top/bottom have horizontal toolbars
    // Bands at left/right have vertical toolbars
    return (m_edge == ToolBarEdge::Top || m_edge == ToolBarEdge::Bottom)
           ? Qt::Horizontal : Qt::Vertical;
}

void DockToolBarArea::ensureBand(int band)
{
    while (m_bands.size() <= band) {
        ToolBarBand b;
        b.container = new QWidget(this);

        if (bandOrientation() == Qt::Horizontal) {
            b.layout = new QHBoxLayout(b.container);
        } else {
            b.layout = new QVBoxLayout(b.container);
        }
        b.layout->setContentsMargins(0, 0, 0, 0);
        b.layout->setSpacing(0);
        b.layout->addStretch(); // push toolbars to the left/top

        m_rootLayout->addWidget(b.container);
        m_bands.append(b);
    }
}

void DockToolBarArea::addToolBar(DockToolBar *bar, int band, int position)
{
    if (!bar) return;

    // Remove from current area if already placed
    removeToolBar(bar);

    ensureBand(band);

    // Set toolbar orientation to match the band
    bar->setOrientation(bandOrientation());
    bar->setEdge(m_edge);
    bar->setBand(band);

    auto &b = m_bands[band];
    if (position < 0 || position >= b.bars.size()) {
        // Insert before the stretch
        const int insertIdx = b.layout->count() - 1;  // before stretch
        b.layout->insertWidget(insertIdx, bar);
        b.bars.append(bar);
        bar->setBandPosition(b.bars.size() - 1);
    } else {
        b.layout->insertWidget(position, bar);
        b.bars.insert(position, bar);
        // Update positions
        for (int i = 0; i < b.bars.size(); ++i)
            b.bars[i]->setBandPosition(i);
    }

    bar->show();
    updateVisibility();
    emit toolBarAdded(bar);
}

bool DockToolBarArea::removeToolBar(DockToolBar *bar)
{
    if (!bar) return false;

    for (int i = 0; i < m_bands.size(); ++i) {
        auto &b = m_bands[i];
        const int idx = b.bars.indexOf(bar);
        if (idx >= 0) {
            b.layout->removeWidget(bar);
            b.bars.removeAt(idx);
            bar->setParent(nullptr);
            bar->hide();

            // Update positions
            for (int j = 0; j < b.bars.size(); ++j)
                b.bars[j]->setBandPosition(j);

            cleanupEmptyBands();
            updateVisibility();
            emit toolBarRemoved(bar);
            return true;
        }
    }
    return false;
}

void DockToolBarArea::moveToolBar(DockToolBar *bar, int targetBand, int position)
{
    removeToolBar(bar);
    addToolBar(bar, targetBand, position);
}

QList<DockToolBar *> DockToolBarArea::toolBars() const
{
    QList<DockToolBar *> result;
    for (const auto &b : m_bands)
        result.append(b.bars);
    return result;
}

QList<DockToolBar *> DockToolBarArea::toolBarsInBand(int band) const
{
    if (band < 0 || band >= m_bands.size())
        return {};
    return m_bands[band].bars;
}

bool DockToolBarArea::isEmpty() const
{
    for (const auto &b : m_bands) {
        if (!b.bars.isEmpty())
            return false;
    }
    return true;
}

void DockToolBarArea::updateVisibility()
{
    bool hasVisible = false;
    for (const auto &b : m_bands) {
        for (auto *bar : b.bars) {
            if (bar->isVisible()) {
                hasVisible = true;
                break;
            }
        }
        if (hasVisible) break;
    }
    setVisible(hasVisible);
}

void DockToolBarArea::cleanupEmptyBands()
{
    // Remove trailing empty bands
    while (!m_bands.isEmpty() && m_bands.last().bars.isEmpty()) {
        auto &b = m_bands.last();
        m_rootLayout->removeWidget(b.container);
        delete b.container;
        m_bands.removeLast();
    }
}

} // namespace exdock
