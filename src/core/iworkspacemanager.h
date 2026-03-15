#pragma once

/// IWorkspaceManager — Abstract interface for workspace/folder management.
///
/// This is the core "simple editor" interface: open a folder, browse files,
/// manage the project tree. Like VS Code's basic folder experience.
/// Plugins obtain this via ServiceRegistry under key "workspaceManager".
///
/// Language plugins enhance this with LSP, build systems, etc.
/// But the core workspace manager works standalone — open any folder,
/// browse any source, edit any file.

#include <QString>
#include <QStringList>

class IWorkspaceManager
{
public:
    virtual ~IWorkspaceManager() = default;

    // ── Folder operations ───────────────────────────────────────────

    /// Open a folder as the workspace root. If path is empty, show dialog.
    virtual void openFolder(const QString &path = {}) = 0;

    /// Close the current folder.
    virtual void closeFolder() = 0;

    /// Current workspace root path. Empty if no folder is open.
    virtual QString rootPath() const = 0;

    /// Whether a folder is currently open.
    virtual bool hasFolder() const = 0;

    // ── File operations ─────────────────────────────────────────────

    /// Open a file in the editor.
    virtual void openFile(const QString &path) = 0;

    /// Open a file at a specific position.
    virtual void openFile(const QString &path, int line, int column = 0) = 0;

    /// Get open file paths (all tabs).
    virtual QStringList openFiles() const = 0;

    /// Current active file path.
    virtual QString activeFile() const = 0;

    // ── Recent items ────────────────────────────────────────────────

    /// Recent folders.
    virtual QStringList recentFolders() const = 0;

    /// Recent files.
    virtual QStringList recentFiles() const = 0;

    // ── Solution/Project (optional) ──────────────────────────────────

    /// Whether a solution/project file is loaded.
    virtual bool hasSolution() const = 0;

    /// Current solution file path.
    virtual QString solutionPath() const = 0;
};
