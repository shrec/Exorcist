#pragma once

#include <QString>

#include "../itool.h"

// ─────────────────────────────────────────────────────────────────────────────
// MemoryTool — persistent memory system for the AI agent
//
// Operations: view, create, str_replace, insert, delete, rename
// Scopes:
//   /memories/         — user-level (persistent across workspaces)
//   /memories/session/  — conversation-scoped
//   /memories/repo/     — workspace-scoped
// ─────────────────────────────────────────────────────────────────────────────

class MemoryTool : public ITool
{
public:
    explicit MemoryTool(const QString &baseDir);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static ToolExecResult ok(const QString &text);
    static ToolExecResult fail(const QString &msg);

    QString resolvePath(const QString &memPath) const;

    ToolExecResult doView(const QString &path) const;
    ToolExecResult doCreate(const QString &path, const QJsonObject &args) const;
    ToolExecResult doStrReplace(const QString &path, const QJsonObject &args) const;
    ToolExecResult doInsert(const QString &path, const QJsonObject &args) const;
    ToolExecResult doDelete(const QString &path) const;
    ToolExecResult doRename(const QString &path, const QJsonObject &args) const;

    QString m_baseDir;
};
