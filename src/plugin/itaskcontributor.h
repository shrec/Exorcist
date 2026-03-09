#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// ── ITaskContributor ──────────────────────────────────────────────────────────
//
// Plugins implement this to provide custom task types and task execution.
// Static tasks are declared in the manifest (TaskContribution).
// This interface handles the runtime execution side.

struct TaskRunResult
{
    bool    success = false;
    int     exitCode = -1;
    QString output;
    QString error;
};

class ITaskContributor
{
public:
    virtual ~ITaskContributor() = default;

    /// Which task type does this contributor handle? (e.g., "shell", "process", "cmake")
    virtual QString taskType() const = 0;

    /// Execute a task. Called on a worker thread — safe to block.
    /// `taskConfig` mirrors the TaskContribution fields as JSON.
    virtual TaskRunResult executeTask(const QString &taskId,
                                     const QJsonObject &taskConfig) = 0;

    /// Cancel a running task.
    virtual void cancelTask(const QString &taskId) { Q_UNUSED(taskId); }
};

#define EXORCIST_TASK_CONTRIBUTOR_IID "org.exorcist.ITaskContributor/1.0"
Q_DECLARE_INTERFACE(ITaskContributor, EXORCIST_TASK_CONTRIBUTOR_IID)
