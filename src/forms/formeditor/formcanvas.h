// formcanvas.h — the design surface.
//
// FormCanvas owns the live widget tree being edited.  It accepts drops from
// the palette, instantiates widgets via QUiLoader::createWidget() (which is
// the supported way to create Qt widgets by class name without a giant
// switch-statement), tracks selection, paints selection grips, and routes
// keyboard / mouse interaction to mutate position and size.
//
// Layout philosophy for Phase 1: absolute positioning.  Children are laid out
// at their setGeometry() — the same way Qt Designer's "Break Layout" mode
// works.  Layouts (HBox/VBox/Grid/Form) are Phase 2 and live as commands on
// the toolbar.
//
// Selection model
//   m_selection is a small QSet<QWidget*> updated by mousePressEvent.
//     - plain click            → replace selection with hit widget
//     - Ctrl+click             → toggle widget in/out of selection
//     - drag on empty area     → marquee select
//     - Shift+marquee          → additive marquee
//   m_primary is the most recently clicked widget (used as the anchor for
//   align/distribute and as the source of properties when multiple widgets
//   are selected).
//
// Smart guides (Figma-style)
//   While dragging, snapTo() compares the moving widget's edges (left/centre/
//   right + top/middle/bottom) to every sibling's edges within ±5 px.  When a
//   match is found we (a) snap the geometry, and (b) cache the alignment line
//   so paintEvent() can draw a thin blue overlay.
//
// Undo
//   All mutations are pushed onto a QUndoStack owned by UiFormEditor and
//   passed in via setUndoStack().  Each command is intentionally fine-grained
//   (one drag = one MoveWidgetsCommand, not N).
#pragma once

#include <QHash>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QSet>
#include <QString>
#include <QWidget>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QUndoStack;

namespace exo::forms {

class FormCanvas : public QWidget {
    Q_OBJECT
public:
    explicit FormCanvas(QWidget *parent = nullptr);

    // Replace the form root with a freshly loaded tree (e.g. after .ui load).
    // Takes ownership; the previous root is deleted.
    void setRoot(QWidget *newRoot);
    QWidget *root() const { return m_root; }

    // Top-level form widget reflected back to UiFormEditor for save().
    QWidget *formRoot() const { return m_root; }

    // Selection accessors / mutation.  selection() returns a stable list
    // (most-recent-first) and primary() the anchor for property editing.
    QList<QWidget *> selection() const;
    QWidget         *primary()    const { return m_primary; }
    void             clearSelection();
    void             selectOnly(QWidget *w);
    void             selectAll();

    // Bind to a shared QUndoStack so all canvas mutations are undoable.
    void setUndoStack(QUndoStack *stack) { m_undo = stack; }

    // Toggle preview / edit mode.  In preview mode we hide grips, ignore
    // selection clicks, and make widgets respond to user events normally.
    void setPreviewMode(bool on);
    bool isPreviewMode() const { return m_preview; }

    // Insert by class name at a given canvas-local position.  Returns the
    // newly-instantiated widget (parented to m_root), or nullptr on failure.
    QWidget *insertWidget(const QString &className, const QPoint &where);

    // Delete current selection (used by Delete shortcut and Edit menu).
    void deleteSelection();
    void duplicateSelection();
    void nudgeSelection(int dx, int dy);

signals:
    void selectionChanged();
    void modified();

protected:
    void paintEvent(QPaintEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dragMoveEvent(QDragMoveEvent *ev) override;
    void dropEvent(QDropEvent *ev) override;

private:
    // Hit testing — returns the deepest direct child of m_root under p.
    // (We deliberately don't recurse into containers in Phase 1; nested
    // selection is Phase 2 once we have a proper Object Inspector.)
    QWidget *hitTest(const QPoint &p) const;

    // Smart-guide snap.  Returns the snapped top-left for `target` (in m_root
    // coords) given a desired top-left, and updates m_guideLines.
    QPoint snapTo(QWidget *target, const QPoint &desiredTopLeft);

    // Paint helpers — kept in cpp to avoid pulling QPainter into the header.
    void  paintSelectionOverlay(QWidget *w);

    QWidget    *m_root    = nullptr;   // form root (re-parented child)
    QSet<QWidget *> m_selection;
    QWidget    *m_primary = nullptr;
    QUndoStack *m_undo    = nullptr;
    bool        m_preview = false;

    // Drag state
    bool       m_dragging      = false;
    QPoint     m_dragStartPos;             // cursor pos at press (canvas coords)
    QHash<QWidget *, QPoint> m_dragOrigin; // per-widget original top-left

    // Marquee state
    bool       m_marquee  = false;
    QPoint     m_marqueeStart;
    QPoint     m_marqueeEnd;
    bool       m_marqueeAdditive = false;

    // Smart-guide state — list of (orientation, position) lines to paint.
    struct Guide { bool vertical; int pos; };
    QList<Guide> m_guideLines;
};

} // namespace exo::forms
