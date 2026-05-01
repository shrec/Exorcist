#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;

/// Small modal dialog for setting/editing a conditional breakpoint expression.
///
/// The expression is a C/C++ expression that GDB evaluates when the breakpoint
/// is hit; the breakpoint stops execution only if the expression is true.
///
/// Styled with the VS dark theme palette (#252526 background, #d4d4d4 text,
/// #3c3c3c borders) to match the rest of the editor surface.
class BreakpointConditionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BreakpointConditionDialog(QWidget *parent = nullptr,
                                       const QString &existingCondition = QString(),
                                       int line = 0);

    /// Returns the (possibly empty) condition entered by the user.
    /// An empty string means "remove the condition" (unconditional breakpoint).
    QString condition() const;

private:
    QLineEdit *m_input = nullptr;
};
