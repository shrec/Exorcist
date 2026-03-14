#pragma once

#include "../itool.h"
#include "../../core/ifilesystem.h"

// Shared path-resolution helper for all filesystem tools.
// Turns a workspace-relative path into an absolute one.
namespace FsToolUtil {
QString resolve(const QString &path, const QString &wsRoot);
} // namespace FsToolUtil

// ── ReadFileTool ──────────────────────────────────────────────────────────────
// Reads the content of a file and returns it to the model.

class ReadFileTool : public ITool
{
public:
    explicit ReadFileTool(IFileSystem *fs);

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};

// ── ListFilesTool ─────────────────────────────────────────────────────────────
// Lists the files in a directory.

class ListFilesTool : public ITool
{
public:
    explicit ListFilesTool(IFileSystem *fs);

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};

// ── WriteFileTool ─────────────────────────────────────────────────────────────
// Writes content to a file (create or overwrite).

class WriteFileTool : public ITool
{
public:
    explicit WriteFileTool(IFileSystem *fs);

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};
