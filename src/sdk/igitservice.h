#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// ── Git Service ──────────────────────────────────────────────────────────────
//
// Stable SDK interface for read-only git operations.
// Write operations (stage, commit) require PluginPermission::GitWrite.

class IGitService
{
public:
    virtual ~IGitService() = default;

    /// Whether the workspace is a git repository.
    virtual bool isGitRepo() const = 0;

    /// Current branch name.
    virtual QString currentBranch() const = 0;

    /// Working tree diff (unstaged changes). If staged is true, returns staged diff.
    virtual QString diff(bool staged = false) const = 0;

    /// List of changed files (relative paths).
    virtual QStringList changedFiles() const = 0;

    /// List of local branch names.
    virtual QStringList branches() const = 0;
};
