#include "DockSplitter.h"

#include <QGuiApplication>
#include <QPainter>
#include <QResizeEvent>
#include <QScreen>
#include <QShowEvent>
#include <algorithm>
#include <numeric>

namespace exdock {

// ── DockSplitterHandle ───────────────────────────────────────────────────────

DockSplitterHandle::DockSplitterHandle(Qt::Orientation orientation,
                                       QSplitter *parent)
    : QSplitterHandle(orientation, parent)
{
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
}

void DockSplitterHandle::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    // Base: same color as toolbar background so handle is flush
    p.fillRect(rect(), QColor(45, 45, 48));

    // Thin center line — visible but subtle; #007acc blue on hover
    const QColor lineColor = m_hovered ? QColor(0x00, 0x7a, 0xcc) : QColor(63, 63, 70);
    if (orientation() == Qt::Horizontal) {
        const int cx = width() / 2;
        p.fillRect(cx, 0, 1, height(), lineColor);
    } else {
        const int cy = height() / 2;
        p.fillRect(0, cy, width(), 1, lineColor);
    }
}

void DockSplitterHandle::enterEvent(QEnterEvent *event)
{
    m_hovered = true;
    update();
    QSplitterHandle::enterEvent(event);
}

void DockSplitterHandle::leaveEvent(QEvent *event)
{
    m_hovered = false;
    update();
    QSplitterHandle::leaveEvent(event);
}

// ── DockSplitter ─────────────────────────────────────────────────────────────

