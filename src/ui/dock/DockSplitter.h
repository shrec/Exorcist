#pragma once

#include <QSplitter>
#include <QList>

namespace exdock {

class DockArea;

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

    /// Resize event — ensure minimum sizes enforced after layout.
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void enforceMinimumSizes();

    QList<int> m_stretchFactors;
    QList<int> m_minSizes;
    bool       m_firstShow = true;
};

} // namespace exdock
