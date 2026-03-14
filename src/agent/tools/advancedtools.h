#pragma once

#include "../itool.h"

class QDir;

// ── FileSearchTool ────────────────────────────────────────────────────────────
// Searches for files in the workspace by glob pattern.

class FileSearchTool : public ITool
{
public:
    explicit FileSearchTool(const QString &workspaceRoot = {});

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_workspaceRoot;
};

// ── CreateDirectoryTool ───────────────────────────────────────────────────────
// Creates a directory (and all necessary parent directories).

class CreateDirectoryTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ── MultiReplaceStringTool ────────────────────────────────────────────────────
// Applies multiple string replacements in a single call — more efficient
// than calling replace_string_in_file multiple times.

class MultiReplaceStringTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ── InsertEditIntoFileTool ────────────────────────────────────────────────────
// Inserts/replaces text at a specific line range in a file.

class InsertEditIntoFileTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ── ManageTodoListTool ────────────────────────────────────────────────────────
// Manages a structured todo list stored as JSON in a session temp file.

class ManageTodoListTool : public ITool
{
public:
    explicit ManageTodoListTool(const QString &todoFile = {});

    void setTodoFile(const QString &path) { m_todoFile = path; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_todoFile;
};

// ── ReadProjectStructureTool ──────────────────────────────────────────────────
// Returns a depth-limited recursive tree of the workspace directory.

class ReadProjectStructureTool : public ITool
{
public:
    explicit ReadProjectStructureTool(const QString &workspaceRoot = {});

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    void listDir(const QDir &dir, int depth, int maxDepth, QStringList &output) const;

    QString m_workspaceRoot;
};
