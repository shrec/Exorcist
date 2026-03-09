#pragma once

#include <QString>

// ── Task Service ─────────────────────────────────────────────────────────────
//
// Stable SDK interface for running build/test tasks.

class ITaskService
{
public:
    virtual ~ITaskService() = default;

    /// Run a registered task by id (from TaskContribution).
    virtual void runTask(const QString &taskId) = 0;

    /// Cancel a running task.
    virtual void cancelTask(const QString &taskId) = 0;

    /// Whether a task is currently running.
    virtual bool isTaskRunning(const QString &taskId) const = 0;
};
