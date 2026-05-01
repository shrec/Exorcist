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

    // Start an arbitrary program (e.g. ssh) with arguments.
    void startProgram(const QString &program, const QStringList &args,
                      const QString &workDir = {});

    // Change the working directory.  If shell is already running, sends a
    // 'cd' command.  Otherwise starts a fresh shell in workDir.
    void setWorkingDirectory(const QString &dir);

    // Send a command line to the shell (appends newline).
    void sendCommand(const QString &cmd);
    // Send raw input to the shell (no newline).
    void sendInput(const QString &text);
    // Get recent terminal output (best-effort, last N lines).
    QString recentOutput(int maxLines = 80) const;
    // Get the currently selected text in the terminal view.
    QString selectedText() const;

    // Font-size controls (forwarded to TerminalView).  Used by TerminalPanel's
    // per-terminal QShortcuts (Ctrl+Shift+= / Ctrl+Shift+-) and by the
    // right-click context menu.
    void zoomIn();
    void zoomOut();
    void resetZoom();
    int  fontSize() const;

signals:
    // Emitted when the user picks "Close" in the right-click context menu.
    // The owning TerminalPanel listens to remove the corresponding tab.
    void closeRequested();

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
