// promotedialog.h — small modal for "Promote widget to custom class".
//
// Designer's promotion workflow lets the user say "this QPushButton is
// actually MyButton (in mybutton.h)".  The .ui file gains a <customwidgets>
// section and the affected widget's class attribute is rewritten from the
// base type to the promoted name.  This dialog gathers the three inputs we
// need:
//   - promoted class name (e.g. "MyButton")
//   - header file        (e.g. "mybutton.h")
//   - global include      (use <…> instead of "…")
// Base class is derived from the target widget and shown read-only so the
// user knows what they're extending.
//
// We stay modal because we are mutating a single widget's identity — that
// is a structural change, not a routine property tweak (UX principle 3).
// However the dialog is intentionally tiny: 4 fields + OK/Cancel, no tabs,
// no nested settings.
#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;

namespace exo::forms {

class PromoteDialog : public QDialog {
    Q_OBJECT
public:
    // Construct with the base class name of the widget being promoted.
    // initial* parameters allow re-editing an existing promotion (Demote
    // would delete it; Promote on an already-promoted widget reopens with
    // current values).
    explicit PromoteDialog(const QString &baseClass,
                           const QString &initialClassName = {},
                           const QString &initialHeader    = {},
                           bool           initialGlobal    = false,
                           QWidget       *parent           = nullptr);

    QString className()    const;
    QString headerFile()   const;
    bool    globalInclude() const;

private slots:
    // Auto-suggest "mybutton.h" from "MyButton" while the class field changes,
    // unless the user has already manually edited the header field.
    void onClassNameEdited(const QString &text);
    void onHeaderManuallyEdited();

private:
    QString    m_baseClass;
    QLineEdit *m_className   = nullptr;
    QLineEdit *m_headerFile  = nullptr;
    QCheckBox *m_globalInc   = nullptr;
    QLabel    *m_baseLabel   = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    bool       m_headerEdited = false;
};

} // namespace exo::forms
