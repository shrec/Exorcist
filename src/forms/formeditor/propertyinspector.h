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
// We deliberately don't push every property edit onto the undo stack in
// Phase 1: the granularity for property edits is coarse (per commit, not per
// keystroke) and the bookkeeping for "delete this widget then undo" already
// snapshots geometry.  Phase 2 will introduce a SetPropertyCommand.
#pragma once

#include <QPointer>
#include <QString>
#include <QWidget>

class QLineEdit;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

namespace exo::forms {

class PropertyInspector : public QWidget {
    Q_OBJECT
public:
    explicit PropertyInspector(QWidget *parent = nullptr);

    // Bind to a target widget (the canvas selection's primary).  Pass nullptr
    // to clear.  Re-binding rebuilds the table.
    void setTarget(QWidget *target);

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

    QPointer<QWidget> m_target;
    FilterMode        m_filter = ShowAll;
};

} // namespace exo::forms
