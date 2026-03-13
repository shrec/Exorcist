#pragma once

#include <QString>
#include <QStringList>

// ── IFileWatcher ─────────────────────────────────────────────────────────────
//
// Abstract interface for file/directory change monitoring.
// Implementations must emit signals when watched paths change.
//
// The concrete implementation (QtFileWatcher) wraps QFileSystemWatcher.
// Tests and sandboxed environments can substitute stubs.

class IFileWatcher
{
public:
    virtual ~IFileWatcher() = default;

    virtual bool watchPath(const QString &path) = 0;
    virtual bool unwatchPath(const QString &path) = 0;
    virtual QStringList watchedFiles() const = 0;
    virtual QStringList watchedDirectories() const = 0;

    // Implementations must inherit QObject and emit:
    //   void fileChanged(const QString &path);
    //   void directoryChanged(const QString &path);
};
