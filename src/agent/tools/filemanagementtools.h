#pragma once

#include "../itool.h"

#include <QFileSystemWatcher>

#include <memory>

// ── DeleteFileTool ────────────────────────────────────────────────────────────
// Deletes a file or empty directory.

class DeleteFileTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_workspaceRoot;
};

// ── RenameFileTool ────────────────────────────────────────────────────────────
// Renames or moves a file/directory.

class RenameFileTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_workspaceRoot;
};

// ── CopyFileTool ──────────────────────────────────────────────────────────────
// Copies a file to a new location.

class CopyFileTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_workspaceRoot;
};

// ── FileWatcherTool ───────────────────────────────────────────────────────────
// Watches files/directories for changes and reports what changed.

class FileWatcherTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    void ensureWatcher();

    QString m_workspaceRoot;
    std::unique_ptr<QFileSystemWatcher> m_watcher;
    QStringList m_changes;
};
