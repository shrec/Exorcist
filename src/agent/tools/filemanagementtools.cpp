#include "filemanagementtools.h"
#include "filesystemtools.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>

// ── DeleteFileTool ────────────────────────────────────────────────────────────

ToolSpec DeleteFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("delete_file");
    s.description = QStringLiteral(
        "Delete a file or empty directory from the workspace. "
        "For safety, non-empty directories cannot be deleted. "
        "Use with caution — this action is irreversible.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Absolute or workspace-relative path to the file or empty directory to delete.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}
    };
    return s;
}

ToolExecResult DeleteFileTool::invoke(const QJsonObject &args)
{
    const QString rawPath = args[QLatin1String("path")].toString();
    if (rawPath.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

    const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);
    const QFileInfo fi(path);

    if (!fi.exists())
        return {false, {}, {}, QStringLiteral("Path does not exist: %1").arg(path)};

    if (fi.isDir()) {
        QDir dir(path);
        if (!dir.isEmpty())
            return {false, {}, {},
                QStringLiteral("Directory is not empty: %1. Remove contents first.").arg(path)};
        if (!dir.rmdir(QStringLiteral(".")))
            return {false, {}, {}, QStringLiteral("Failed to remove directory: %1").arg(path)};
        return {true, {}, QStringLiteral("Deleted directory: %1").arg(path), {}};
    }

    if (!QFile::remove(path))
        return {false, {}, {}, QStringLiteral("Failed to delete file: %1").arg(path)};

    return {true, {}, QStringLiteral("Deleted file: %1").arg(path), {}};
}

// ── RenameFileTool ────────────────────────────────────────────────────────────

ToolSpec RenameFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("rename_file");
    s.description = QStringLiteral(
        "Rename or move a file or directory. Creates intermediate "
        "directories as needed. Can be used for both renaming and moving.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("oldPath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Current path of the file or directory.")}
            }},
            {QStringLiteral("newPath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("New path (can be in a different directory to move).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("oldPath"), QStringLiteral("newPath")}}
    };
    return s;
}

ToolExecResult RenameFileTool::invoke(const QJsonObject &args)
{
    const QString rawOld = args[QLatin1String("oldPath")].toString();
    const QString rawNew = args[QLatin1String("newPath")].toString();
    if (rawOld.isEmpty() || rawNew.isEmpty())
        return {false, {}, {},
            QStringLiteral("Both 'oldPath' and 'newPath' are required.")};

    const QString oldPath = FsToolUtil::resolve(rawOld, m_workspaceRoot);
    const QString newPath = FsToolUtil::resolve(rawNew, m_workspaceRoot);

    if (!QFileInfo::exists(oldPath))
        return {false, {}, {}, QStringLiteral("Source does not exist: %1").arg(oldPath)};
    if (QFileInfo::exists(newPath))
        return {false, {}, {},
            QStringLiteral("Destination already exists: %1").arg(newPath)};

    // Ensure parent directory of destination exists
    QDir().mkpath(QFileInfo(newPath).absolutePath());

    if (!QFile::rename(oldPath, newPath))
        return {false, {}, {},
            QStringLiteral("Failed to rename '%1' to '%2'").arg(oldPath, newPath)};

    return {true, {},
            QStringLiteral("Renamed '%1' → '%2'").arg(oldPath, newPath), {}};
}

// ── CopyFileTool ──────────────────────────────────────────────────────────────

ToolSpec CopyFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("copy_file");
    s.description = QStringLiteral(
        "Copy a file to a new location. Creates intermediate directories "
        "as needed. Does not overwrite existing files.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("source"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the source file.")}
            }},
            {QStringLiteral("destination"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path for the copy.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("source"), QStringLiteral("destination")}}
    };
    return s;
}

