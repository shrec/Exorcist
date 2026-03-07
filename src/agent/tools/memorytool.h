#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QTextStream>

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
    explicit MemoryTool(const QString &baseDir)
        : m_baseDir(baseDir)
    {
        QDir().mkpath(m_baseDir);
        QDir().mkpath(m_baseDir + QStringLiteral("/session"));
        QDir().mkpath(m_baseDir + QStringLiteral("/repo"));
    }

    ToolSpec spec() const override
    {
        QJsonObject props;
        props[QLatin1String("command")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("enum"),        QJsonArray{
                QLatin1String("view"), QLatin1String("create"),
                QLatin1String("str_replace"), QLatin1String("insert"),
                QLatin1String("delete"), QLatin1String("rename")}},
            {QLatin1String("description"), QLatin1String("Operation to perform")}
        };
        props[QLatin1String("path")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String(
                "Path inside /memories/, e.g. /memories/notes.md")}
        };
        props[QLatin1String("file_text")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("Content for 'create' command")}
        };
        props[QLatin1String("old_str")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("String to replace for 'str_replace'")}
        };
        props[QLatin1String("new_str")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("Replacement string for 'str_replace'")}
        };
        props[QLatin1String("insert_line")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("integer")},
            {QLatin1String("description"), QLatin1String("0-based line number for 'insert'")}
        };
        props[QLatin1String("insert_text")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("Text to insert at the given line")}
        };
        props[QLatin1String("new_path")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("New path for 'rename'")}
        };

        ToolSpec s;
        s.name        = QStringLiteral("memory");
        s.description = QStringLiteral(
            "Manage persistent memory files. Supports view, create, str_replace, "
            "insert, delete, and rename operations on /memories/ paths.");
        s.inputSchema = QJsonObject{
            {QLatin1String("type"),       QLatin1String("object")},
            {QLatin1String("properties"), props},
            {QLatin1String("required"),   QJsonArray{QLatin1String("command")}}
        };
        s.permission = AgentToolPermission::SafeMutate;
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString command = args.value(QLatin1String("command")).toString();
        const QString path    = args.value(QLatin1String("path")).toString();

        if (command == QLatin1String("view"))        return doView(path);
        if (command == QLatin1String("create"))      return doCreate(path, args);
        if (command == QLatin1String("str_replace"))  return doStrReplace(path, args);
        if (command == QLatin1String("insert"))       return doInsert(path, args);
        if (command == QLatin1String("delete"))       return doDelete(path);
        if (command == QLatin1String("rename"))       return doRename(path, args);

        return fail(QStringLiteral("Unknown command: %1").arg(command));
    }

