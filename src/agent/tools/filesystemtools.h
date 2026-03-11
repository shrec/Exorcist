#pragma once

#include "../itool.h"
#include "../../core/ifilesystem.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>

// Shared path-resolution helper for all filesystem tools.
// Turns a workspace-relative path into an absolute one.
namespace FsToolUtil {
inline QString resolve(const QString &path, const QString &wsRoot)
{
    if (path.isEmpty() || QFileInfo(path).isAbsolute() || wsRoot.isEmpty())
        return path;
    return QDir(wsRoot).absoluteFilePath(path);
}
} // namespace FsToolUtil

// ── ReadFileTool ──────────────────────────────────────────────────────────────
// Reads the content of a file and returns it to the model.

class ReadFileTool : public ITool
{
public:
    explicit ReadFileTool(IFileSystem *fs) : m_fs(fs) {}

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("read_file");
        s.description = QStringLiteral(
            "Read the contents of a file. Returns the file content as text. "
            "Use this to understand code before making changes.");
        s.permission  = AgentToolPermission::ReadOnly;
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

    ToolExecResult invoke(const QJsonObject &args) override
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

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};

// ── ListFilesTool ─────────────────────────────────────────────────────────────
// Lists the files in a directory.

class ListFilesTool : public ITool
{
public:
    explicit ListFilesTool(IFileSystem *fs) : m_fs(fs) {}

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("list_dir");
        s.description = QStringLiteral(
            "List the contents of a directory. Returns a newline-separated list "
            "of names. If the name ends in /, it's a folder, otherwise a file.");
        s.permission  = AgentToolPermission::ReadOnly;
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

    ToolExecResult invoke(const QJsonObject &args) override
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

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};

// ── WriteFileTool ─────────────────────────────────────────────────────────────
// Writes content to a file (create or overwrite).

class WriteFileTool : public ITool
{
public:
    explicit WriteFileTool(IFileSystem *fs) : m_fs(fs) {}

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
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

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString rawPath = args[QLatin1String("path")].toString();
        const QString content = args[QLatin1String("content")].toString();
        if (rawPath.isEmpty())
            return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

        const QString path = FsToolUtil::resolve(rawPath, m_workspaceRoot);

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

private:
    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};
