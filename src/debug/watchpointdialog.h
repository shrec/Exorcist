#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;

/// Dialog for configuring a data breakpoint (watchpoint).
///
/// Lets the user enter an expression (variable name or memory expression
/// such as "*p" or "arr[3]") and choose the trigger type — Write, Read,
/// or Access (read-or-write). Maps directly to DebugWatchpoint::Type values.
///
/// Emits watchpointConfigured(expression, type) on accept where:
///   type = 0 → Write
///   type = 1 → Read
///   type = 2 → Access
///
/// The numeric type values intentionally match DebugWatchpoint::Type so
/// callers can static_cast directly without re-mapping.
class WatchpointDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WatchpointDialog(QWidget *parent = nullptr);
    ~WatchpointDialog() override;

    /// Pre-fill the expression field (e.g. from a selected variable).
    void setExpression(const QString &expression);

    QString expression() const;
    int     watchType() const;     // 0=Write, 1=Read, 2=Access

signals:
    /// Emitted on accept. type is DebugWatchpoint::Type cast to int.
    void watchpointConfigured(const QString &expression, int type);

private slots:
    void onAccept();

private:
    QLineEdit  *m_exprInput   = nullptr;
    QComboBox  *m_typeCombo   = nullptr;
    QPushButton *m_okBtn      = nullptr;
    QPushButton *m_cancelBtn  = nullptr;
};
