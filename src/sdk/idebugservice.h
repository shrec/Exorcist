#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include "idebugadapter.h"  // DebugFrame

/// Stable SDK interface for debug service signals and breakpoint UI management.
///
/// Plugins register concrete implementations via ServiceRegistry as "debugService".
/// Core IDE wires to these signals for editor debug-line highlighting and navigation.
class IDebugService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    virtual ~IDebugService() = default;

    /// Add a breakpoint entry to the debug panel's visual list.
    virtual void addBreakpointEntry(const QString &filePath, int line) = 0;

    /// Remove a breakpoint entry from the debug panel's visual list.
    virtual void removeBreakpointEntry(const QString &filePath, int line) = 0;

    /// Look up the adapter-assigned breakpoint ID for a file:line location.
    /// Returns -1 if not found (e.g. adapter hasn't confirmed the breakpoint yet).
    virtual int breakpointIdForLocation(const QString &filePath, int line) const
    {
        Q_UNUSED(filePath) Q_UNUSED(line)
        return -1;
    }

signals:
    /// User wants to navigate to a source location (e.g. double-click stack frame).
    void navigateToSource(const QString &filePath, int line);

    /// Debugger stopped; frames are the current call stack.
    void debugStopped(const QList<DebugFrame> &frames);

    /// Debug session terminated.
    void debugTerminated();
};
