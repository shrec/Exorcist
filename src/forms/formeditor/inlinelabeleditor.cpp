#include "inlinelabeleditor.h"

#include "formcanvas.h"

#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMetaProperty>
#include <QPlainTextEdit>
#include <QUndoCommand>
#include <QUndoStack>
#include <QWidget>

namespace exo::forms {

// ── Local PropertyChangeCommand ──────────────────────────────────────────────
//
// Mirrors the PropertyChangeCommand declared in formcanvas.cpp; we replicate
// it here as a private command so the inline editor can push without leaking
// a header.  This keeps undo granularity identical to typed property edits.
namespace {

class InlinePropertyChangeCommand : public QUndoCommand {
public:
    InlinePropertyChangeCommand(QPointer<QWidget> target,
                                const QByteArray &propName,
                                const QVariant &oldValue,
                                const QVariant &newValue,
                                FormCanvas *canvas)
        : QUndoCommand(QObject::tr("Edit %1").arg(QString::fromLatin1(propName)))
        , m_target(target), m_prop(propName)
        , m_old(oldValue), m_new(newValue), m_canvas(canvas) {}

    void redo() override {
        if (!m_target) return;
        m_target->setProperty(m_prop.constData(), m_new);
        m_target->setProperty(
            (QByteArray("exo_modified_") + m_prop).constData(), true);
        if (m_canvas) m_canvas->notifyExternalChange(m_target);
    }
    void undo() override {
        if (!m_target) return;
        m_target->setProperty(m_prop.constData(), m_old);
        if (m_canvas) m_canvas->notifyExternalChange(m_target);
    }

private:
    QPointer<QWidget> m_target;
    QByteArray        m_prop;
    QVariant          m_old, m_new;
    FormCanvas       *m_canvas;
};

} // namespace

// ── InlineLabelEditor ────────────────────────────────────────────────────────

InlineLabelEditor::InlineLabelEditor(FormCanvas *canvas)
    : QPlainTextEdit(canvas), m_canvas(canvas) {
    // Visual: VS dark, accent border to draw the eye, no scrollbars.
    setStyleSheet(
        "QPlainTextEdit{background:#1e1e1e;color:#ffffff;"
        "border:1px solid #007acc;padding:1px 3px;selection-background-color:#264f78;}");
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setTabChangesFocus(true);
    hide();
}

bool InlineLabelEditor::beginEdit(QWidget *target, QUndoStack *stack) {
    if (!target) return false;
    const QMetaObject *mo = target->metaObject();
    QByteArray prop;
    if (mo->indexOfProperty("text")  >= 0) prop = "text";
    else if (mo->indexOfProperty("title") >= 0) prop = "title";
    else return false;

    m_target = target;
    m_stack  = stack;
    m_propertyName  = prop;
    m_originalValue = target->property(prop.constData()).toString();

    setPlainText(m_originalValue);
    selectAll();
    positionOverTarget();
    show();
    raise();
    setFocus();
    return true;
}

void InlineLabelEditor::positionOverTarget() {
    if (!m_target || !m_canvas) return;
    QWidget *root = m_canvas->root();
    if (!root) return;
    // Map target's top-left in canvas coordinates: target is parented inside
    // root, so geometry is in root coords.  Add root's offset.
    const QRect r = m_target->geometry().translated(root->pos());
    setGeometry(r);
}

void InlineLabelEditor::commitEdit() {
    if (!m_target || m_committing) return;
    m_committing = true;
    const QString newValue = toPlainText();
    if (newValue != m_originalValue && m_stack) {
        m_stack->push(new InlinePropertyChangeCommand(
            m_target, m_propertyName, m_originalValue, newValue, m_canvas));
    } else if (newValue != m_originalValue) {
        // No undo stack — write directly.
        m_target->setProperty(m_propertyName.constData(), newValue);
    }
    if (m_canvas) m_canvas->emitModifiedSignal();
    m_target = nullptr;
    m_stack  = nullptr;
    hide();
    if (m_canvas) m_canvas->setFocus();
    m_committing = false;
}

void InlineLabelEditor::cancelEdit() {
    if (!m_target) return;
    m_target = nullptr;
    m_stack  = nullptr;
    hide();
    if (m_canvas) m_canvas->setFocus();
}

void InlineLabelEditor::keyPressEvent(QKeyEvent *ev) {
    if (ev->key() == Qt::Key_Escape) {
        cancelEdit();
        ev->accept();
        return;
    }
    if (ev->key() == Qt::Key_Return || ev->key() == Qt::Key_Enter) {
        if (ev->modifiers() & Qt::ShiftModifier) {
            // Shift+Enter → newline (multiline support).
            QPlainTextEdit::keyPressEvent(ev);
            return;
        }
        commitEdit();
        ev->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(ev);
}

void InlineLabelEditor::focusOutEvent(QFocusEvent *ev) {
    QPlainTextEdit::focusOutEvent(ev);
    if (isEditing()) commitEdit();
}

} // namespace exo::forms
