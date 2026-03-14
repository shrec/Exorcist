#pragma once

#include "../itool.h"

// ── ScratchpadTool ──────────────────────────────────────────────────────────
// Project-linked notes system that lives OUTSIDE the repo but is associated
// with it. Solves the "work notes polluting the repo" problem.
//
// Storage: .exorcist/scratchpad/ in workspace root (gitignored)
// Or: AppData/exorcist/scratchpad/<project-hash>/ for portable notes
//
// Notes are Markdown files organized by topic. Both the human developer and
// the AI agent can create, read, update, and search notes. Think of it as a
// project-specific wiki/notebook that never enters version control.

class ScratchpadTool : public ITool
{
public:
    explicit ScratchpadTool(const QString &scratchpadDir)
        : m_dir(scratchpadDir) {}

    void setScratchpadDir(const QString &dir) { m_dir = dir; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_dir;

    void ensureDir();
    static QString sanitizeTitle(const QString &title);
    QString notePath(const QString &title) const;
    static QString buildFrontMatter(const QJsonArray &tags);
    static QStringList parseTags(const QString &content);
    static QString readFile(const QString &path);

    ToolExecResult doWrite(const QJsonObject &args);
    ToolExecResult doAppend(const QJsonObject &args);
    ToolExecResult doRead(const QJsonObject &args);
    ToolExecResult doList(const QJsonObject &args);
    ToolExecResult doSearch(const QJsonObject &args);
    ToolExecResult doDelete(const QJsonObject &args);
    ToolExecResult doTags();
};