ToolExecResult CopyFileTool::invoke(const QJsonObject &args)
{
    const QString rawSrc = args[QLatin1String("source")].toString();
    const QString rawDst = args[QLatin1String("destination")].toString();
    if (rawSrc.isEmpty() || rawDst.isEmpty())
        return {false, {}, {},
            QStringLiteral("Both 'source' and 'destination' are required.")};

    const QString src = FsToolUtil::resolve(rawSrc, m_workspaceRoot);
    const QString dst = FsToolUtil::resolve(rawDst, m_workspaceRoot);

    if (!QFileInfo::exists(src))
        return {false, {}, {}, QStringLiteral("Source does not exist: %1").arg(src)};
    if (QFileInfo(src).isDir())
        return {false, {}, {},
            QStringLiteral("Cannot copy directories — only files.")};
    if (QFileInfo::exists(dst))
        return {false, {}, {},
            QStringLiteral("Destination already exists: %1").arg(dst)};

    QDir().mkpath(QFileInfo(dst).absolutePath());

    if (!QFile::copy(src, dst))
        return {false, {}, {},
            QStringLiteral("Failed to copy '%1' to '%2'").arg(src, dst)};

    return {true, {},
            QStringLiteral("Copied '%1' → '%2'").arg(src, dst), {}};
}

// ── FileWatcherTool ───────────────────────────────────────────────────────────

ToolSpec FileWatcherTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("watch_files");
    s.description = QStringLiteral(
        "Watch files or directories for changes. Supports operations:\n"
        "  'watch' — start watching a path for changes\n"
        "  'unwatch' — stop watching a path\n"
        "  'poll' — get recent changes since last poll\n"
        "  'list' — list currently watched paths\n"
        "Changes detected: file modified, created, or removed.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("One of: watch, unwatch, poll, list")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("watch"), QStringLiteral("unwatch"),
                    QStringLiteral("poll"), QStringLiteral("list")}}
            }},
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("File or directory path to watch/unwatch.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

ToolExecResult FileWatcherTool::invoke(const QJsonObject &args)
{
    const QString op = args[QLatin1String("operation")].toString();

    if (op == QLatin1String("watch")) {
        const QString rawPath = args[QLatin1String("path")].toString();
        if (rawPath.isEmpty())
            return {false, {}, {}, QStringLiteral("'path' required for 'watch'.")};
        const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);
        if (!QFileInfo::exists(path))
            return {false, {}, {}, QStringLiteral("Path does not exist: %1").arg(path)};

        ensureWatcher();
        if (QFileInfo(path).isDir())
            m_watcher->addPath(path);
        else
            m_watcher->addPath(path);

        return {true, {}, QStringLiteral("Now watching: %1").arg(path), {}};
    }

    if (op == QLatin1String("unwatch")) {
        const QString rawPath = args[QLatin1String("path")].toString();
        if (rawPath.isEmpty())
            return {false, {}, {}, QStringLiteral("'path' required for 'unwatch'.")};
        const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);
        if (m_watcher)
            m_watcher->removePath(path);
        return {true, {}, QStringLiteral("Stopped watching: %1").arg(path), {}};
    }

    if (op == QLatin1String("poll")) {
        QStringList changes;
        changes.swap(m_changes);
        if (changes.isEmpty())
            return {true, {}, QStringLiteral("No changes detected."), {}};
        return {true, {}, changes.join(QLatin1Char('\n')), {}};
    }

    if (op == QLatin1String("list")) {
        if (!m_watcher)
            return {true, {}, QStringLiteral("No paths being watched."), {}};
        QStringList watched = m_watcher->files() + m_watcher->directories();
        if (watched.isEmpty())
            return {true, {}, QStringLiteral("No paths being watched."), {}};
        return {true, {}, watched.join(QLatin1Char('\n')), {}};
    }

    return {false, {}, {},
        QStringLiteral("Unknown operation: %1. Use: watch, unwatch, poll, list").arg(op)};
}

void FileWatcherTool::ensureWatcher()
{
    if (m_watcher) return;
    m_watcher = std::make_unique<QFileSystemWatcher>();
    QObject::connect(m_watcher.get(), &QFileSystemWatcher::fileChanged,
        [this](const QString &path) {
            m_changes.append(QStringLiteral("modified: %1").arg(path));
        });
    QObject::connect(m_watcher.get(), &QFileSystemWatcher::directoryChanged,
        [this](const QString &path) {
            m_changes.append(QStringLiteral("directory_changed: %1").arg(path));
        });
}