private:
    static ToolExecResult ok(const QString &text)
    {
        ToolExecResult r;
        r.ok = true;
        r.textContent = text;
        return r;
    }
    static ToolExecResult fail(const QString &msg)
    {
        return {false, {}, {}, msg};
    }

    QString resolvePath(const QString &memPath) const
    {
        QString rel = memPath;
        if (rel.startsWith(QLatin1String("/memories/")))
            rel = rel.mid(10);
        else if (rel.startsWith(QLatin1String("/memories")))
            rel = rel.mid(9);
        if (rel.startsWith(QLatin1Char('/')))
            rel = rel.mid(1);

        // Prevent path traversal
        if (rel.contains(QLatin1String("..")))
            return {};

        return rel.isEmpty() ? m_baseDir : (m_baseDir + QLatin1Char('/') + rel);
    }

    ToolExecResult doView(const QString &path) const
    {
        const QString resolved = resolvePath(path);
        if (resolved.isEmpty())
            return fail(QStringLiteral("Invalid path"));

        QFileInfo fi(resolved);
        if (!fi.exists())
            return fail(QStringLiteral("Path does not exist: %1").arg(path));

        if (fi.isDir()) {
            QDir dir(resolved);
            QStringList entries;
            for (const QFileInfo &entry : dir.entryInfoList(
                     QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name)) {
                entries << (entry.isDir()
                    ? entry.fileName() + QLatin1Char('/')
                    : entry.fileName());
            }
            return ok(entries.isEmpty()
                ? QStringLiteral("(empty directory)")
                : entries.join(QLatin1Char('\n')));
        }

        QFile f(resolved);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return fail(QStringLiteral("Cannot read file"));
        return ok(QString::fromUtf8(f.readAll()));
    }

    ToolExecResult doCreate(const QString &path, const QJsonObject &args) const
    {
        const QString resolved = resolvePath(path);
        if (resolved.isEmpty())
            return fail(QStringLiteral("Invalid path"));

        if (QFileInfo::exists(resolved))
            return fail(QStringLiteral("File already exists: %1").arg(path));

        QDir().mkpath(QFileInfo(resolved).absolutePath());

        QFile f(resolved);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return fail(QStringLiteral("Cannot create file"));

        const QString content = args.value(QLatin1String("file_text")).toString();
        f.write(content.toUtf8());
        return ok(QStringLiteral("Created %1").arg(path));
    }

    ToolExecResult doStrReplace(const QString &path, const QJsonObject &args) const
    {
        const QString resolved = resolvePath(path);
        if (resolved.isEmpty())
            return fail(QStringLiteral("Invalid path"));

        QFile f(resolved);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return fail(QStringLiteral("Cannot read file"));
        QString content = QString::fromUtf8(f.readAll());
        f.close();

        const QString oldStr = args.value(QLatin1String("old_str")).toString();
        const QString newStr = args.value(QLatin1String("new_str")).toString();

        if (content.count(oldStr) != 1)
            return fail(QStringLiteral(
                "old_str must appear exactly once (found %1 times)")
                .arg(content.count(oldStr)));

        content.replace(oldStr, newStr);

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return fail(QStringLiteral("Cannot write file"));
        f.write(content.toUtf8());
        return ok(QStringLiteral("Updated %1").arg(path));
    }

    ToolExecResult doInsert(const QString &path, const QJsonObject &args) const
    {
        const QString resolved = resolvePath(path);
        if (resolved.isEmpty())
            return fail(QStringLiteral("Invalid path"));

        QFile f(resolved);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            return fail(QStringLiteral("Cannot read file"));
        QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
        f.close();

        const int lineNum = args.value(QLatin1String("insert_line")).toInt(0);
        const QString text = args.value(QLatin1String("insert_text")).toString();

        const int idx = qBound(0, lineNum, lines.size());
        lines.insert(idx, text);

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            return fail(QStringLiteral("Cannot write file"));
        f.write(lines.join(QLatin1Char('\n')).toUtf8());
        return ok(QStringLiteral("Inserted at line %1 in %2").arg(idx).arg(path));
    }

    ToolExecResult doDelete(const QString &path) const
    {
        const QString resolved = resolvePath(path);
        if (resolved.isEmpty())
            return fail(QStringLiteral("Invalid path"));

        QFileInfo fi(resolved);
        if (!fi.exists())
            return fail(QStringLiteral("Path does not exist"));

        if (fi.isDir()) {
            QDir dir(resolved);
            if (!dir.removeRecursively())
                return fail(QStringLiteral("Cannot delete directory"));
        } else {
            if (!QFile::remove(resolved))
                return fail(QStringLiteral("Cannot delete file"));
        }
        return ok(QStringLiteral("Deleted %1").arg(path));
    }

    ToolExecResult doRename(const QString &path, const QJsonObject &args) const
    {
        const QString oldResolved = resolvePath(path);
        const QString newPath = args.value(QLatin1String("new_path")).toString();
        const QString newResolved = resolvePath(newPath);

        if (oldResolved.isEmpty() || newResolved.isEmpty())
            return fail(QStringLiteral("Invalid path"));
        if (!QFileInfo::exists(oldResolved))
            return fail(QStringLiteral("Source does not exist"));

        QDir().mkpath(QFileInfo(newResolved).absolutePath());
        if (!QFile::rename(oldResolved, newResolved))
            return fail(QStringLiteral("Rename failed"));
        return ok(QStringLiteral("Renamed %1 to %2").arg(path, newPath));
    }

    QString m_baseDir;
};