DockSplitter::DockSplitter(Qt::Orientation orient, QWidget *parent)
    : QSplitter(orient, parent)
{
    setChildrenCollapsible(false);
    // 5px handle — wide enough to grab comfortably, thin enough to be unobtrusive
    setHandleWidth(5);
    setObjectName(QStringLiteral("exdock-splitter"));

    // Allow the splitter itself to shrink to its children's minimums
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSplitterHandle *DockSplitter::createHandle()
{
    return new DockSplitterHandle(orientation(), this);
}

void DockSplitter::insertChildWidget(int index, QWidget *widget)
{
    QSplitter::insertWidget(index, widget);
    widget->show();

    const int realIdx = indexOf(widget);

    // Insert default tracking values at the actual insertion position.
    // This mirrors QSplitter shifting its children at the same position,
    // keeping m_stretchFactors / m_minSizes in sync with widget order.
    while (m_stretchFactors.size() < realIdx)
        m_stretchFactors.append(1);
    m_stretchFactors.insert(realIdx, 1);

    while (m_minSizes.size() < realIdx)
        m_minSizes.append(100);
    m_minSizes.insert(realIdx, 100);

    // Enforce non-collapsible and set native stretch
    setCollapsible(realIdx, false);
    QSplitter::setStretchFactor(realIdx, m_stretchFactors.value(realIdx, 1));
}

DockSplitter *DockSplitter::splitChild(QWidget *existingChild,
                                        QWidget *newChild,
                                        Qt::Orientation splitOrientation,
                                        bool insertAfter)
{
    const int idx = indexOf(existingChild);
    if (idx < 0) return nullptr;

    // If same orientation, just insert next to existing child
    if (orientation() == splitOrientation) {
        const int insertIdx = insertAfter ? idx + 1 : idx;
        insertChildWidget(insertIdx, newChild);

        // Distribute evenly
        distributeByStretch();
        return nullptr;
    }

    // Different orientation — need a sub-splitter
    const auto savedSizes = sizes();
    const auto savedRatios = sizeRatios();
    existingChild->setParent(nullptr);

    auto *sub = new DockSplitter(splitOrientation, nullptr);

    if (insertAfter) {
        sub->insertChildWidget(0, existingChild);
        sub->insertChildWidget(1, newChild);
    } else {
        sub->insertChildWidget(0, newChild);
        sub->insertChildWidget(1, existingChild);
    }

    // Equal split inside the sub-splitter
    const int total = (splitOrientation == Qt::Horizontal)
                      ? sub->width() : sub->height();
    const int half = qMax(total / 2, 100);
    sub->setSizes({half, half});

    insertWidget(idx, sub);

    // Restore stretch/min tracking
    while (m_stretchFactors.size() < count())
        m_stretchFactors.append(1);
    while (m_minSizes.size() < count())
        m_minSizes.append(100);
    setCollapsible(idx, false);

    // Restore saved ratios for the original children
    if (!savedRatios.isEmpty())
        restoreFromRatios(savedRatios);

    return sub;
}

bool DockSplitter::removeChild(QWidget *child)
{
    const int idx = indexOf(child);
    if (idx < 0) return count() == 0;

    child->setParent(nullptr);
    child->hide();

    // Remove tracking entries
    if (idx < m_stretchFactors.size())
        m_stretchFactors.removeAt(idx);
    if (idx < m_minSizes.size())
        m_minSizes.removeAt(idx);

    // If this splitter has only one child left, promote it to the parent.
    // Skip promotion for root/center splitters (owned by DockManager) —
    // those are marked permanent and must never self-destruct.
    if (count() == 1 && !m_permanent) {
        auto *remaining = widget(0);
        auto *parentSplitter = qobject_cast<DockSplitter *>(parentWidget());
        if (parentSplitter) {
            const int myIdx = parentSplitter->indexOf(this);

            // Transfer tracking values for remaining child
            int stretchVal = m_stretchFactors.value(0, 1);
            int minVal     = m_minSizes.value(0, 100);

            remaining->setParent(nullptr);
            parentSplitter->insertWidget(myIdx, remaining);
            remaining->show();
            parentSplitter->setCollapsible(myIdx, false);

            // Update parent's tracking at the promoted position
            int promotedIdx = parentSplitter->indexOf(remaining);
            parentSplitter->setChildStretchFactor(promotedIdx, stretchVal);
            parentSplitter->setChildMinimumSize(promotedIdx, minVal);

            setParent(nullptr);
            deleteLater();

            // Cascade: if parent now has only one child, it should
            // also simplify. Use deleteLater so 'this' is fully gone.
            if (parentSplitter->count() == 1) {
                QMetaObject::invokeMethod(parentSplitter, [parentSplitter]() {
                    auto *pp = qobject_cast<DockSplitter *>(parentSplitter->parentWidget());
                    if (pp && parentSplitter->count() == 1) {
                        auto *last = parentSplitter->widget(0);
                        const int pIdx = pp->indexOf(parentSplitter);
                        int sf = parentSplitter->m_stretchFactors.value(0, 1);
                        int ms = parentSplitter->m_minSizes.value(0, 100);
                        last->setParent(nullptr);
                        pp->insertWidget(pIdx, last);
                        last->show();
                        pp->setCollapsible(pIdx, false);
                        int idx = pp->indexOf(last);
                        pp->setChildStretchFactor(idx, sf);
                        pp->setChildMinimumSize(idx, ms);
                        parentSplitter->setParent(nullptr);
                        parentSplitter->deleteLater();
                    }
                }, Qt::QueuedConnection);
            }
        }
    }

    return count() == 0;
}

int DockSplitter::visibleCount() const
{
    int n = 0;
    for (int i = 0; i < count(); ++i) {
        if (widget(i)->isVisible())
            ++n;
    }
    return n;
}

// ── Resize policy helpers ────────────────────────────────────────────────────

void DockSplitter::setChildStretchFactor(int index, int stretch)
{
    while (m_stretchFactors.size() <= index)
        m_stretchFactors.append(1);
    m_stretchFactors[index] = qMax(1, stretch);

    // Also set QSplitter's native stretch factor so it takes effect
    // during initial layout before the widget has a valid size.
    QSplitter::setStretchFactor(index, qMax(1, stretch));
}

void DockSplitter::distributeByStretch()
{
    if (count() == 0) return;

    // Ensure stretch factors are initialized
    while (m_stretchFactors.size() < count())
        m_stretchFactors.append(1);

    // Use contentsRect to respect margins (sidebars reserve space via margins).
    const QRect cr = contentsRect();
    const int total = (orientation() == Qt::Horizontal)
                      ? cr.width() : cr.height();
    const int handleSpace = handleWidth() * (count() - 1);
    const int available = total - handleSpace;

    if (available <= 0) return;

    const int totalStretch = std::accumulate(
        m_stretchFactors.begin(),
        m_stretchFactors.begin() + count(), 0);

    if (totalStretch == 0) return;

    QList<int> newSizes;
    newSizes.reserve(count());
    int assigned = 0;

    for (int i = 0; i < count() - 1; ++i) {
        int s = (available * m_stretchFactors[i]) / totalStretch;
        // Enforce minimum
        if (i < m_minSizes.size())
            s = qMax(s, m_minSizes[i]);
        newSizes.append(s);
        assigned += s;
    }
    // Last child gets the remainder (prevents rounding drift)
    newSizes.append(qMax(available - assigned, 0));

    setSizes(newSizes);
}

QList<double> DockSplitter::sizeRatios() const
{
    const auto s = sizes();
    const int total = std::accumulate(s.begin(), s.end(), 0);
    QList<double> ratios;
    if (total == 0) {
        for (int i = 0; i < s.size(); ++i)
            ratios.append(1.0 / qMax(1, s.size()));
        return ratios;
    }
    for (int v : s)
        ratios.append(static_cast<double>(v) / total);
    return ratios;
}

void DockSplitter::restoreFromRatios(const QList<double> &ratios)
{
    if (ratios.isEmpty() || count() == 0)
        return;

    const QRect cr = contentsRect();
    const int total = (orientation() == Qt::Horizontal)
                      ? cr.width() : cr.height();
    const int handleSpace = handleWidth() * (count() - 1);
    const int available = total - handleSpace;

    QList<int> newSizes;
    int assigned = 0;
    const int n = qMin(ratios.size(), count());

    for (int i = 0; i < n - 1; ++i) {
        int s = static_cast<int>(available * ratios[i]);
        if (i < m_minSizes.size())
            s = qMax(s, m_minSizes[i]);
        newSizes.append(s);
        assigned += s;
    }
    newSizes.append(qMax(available - assigned, 0));

    // Pad if ratios are fewer than children
    while (newSizes.size() < count())
        newSizes.append(100);

    setSizes(newSizes);
}

void DockSplitter::setChildMinimumSize(int index, int minSize)
{
    while (m_minSizes.size() <= index)
        m_minSizes.append(100);
    m_minSizes[index] = qMax(0, minSize);
}

QSize DockSplitter::minimumSizeHint() const
{
    int minW = 0, minH = 0;
    for (int i = 0; i < count(); ++i) {
        const QSize childMin = widget(i)->minimumSizeHint();
        if (orientation() == Qt::Horizontal) {
            minW += qMax(childMin.width(),
                         i < m_minSizes.size() ? m_minSizes[i] : 100);
            minH = qMax(minH, childMin.height());
        } else {
            minH += qMax(childMin.height(),
                         i < m_minSizes.size() ? m_minSizes[i] : 100);
            minW = qMax(minW, childMin.width());
        }
    }
    // Add handle widths
    if (count() > 1) {
        if (orientation() == Qt::Horizontal)
            minW += handleWidth() * (count() - 1);
        else
            minH += handleWidth() * (count() - 1);
    }
    return {minW, minH};
}

void DockSplitter::resizeEvent(QResizeEvent *event)
{
    QSplitter::resizeEvent(event);
    if (m_firstShow && count() > 0) {
        // Use contentsRect (not event->size()) to respect margins.
        const QRect cr = contentsRect();
        const int total = (orientation() == Qt::Horizontal)
                          ? cr.width() : cr.height();
        if (total > 0) {
            m_firstShow = false;
            distributeByStretch();
            return; // skip enforceMinimumSizes right after distribution
        }
    }
    enforceMinimumSizes();
}

void DockSplitter::showEvent(QShowEvent *event)
{
    QSplitter::showEvent(event);
}

void DockSplitter::enforceMinimumSizes()
{
    if (count() == 0) return;

    auto s = sizes();
    bool needsUpdate = false;

    for (int i = 0; i < s.size(); ++i) {
        const int minS = (i < m_minSizes.size()) ? m_minSizes[i] : 100;
        if (s[i] < minS) {
            s[i] = minS;
            needsUpdate = true;
        }
    }

    if (needsUpdate)
        setSizes(s);
}

} // namespace exdock
