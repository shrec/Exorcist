#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPlainTextEdit;
class QCheckBox;
class QPushButton;

// ── RunConfigDialog ─────────────────────────────────────────────────────────
// Per-target Run/Debug configuration editor. Lets the user set program
// arguments, environment variables (KEY=value lines), working directory, and
// a "run in external terminal" flag. Values are persisted by the caller via
// QSettings (per-workspace, per-target).
//
// The dialog itself is a pure view: the caller seeds it with the current
// values and reads them back on Accept via the public getters.
class RunConfigDialog : public QDialog
{
    Q_OBJECT

public:
    RunConfigDialog(const QString &args,
                    const QString &envBlock,
                    const QString &cwd,
                    QWidget *parent = nullptr);

    // Edited values (valid after exec() == Accepted; harmless to call anytime).
    QString args() const;
    QString envBlock() const;
    QString workingDir() const;
    bool    useExternalTerminal() const;

private slots:
    void browseWorkingDir();

private:
    void buildUi(const QString &args,
                 const QString &envBlock,
                 const QString &cwd);

    QLineEdit      *m_argsEdit       = nullptr;
    QPlainTextEdit *m_envEdit        = nullptr;
    QLineEdit      *m_cwdEdit        = nullptr;
    QPushButton    *m_browseBtn      = nullptr;
    QCheckBox      *m_externalTerm   = nullptr;
};
