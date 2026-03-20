#include "filesystemtools.h"
#include "transactiontool.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

// ── FsToolUtil ───────────────────────────────────────────────────────────────

namespace FsToolUtil {

QString resolve(const QString &path, const QString &wsRoot)
{
    if (path.isEmpty() || QFileInfo(path).isAbsolute() || wsRoot.isEmpty())
        return path;
    return QDir(wsRoot).absoluteFilePath(path);
}

} // namespace FsToolUtil

// ── ReadFileTool ──────────────────────────────────────────────────────────────

ReadFileTool::ReadFileTool(IFileSystem *fs)
    : m_fs(fs)
{
}

ToolSpec ReadFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("read_file");
    s.description = QStringLiteral(
        "Read the contents of a file. Returns the file content as text. "
        "Use this to understand code before making changes.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.parallelSafe = true;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Absolute or workspace-relative path to the file.")}
            }},
            {QStringLiteral("startLine"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Optional 1-based start line to read from.")}
            }},
            {QStringLiteral("endLine"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Optional 1-based end line (inclusive).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}
    };
    return s;
}

ToolExecResult ReadFileTool::invoke(const QJsonObject &args)
{
    const QString rawPath = args[QLatin1String("path")].toString();
    if (rawPath.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

    const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);

    // On Windows, opening a directory with QFile gives "Access is denied".
    // Detect it early and return a useful message.
    if (QFileInfo(path).isDir())
        return {false, {}, {},
            QStringLiteral("'%1' is a directory — use list_dir to list it.").arg(path)};

    if (!m_fs->exists(path))
        return {false, {}, {}, QStringLiteral("File not found: %1").arg(path)};

    QString error;
    const QString content = m_fs->readTextFile(path, &error);
    if (!error.isEmpty())
        return {false, {}, {}, error};

    // Optional line range
    const int startLine = args[QLatin1String("startLine")].toInt(0);
    const int endLine   = args[QLatin1String("endLine")].toInt(0);

    QString result = content;
    if (startLine > 0 || endLine > 0) {
        const QStringList lines = content.split(QLatin1Char('\n'));
        const int from = qMax(0, startLine - 1);
        const int to   = endLine > 0 ? qMin(lines.size(), endLine) : lines.size();
        QStringList slice;
        for (int i = from; i < to; ++i)
            slice.append(lines[i]);
        result = slice.join(QLatin1Char('\n'));
    }

    return {true, {}, result, {}};
}

// ── ListFilesTool ─────────────────────────────────────────────────────────────

ListFilesTool::ListFilesTool(IFileSystem *fs)
    : m_fs(fs)
{
}

ToolSpec ListFilesTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("list_dir");
    s.description = QStringLiteral(
        "List the contents of a directory. Returns a newline-separated list "
        "of names. If the name ends in /, it's a folder, otherwise a file.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.parallelSafe = true;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Directory path to list.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}
    };
    return s;
}

ToolExecResult ListFilesTool::invoke(const QJsonObject &args)
{
    const QString rawPath = args[QLatin1String("path")].toString();
    if (rawPath.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

    const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);

    QString error;
    const QStringList entries = m_fs->listDir(path, &error);
    if (!error.isEmpty())
        return {false, {}, {}, error};

    // Append '/' to directory entries so the model can tell them from files
    QStringList labeled;
    for (const QString &e : entries) {
        const QString full = path + QLatin1Char('/') + e;
        labeled << (QFileInfo(full).isDir() ? e + QLatin1Char('/') : e);
    }

    return {true, {}, labeled.join(QLatin1Char('\n')), {}};
}

// ── WriteFileTool ─────────────────────────────────────────────────────────────

WriteFileTool::WriteFileTool(IFileSystem *fs)
    : m_fs(fs)
{
}

ToolSpec WriteFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("create_file");
    s.description = QStringLiteral(
        "This is a tool for creating a new file in the workspace. "
        "The file will be created with the specified content. "
        "The directory will be created if it does not already exist. "
        "Never use this tool to edit a file that already exists — "
        "use replace_string_in_file or insert_edit_into_file instead.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("File path to write to.")}
            }},
            {QStringLiteral("content"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The content to write to the file.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("path"), QStringLiteral("content")}}
    };
    return s;
}

ToolExecResult WriteFileTool::invoke(const QJsonObject &args)
{
    const QString rawPath = args[QLatin1String("path")].toString();
    const QString content = args[QLatin1String("content")].toString();
    if (rawPath.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

    const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);

    // Snapshot for transaction before first write
    if (m_txStore && m_txStore->active) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            m_txStore->recordIfNew(path, f.readAll());
            f.close();
        } else {
            // File doesn't exist yet — record empty so rollback deletes it
            m_txStore->recordIfNew(path, QByteArray{});
        }
    }

    // Create parent directories if they don't exist
    QDir().mkpath(QFileInfo(path).absolutePath());

    QString error;
    if (!m_fs->writeTextFile(path, content, &error))
        return {false, {}, {}, error};

    QJsonObject data;
    data[QStringLiteral("filePath")] = path;

    return {true, data,
            QStringLiteral("Successfully wrote %1 bytes to %2")
                .arg(content.size()).arg(path), {}};
}

