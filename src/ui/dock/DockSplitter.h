#pragma once

#include <QSplitter>
#include <QSplitterHandle>
#include <QList>

namespace exdock {

class DockArea;

/// VS2022-style splitter handle — shows a thin blue highlight line on hover.
class DockSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
public:
    explicit DockSplitterHandle(Qt::Orientation orientation, QSplitter *parent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool m_hovered = false;
};

/// A splitter that holds DockAreas and/or other DockSplitters to form
/// a recursive layout tree. When a child area is removed and only one
/// child remains, the splitter simplifies itself.
///
/// Resize policy features:
/// - Minimum sizes are enforced per child (never collapse to zero).
/// - Stretch factors give the editor (center) priority on resize.
/// - Splitter sizes are saved/restored for layout persistence.
/// - Children are non-collapsible by default to prevent accidental loss.
class DockSplitter : public QSplitter
{
    Q_OBJECT

public:
    explicit DockSplitter(Qt::Orientation orient,
                          QWidget *parent = nullptr);

protected:
    QSplitterHandle *createHandle() override;

public:

    /// Insert a widget (DockArea or DockSplitter) at a position.
    void insertChildWidget(int index, QWidget *widget);

    /// Replace an existing child with a new splitter containing the
    /// old child + new child at the given relative position.
    /// Returns the new sub-splitter.
    DockSplitter *splitChild(QWidget *existingChild,
                             QWidget *newChild,
                             Qt::Orientation splitOrientation,
                             bool insertAfter);

    /// Remove a child and simplify if only one child remains.
    /// Returns true if the splitter is now empty.
    bool removeChild(QWidget *child);

    /// The number of visible children.
    int visibleCount() const;

    // ── Resize policy helpers ─────────────────────────────────────────

    /// Set the stretch factor for a child. Higher stretch = more space
    /// when resizing. Use stretch=1 for side panels, stretch=3+ for editor.
    void setChildStretchFactor(int index, int stretch);

    /// Distribute sizes proportionally based on stretch factors.
    void distributeByStretch();

    /// Save current splitter sizes as a ratio vector (for persistence).
    QList<double> sizeRatios() const;

    /// Restore sizes from a ratio vector (0.0-1.0 per child).
    void restoreFromRatios(const QList<double> &ratios);

    /// Set minimum size for a specific child index.
    void setChildMinimumSize(int index, int minSize);

    /// Permanent splitters (root/center) are never collapsed or self-deleted
    /// by removeChild, even when only one child remains.
    void setPermanent(bool permanent) { m_permanent = permanent; }
    bool isPermanent() const { return m_permanent; }

    /// Resize event — ensure minimum sizes enforced after layout.
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void enforceMinimumSizes();

    QList<int> m_stretchFactors;
    QList<int> m_minSizes;
    bool       m_firstShow  = true;
    bool       m_permanent  = false;
};

} // namespace exdock
