#pragma once

#include <QObject>
#include <QList>
#include <QString>

/// Represents a single discovered task.
struct TaskEntry {
    QString name;        // task name as shown to user
    QString command;     // shell command to execute
    QString source;      // which file defined it (e.g. "Taskfile.yml")
    QString sourceType;  // "taskfile" | "justfile" | "makefile" | "npm" | "script"
    QString description; // optional task description
};

/// Discovers runnable tasks from various task-definition files in a workspace.
class TaskDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit TaskDiscovery(QObject *parent = nullptr);

    void setWorkspaceRoot(const QString &root);
    void refresh();

    QList<TaskEntry> tasks() const { return m_tasks; }
    bool hasTasks() const { return !m_tasks.empty(); }

signals:
    void tasksChanged();

private:
    void discoverTaskfile(const QString &path);
    void discoverJustfile(const QString &path);
    void discoverMakefile(const QString &path);
    void discoverNpmScripts(const QString &path);
    void discoverScripts(const QString &dir);

    QString m_root;
    QList<TaskEntry> m_tasks;
};