// ── OverwriteFileTool ─────────────────────────────────────────────────────────

OverwriteFileTool::OverwriteFileTool(IFileSystem *fs)
    : m_fs(fs)
{
}

ToolSpec OverwriteFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("write_file");
    s.description = QStringLiteral(
        "Write content to a file, creating it if it does not exist or "
        "overwriting it entirely if it does. Use this when you need to "
        "replace the full contents of an existing file, or when "
        "create_file is inappropriate because the file already exists. "
        "The directory will be created if it does not already exist.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Absolute or workspace-relative file path.")}
            }},
            {QStringLiteral("content"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The full content to write to the file.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("path"), QStringLiteral("content")}}
    };
    return s;
}

ToolExecResult OverwriteFileTool::invoke(const QJsonObject &args)
{
    const QString rawPath = args[QLatin1String("path")].toString();
    const QString content = args[QLatin1String("content")].toString();
    if (rawPath.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

    const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);

    // Snapshot for transaction before first write
    if (m_txStore && m_txStore->active) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) {
            m_txStore->recordIfNew(path, f.readAll());
            f.close();
        } else {
            m_txStore->recordIfNew(path, QByteArray{});
        }
    }

    QDir().mkpath(QFileInfo(path).absolutePath());

    const bool existed = QFileInfo::exists(path);

    QString error;
    if (!m_fs->writeTextFile(path, content, &error))
        return {false, {}, {}, error};

    QJsonObject data;
    data[QStringLiteral("filePath")] = path;
    data[QStringLiteral("overwritten")] = existed;

    const QString verb = existed ? QStringLiteral("Overwrote")
                                 : QStringLiteral("Created");
    return {true, data,
            QStringLiteral("%1 %2 (%3 bytes)")
                .arg(verb, path).arg(content.size()), {}};
}

// ── UndoFileEditTool ──────────────────────────────────────────────────────────

UndoFileEditTool::UndoFileEditTool(SnapshotGetter snapshotGetter,
                                     FileRestorer restorer)
    : m_snapshotGetter(std::move(snapshotGetter))
    , m_restorer(std::move(restorer))
{
}

ToolSpec UndoFileEditTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("undo_file_edit");
    s.description = QStringLiteral(
        "Undo file changes made during this agent session. Restores files "
        "to their state before the agent modified them. Call with a 'path' "
        "to undo a single file, or without 'path' to list all changed files. "
        "Use 'all' parameter set to true to restore ALL changed files at once.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 15000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("File path to restore. Omit to list changed files.")}
            }},
            {QStringLiteral("all"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("If true, restore ALL changed files to their pre-edit state.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult UndoFileEditTool::invoke(const QJsonObject &args)
{
    const auto snapshots = m_snapshotGetter();
    if (snapshots.isEmpty())
        return {true, {}, QStringLiteral("No file changes recorded in this session."), {}};

    const QString rawPath = args[QLatin1String("path")].toString();
    const bool    undoAll = args[QLatin1String("all")].toBool(false);

    // List mode — no path, no "all"
    if (rawPath.isEmpty() && !undoAll) {
        QStringList files;
        for (auto it = snapshots.cbegin(); it != snapshots.cend(); ++it)
            files.append(it.key());
        files.sort();
        return {true, {},
                QStringLiteral("Changed files in this session (%1):\n%2")
                    .arg(files.size()).arg(files.join(QLatin1Char('\n'))), {}};
    }

    // Undo all
    if (undoAll) {
        int restored = 0;
        QStringList errors;
        for (auto it = snapshots.cbegin(); it != snapshots.cend(); ++it) {
            auto result = m_restorer(it.key());
            if (result.success)
                ++restored;
            else
                errors.append(QStringLiteral("%1: %2").arg(it.key(), result.detail));
        }
        if (!errors.isEmpty())
            return {false, {},
                    QStringLiteral("Restored %1 files, %2 failures:\n%3")
                        .arg(restored).arg(errors.size())
                        .arg(errors.join(QLatin1Char('\n'))), {}};
        return {true, {},
                QStringLiteral("Restored all %1 changed files to their pre-edit state.")
                    .arg(restored), {}};
    }

    // Undo single file
    if (!snapshots.contains(rawPath))
        return {false, {}, {},
                QStringLiteral("No snapshot recorded for '%1'. "
                               "Call without 'path' to list changed files.").arg(rawPath)};

    auto result = m_restorer(rawPath);
    if (!result.success)
        return {false, {}, {}, result.detail};

    return {true, {},
            QStringLiteral("Restored '%1' to its pre-edit state.").arg(rawPath), {}};
}
