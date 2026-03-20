#pragma once

#include "../itool.h"
#include "../../core/ifilesystem.h"

#include <functional>
#include <memory>

#include <QHash>

struct TransactionStore;

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
    void setTransactionStore(std::shared_ptr<TransactionStore> store) { m_txStore = std::move(store); }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
    std::shared_ptr<TransactionStore> m_txStore;
};

// ── OverwriteFileTool ─────────────────────────────────────────────────────────
// Explicitly overwrites an existing file. Same engine as WriteFileTool but
// registered as "write_file" with a description that allows overwriting.

class OverwriteFileTool : public ITool
{
public:
    explicit OverwriteFileTool(IFileSystem *fs);

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }
    void setTransactionStore(std::shared_ptr<TransactionStore> store) { m_txStore = std::move(store); }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
    std::shared_ptr<TransactionStore> m_txStore;
};

// ── UndoFileEditTool ──────────────────────────────────────────────────────────
// Rolls back file changes made during the current agent session by restoring
// pre-edit snapshots. Can undo a single file or all changed files.

class UndoFileEditTool : public ITool
{
public:
    struct UndoResult {
        bool    success;
        int     filesRestored;
        QString detail;
    };
    using SnapshotGetter = std::function<QHash<QString, QString>()>;
    using FileRestorer   = std::function<UndoResult(const QString &path)>;

    UndoFileEditTool(SnapshotGetter snapshotGetter, FileRestorer restorer);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SnapshotGetter m_snapshotGetter;
    FileRestorer   m_restorer;
};
