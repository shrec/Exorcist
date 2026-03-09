#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// ── Workspace Service ────────────────────────────────────────────────────────
//
// Stable SDK interface for workspace / filesystem operations.
// Wraps IFileSystem and project manager behind a plugin-safe API.

class IWorkspaceService
{
public:
    virtual ~IWorkspaceService() = default;

    /// Absolute path to the workspace root folder.
    virtual QString rootPath() const = 0;

    /// List of currently open file paths (absolute).
    virtual QStringList openFiles() const = 0;

    /// Read a file's content. Returns empty QByteArray on error.
    virtual QByteArray readFile(const QString &path) const = 0;

    /// Write data to a file. Returns true on success.
    virtual bool writeFile(const QString &path, const QByteArray &data) = 0;

    /// List files/directories under a relative or absolute path.
    virtual QStringList listDirectory(const QString &path) const = 0;

    /// Check whether a file or directory exists.
    virtual bool exists(const QString &path) const = 0;
};
