#pragma once

#include <QString>

// ── Terminal Service ─────────────────────────────────────────────────────────
//
// Stable SDK interface for terminal interaction.
// Requires PluginPermission::TerminalExecute for command execution.

class ITerminalService
{
public:
    virtual ~ITerminalService() = default;

    /// Run a command in a new terminal tab (non-blocking).
    virtual void runCommand(const QString &command) = 0;

    /// Send raw input to the active terminal.
    virtual void sendInput(const QString &text) = 0;

    /// Get recent output from the active terminal.
    virtual QString recentOutput(int maxLines = 50) const = 0;

    /// Open a new empty terminal tab.
    virtual void openTerminal() = 0;

    /// Get the currently selected text from the active terminal view.
    virtual QString selectedText() const = 0;

    /// Set the working directory for the active terminal tab.
    virtual void setWorkingDirectory(const QString &dir) = 0;
};
