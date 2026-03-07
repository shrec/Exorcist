#pragma once

#include <QWidget>
#include <QString>

class TerminalScreen;
class PtyBackend;
class TerminalView;

// ── TerminalWidget ────────────────────────────────────────────────────────────
// Top-level terminal widget shown in the Terminal dock.
// Composes TerminalScreen + PtyBackend + TerminalView.
//
// Usage:
//   auto *t = new TerminalWidget(parentWidget);
//   t->startShell("/path/to/project");   // optional; auto-starts on show
//   t->sendCommand("cmake --build .");   // send a line to the shell
class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget() override;

    // Start a shell in workDir (called automatically on first show if not started)
    void startShell(const QString &workDir = {});

    // Change the working directory.  If shell is already running, sends a
    // 'cd' command.  Otherwise starts a fresh shell in workDir.
    void setWorkingDirectory(const QString &dir);

    // Send a command line to the shell (appends newline).
    void sendCommand(const QString &cmd);

    // Send raw input bytes (without auto newline).
    void sendInput(const QString &text);

    // Returns most recent terminal output as plain text.
    QString recentOutput(int maxLines = 80) const;

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onShellFinished(int exitCode);
    void handleInput(const QByteArray &data);

private:
    TerminalScreen *m_screen;
    PtyBackend     *m_backend;
    TerminalView   *m_view;
    QString         m_workDir;
    bool            m_started    = false;
    bool            m_shellExited = false;
};
