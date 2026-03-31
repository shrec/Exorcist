#pragma once

#include <QObject>
#include <QString>

// ── Run Output Service ───────────────────────────────────────────────────────
//
// Stable SDK interface for writing to the IDE run/launch output panel.
// Plugins that launch processes (build plugin, test runners, language
// tools, etc.) can resolve this service to display process output in
// the Run panel without depending on the concrete RunLaunchPanel class.
//
// Registered in ServiceRegistry as "runOutputService".

class IRunOutputService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Append text to the run output (stdout or stderr).
    virtual void appendOutput(const QString &text, bool isError = false) = 0;

    /// Clear the run output.
    virtual void clear() = 0;

    /// Show the run panel dock.
    virtual void show() = 0;

    /// Notify that a process has started (updates panel header/status).
    virtual void notifyProcessStarted(const QString &executable) = 0;

    /// Notify that a process has finished.
    virtual void notifyProcessFinished(int exitCode) = 0;

signals:
    /// Emitted after output is appended.
    void outputAppended(const QString &text, bool isError);
};
