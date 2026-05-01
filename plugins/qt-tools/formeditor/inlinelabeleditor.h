// inlinelabeleditor.h — overlay editor for in-place text/label editing.
//
// Phase 1 had a stub: double-click selected the widget but did not allow
// editing.  Phase 2 ships a real inline-edit overlay — a QLineEdit (single
// line) or QPlainTextEdit-like behaviour (Shift+Enter newline) anchored as a
// child of FormCanvas, positioned on top of the target widget with identical
// geometry.  This implements Principle 3 (Inline > Modal) of ux-principles.md
// for the "text" property of common widgets.
//
// Trigger contract
//   FormCanvas::mouseDoubleClickEvent calls InlineLabelEditor::beginEdit()
//   when the hit widget exposes a "text" or "title" QMetaProperty.  The
//   editor reads the current value, anchors itself over the widget, takes
//   focus, and selects all text.  Enter commits, Esc cancels, focus loss
//   commits (matches Designer / VS Code behaviour).
//
// Undo
//   commitEdit() pushes a PropertyChangeCommand onto the canvas's QUndoStack
//   so the inline edit is fully reversible like any other property change.
//
// Multiline support
//   Some widgets (QGroupBox::title is single-line; QLabel::text may be
//   multiline) — Shift+Enter inserts a newline.  We use a QPlainTextEdit-
//   based subclass internally so multi-line works naturally; for fields we
//   know are single-line (objectName, button text), we restrict to one line.
#pragma once

#include <QPlainTextEdit>
#include <QPointer>
#include <QString>

class QKeyEvent;
class QFocusEvent;
class QUndoStack;

namespace exo::forms {

class FormCanvas;

class InlineLabelEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit InlineLabelEditor(FormCanvas *canvas);

    // Begin editing the given widget's textual property ("text" or "title").
    // Returns false if the target exposes neither.
    bool beginEdit(QWidget *target, QUndoStack *stack);

    // Commit / cancel — public so FormCanvas can dismiss us on selection
    // changes or preview-mode toggles.
    void commitEdit();
    void cancelEdit();

    bool isEditing() const { return m_target != nullptr; }

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    void focusOutEvent(QFocusEvent *ev) override;

private:
    void positionOverTarget();

    FormCanvas        *m_canvas       = nullptr;
    QPointer<QWidget>  m_target;
    QUndoStack        *m_stack        = nullptr;
    QByteArray         m_propertyName;     // "text" or "title"
    QString            m_originalValue;
    bool               m_committing   = false;  // re-entrancy guard
};

} // namespace exo::forms
