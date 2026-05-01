// formcanvas.h — the design surface.
//
// FormCanvas owns the live widget tree being edited.  It accepts drops from
// the palette, instantiates widgets via QUiLoader::createWidget() (which is
// the supported way to create Qt widgets by class name without a giant
// switch-statement), tracks selection, paints selection grips, and routes
// keyboard / mouse interaction to mutate position and size.
//
// Phase 2 additions:
//   - Layout management commands (HBox/VBox/Grid/Form/Break) — push undo,
//     create a container QWidget with the chosen layout, reparent selected
//     children into it.
//   - Inline label edit overlay — double-clicking a widget with a "text" or
//     "title" property pops a child QLineEdit/QPlainTextEdit overlay.
//   - Connection storage — list of FormConnection records used by the
//     SignalSlotEditor and serialized via uixmlio.
//   - Fine-grained PropertyChangeCommand — every property edit pushes its
//     own undo entry.
//   - Layout cell visualization — when dragging over a layout container, we
//     paint the cell boundaries to give spatial feedback (Principle 4).
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
//   (one drag = one MoveWidgetsCommand, one property edit = one
//   PropertyChangeCommand).
#pragma once

#include "signalsloteditor.h"   // FormConnection

#include <QHash>
#include <QList>
#include <QPoint>
#include <QPointer>
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

class InlineLabelEditor;

class FormCanvas : public QWidget {
    Q_OBJECT
public:
    enum LayoutKind {
        LayoutHorizontal,   // QHBoxLayout
        LayoutVertical,     // QVBoxLayout
        LayoutGrid,         // QGridLayout
        LayoutForm,         // QFormLayout
        LayoutBreak,        // remove parent layout, free-position children
    };

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
    void setUndoStack(QUndoStack *stack);
    QUndoStack *undoStack() const { return m_undo; }

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

    // Phase 2: layout commands.  Wrap selection in a new container that
    // owns the requested layout, reparenting the selected widgets.  Break
    // removes the parent layout of `primary` and restores absolute pos.
    void applyLayoutToSelection(LayoutKind kind);

    // Phase 2: inline label editing — invoked on double-click for widgets
    // exposing a "text" or "title" QMetaProperty.
    void beginInlineEdit(QWidget *target);

    // Phase 2: signal-slot connection storage (serialized in uixmlio.cpp).
    QList<FormConnection> connections() const { return m_connections; }
    void                  setConnections(const QList<FormConnection> &c);
    QWidget              *findNamedWidget(const QString &name) const;

    // Phase 2: brief flash highlight (used by SignalSlotEditor when sender
    // or receiver is changed — gives spatial feedback per Principle 9).
    void flashHighlight(QWidget *w);

    // Phase 2: hook called by external commands (PropertyChangeCommand) so
    // the canvas can repaint and the inspector can be re-targeted.
    void notifyExternalChange(QWidget *w);
    void emitModifiedSignal() { emit modified(); }

    // Phase 3: custom-widget promotion.  PromotionInfo travels with the
    // widget through save/load and changes how UiXmlIO emits the <widget
    // class="..."> attribute and the <customwidgets> block.  The runtime
    // widget itself stays whatever Qt class it was instantiated as (we
    // don't dynamically swap base types — that would require dlopen +
    // moc), only its on-disk identity is rewritten.  Designer's promotion
    // mechanism works exactly this way.
    struct PromotionInfo {
        QString className;       // e.g. "MyButton"
        QString headerFile;      // e.g. "mybutton.h"
        bool    globalInclude = false;
    };
    bool                    isPromoted(QWidget *w) const;
    PromotionInfo           promotionFor(QWidget *w) const;
    QHash<QWidget *, PromotionInfo> promotions() const { return m_promotions; }
    void                    setPromotion(QWidget *w, const PromotionInfo &info);
    void                    clearPromotion(QWidget *w);
    // Replace the entire promotion table (used on file load).  Keys are
    // resolved by widget objectName from the live tree.
    void                    setPromotionsByName(const QHash<QString, PromotionInfo> &byName);

    // Phase 2: a context menu requested → tells host editor to show the
    // signal-slot menu item, etc.
signals:
    void selectionChanged();
    void modified();
    void contextMenuOnWidget(QWidget *w, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;
    void contextMenuEvent(QContextMenuEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dragMoveEvent(QDragMoveEvent *ev) override;
    void dropEvent(QDropEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;

private:
    // Hit testing — returns the deepest direct child of m_root under p.
    QWidget *hitTest(const QPoint &p) const;

    // Smart-guide snap.  Returns the snapped top-left for `target` (in m_root
    // coords) given a desired top-left, and updates m_guideLines.
    QPoint snapTo(QWidget *target, const QPoint &desiredTopLeft);

    void  paintSelectionOverlay(QWidget *w);
    void  paintLayoutCells();      // Phase 2: layout cell boundaries on hover/drag
    void  paintFlashOverlay();     // Phase 2: brief highlight pulse
    void  paintPromotionBadges(); // Phase 3: purple chip on promoted widgets

    // Map a widget rect into canvas-local coordinates by walking up the
    // parent chain to m_root and translating.  Shared between selection
    // overlay and the promotion badge so both stay perfectly aligned.
    QRect mapToCanvas(QWidget *w) const;

    QWidget    *m_root    = nullptr;   // form root (re-parented child)
    QSet<QWidget *> m_selection;
    QWidget    *m_primary = nullptr;
    QUndoStack *m_undo    = nullptr;
    bool        m_preview = false;

    bool       m_dragging      = false;
    QPoint     m_dragStartPos;
    QHash<QWidget *, QPoint> m_dragOrigin;

    bool       m_marquee  = false;
    QPoint     m_marqueeStart;
    QPoint     m_marqueeEnd;
    bool       m_marqueeAdditive = false;

    struct Guide { bool vertical; int pos; };
    QList<Guide> m_guideLines;

    // Phase 2 state
    InlineLabelEditor             *m_inlineEditor    = nullptr;
    QList<FormConnection>          m_connections;
    QPointer<QWidget>              m_flashTarget;     // widget to flash-highlight
    int                            m_flashFrame = 0;
    QPointer<QWidget>              m_layoutHover;     // layout container under cursor

    // Phase 3: per-widget promotion table.  QHash<QWidget*, …>; we wipe
    // entries when widgets are deleted via clearPromotion() to keep the
    // table from holding dangling pointers across undo/redo.
    QHash<QWidget *, PromotionInfo> m_promotions;
};

} // namespace exo::forms
