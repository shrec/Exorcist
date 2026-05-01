// propertyinspector.h — flat searchable property list (right pane).
//
// Design choice: NO nested categories.  Designer's nested tree wastes
// vertical space and forces the user to expand/collapse to find things.
// A flat sortable list with "Search…" at the top + "Modified / All" toggle
// is dramatically faster to use, and matches Figma's right-rail inspector.
//
// Per-type editors are created in createEditorForCell() based on the
// QMetaProperty's metaType — bool → QCheckBox, int → QSpinBox, etc.  When the
// editor commits, we call QMetaProperty::write() on the live widget AND mark
// "exo_modified_<name>" as a dynamic property so uixmlio knows to emit it.
//
// Phase 2: every property edit pushes its own PropertyChangeCommand onto the
// shared QUndoStack — fine-grained undo for typed values, color picks, etc.
// The command captures (widget, property name, old value, new value).
#pragma once

#include <QPointer>
#include <QString>
#include <QVariant>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;
class QUndoStack;

namespace exo::forms {

class PropertyInspector : public QWidget {
    Q_OBJECT
public:
    explicit PropertyInspector(QWidget *parent = nullptr);

    // Bind to a target widget (the canvas selection's primary).  Pass nullptr
    // to clear.  Re-binding rebuilds the table.
    void setTarget(QWidget *target);

    // Phase 2: bind to the editor's QUndoStack so property edits become
    // proper undoable commands.
    void setUndoStack(QUndoStack *stack) { m_undo = stack; }

    // Phase 2: external callers (e.g. PropertyChangeCommand from the canvas)
    // can request a refresh after they mutate the target.
    void refreshFromTarget();

signals:
    // Emitted whenever a property is committed by the user, so the editor
    // host can mark the document modified and update the canvas.
    void propertyChanged(QWidget *target, const QString &name);

private:
    enum FilterMode { ShowAll, ShowModified };

    void buildUi();
    void rebuild();
    void applyFilter();

    // Per-type editor factory.  Returns a widget that, when its value changes,
    // calls writeProperty() on the target.
    QWidget *createEditorForCell(int row, const char *propName, const QVariant &value);
    void     writeProperty(const char *propName, const QVariant &value);

    QLineEdit    *m_search   = nullptr;
    QPushButton  *m_btnAll   = nullptr;
    QPushButton  *m_btnMod   = nullptr;
    QTableWidget *m_table    = nullptr;
    QUndoStack   *m_undo     = nullptr;   // Phase 2: fine-grained undo

    QPointer<QWidget> m_target;
    FilterMode        m_filter = ShowAll;
};

} // namespace exo::forms
