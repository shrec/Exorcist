#pragma once

#include <functional>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QString>
#include <QTextStream>

#include "../itool.h"
#include "../../aiinterface.h"

// ─────────────────────────────────────────────────────────────────────────────
// ReplaceStringTool — replace_string_in_file
//
// Replaces ALL occurrences of old_string with new_string in a file.
// Falls back to a single-occurrence replace if old_string is unique.
// ─────────────────────────────────────────────────────────────────────────────

class ReplaceStringTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        QJsonObject props;
        props[QLatin1String("path")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("Absolute path to the file to edit")}
        };
        props[QLatin1String("old_string")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("Exact string to replace (must be unique in the file)")}
        };
        props[QLatin1String("new_string")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("Replacement string")}
        };

        ToolSpec s;
        s.name        = QStringLiteral("replace_string_in_file");
        s.description = QStringLiteral(
            "Replace a specific string in a file with a new string. "
            "The old_string must appear at least once in the file.");
        s.inputSchema = QJsonObject{
            {QLatin1String("type"),       QLatin1String("object")},
            {QLatin1String("properties"), props},
            {QLatin1String("required"),   QJsonArray{QLatin1String("path"),
                                                     QLatin1String("old_string"),
                                                     QLatin1String("new_string")}}
        };
        s.permission  = AgentToolPermission::SafeMutate;
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString path      = args[QLatin1String("path")].toString();
        const QString oldStr    = args[QLatin1String("old_string")].toString();
        const QString newStr    = args[QLatin1String("new_string")].toString();

        if (path.isEmpty() || oldStr.isEmpty()) {
            return {false, {}, {}, QStringLiteral("path and old_string are required")};
        }

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {false, {}, {}, QStringLiteral("Cannot read file: %1").arg(path)};
        }
        QString content = QTextStream(&f).readAll();
        f.close();

        if (!content.contains(oldStr)) {
            return {false, {}, {},
                    QStringLiteral("old_string not found in file: %1").arg(path)};
        }

        const int count = content.count(oldStr);
        content.replace(oldStr, newStr);

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return {false, {}, {}, QStringLiteral("Cannot write file: %1").arg(path)};
        }
        QTextStream(&f) << content;
        f.close();

        ToolExecResult r;
        r.ok          = true;
        r.textContent = QStringLiteral("Replaced %1 occurrence(s) in %2")
                         .arg(count).arg(QFileInfo(path).fileName());
        return r;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// GetErrorsTool — get_errors
//
// Returns current diagnostics (errors/warnings) from the active file or
// the entire workspace. Uses a callback provided at construction.
// ─────────────────────────────────────────────────────────────────────────────

class GetErrorsTool : public ITool
{
public:
    using DiagnosticsGetter = std::function<QList<AgentDiagnostic>()>;

    explicit GetErrorsTool(DiagnosticsGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("get_errors");
        s.description = QStringLiteral(
            "Get all current errors and warnings (diagnostics) from the workspace. "
            "Returns file path, line, column, message and severity for each issue.");
        s.inputSchema = QJsonObject{
            {QLatin1String("type"),       QLatin1String("object")},
            {QLatin1String("properties"), QJsonObject{}},
            {QLatin1String("required"),   QJsonArray{}}
        };
        s.permission  = AgentToolPermission::ReadOnly;
        return s;
    }

    ToolExecResult invoke(const QJsonObject &) override
    {
        const auto diags = m_getter ? m_getter() : QList<AgentDiagnostic>{};

        QJsonArray arr;
        QStringList lines;
        for (const AgentDiagnostic &d : diags) {
            const QString sev = (d.severity == AgentDiagnostic::Severity::Error)   ? QStringLiteral("error")
                              : (d.severity == AgentDiagnostic::Severity::Warning) ? QStringLiteral("warning")
                                                                                   : QStringLiteral("info");
            QJsonObject obj;
            obj[QLatin1String("file")]     = d.filePath;
            obj[QLatin1String("line")]     = d.line + 1;  // 1-based for readability
            obj[QLatin1String("column")]   = d.column + 1;
            obj[QLatin1String("message")]  = d.message;
            obj[QLatin1String("severity")] = sev;
            arr.append(obj);
            lines << QStringLiteral("[%1] %2:%3:%4 %5")
                     .arg(sev, QFileInfo(d.filePath).fileName())
                     .arg(d.line + 1).arg(d.column + 1)
                     .arg(d.message);
        }

        ToolExecResult r;
        r.ok          = true;
        r.data        = {{QLatin1String("diagnostics"), arr}};
        r.textContent = lines.isEmpty()
            ? QStringLiteral("No errors or warnings found.")
            : lines.join('\n');
        return r;
    }

private:
    DiagnosticsGetter m_getter;
};

// ─────────────────────────────────────────────────────────────────────────────
// GetChangedFilesTool — get_changed_files
//
// Returns git status (changed files) using a callback.
// ─────────────────────────────────────────────────────────────────────────────

class GetChangedFilesTool : public ITool
{
public:
    using StatusGetter = std::function<QHash<QString, QString>()>;  // absPath → XY

    explicit GetChangedFilesTool(StatusGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("get_changed_files");
        s.description = QStringLiteral(
            "Get all files changed in the current git working tree. "
            "Returns status (M=modified, A=added, D=deleted, ?=untracked) "
            "and file paths.");
        s.inputSchema = QJsonObject{
            {QLatin1String("type"),       QLatin1String("object")},
            {QLatin1String("properties"), QJsonObject{}},
            {QLatin1String("required"),   QJsonArray{}}
        };
        s.permission  = AgentToolPermission::ReadOnly;
        return s;
    }

    ToolExecResult invoke(const QJsonObject &) override
    {
        const auto statuses = m_getter ? m_getter() : QHash<QString, QString>{};

        QJsonArray arr;
        QStringList lines;
        for (auto it = statuses.cbegin(); it != statuses.cend(); ++it) {
            const QString &path = it.key();
            const QString &xy   = it.value();

            const QChar x = xy.size() > 0 ? xy[0] : QChar(' ');
            const QChar y = xy.size() > 1 ? xy[1] : QChar(' ');
            const QChar ch = (x != ' ' && x != '?') ? x : y;

            const QString status = (ch == 'M') ? QStringLiteral("modified")
                                 : (ch == 'A') ? QStringLiteral("added")
                                 : (ch == 'D') ? QStringLiteral("deleted")
                                 : (ch == 'R') ? QStringLiteral("renamed")
                                               : QStringLiteral("untracked");
            QJsonObject obj;
            obj[QLatin1String("status")] = status;
            obj[QLatin1String("path")]   = path;
            arr.append(obj);
            lines << QStringLiteral("[%1] %2").arg(status, path);
        }

        ToolExecResult r;
        r.ok          = true;
        r.data        = {{QLatin1String("files"), arr}};
        r.textContent = lines.isEmpty()
            ? QStringLiteral("No changed files (clean working tree).")
            : lines.join('\n');
        return r;
    }

private:
    StatusGetter m_getter;
};

// ─────────────────────────────────────────────────────────────────────────────
// FetchWebpageTool — fetch_webpage
//
// Fetches the content of a URL using curl (available on all platforms).
// Returns the text content (HTML stripped to plain text).
// ─────────────────────────────────────────────────────────────────────────────

class FetchWebpageTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        QJsonObject props;
        props[QLatin1String("url")] = QJsonObject{
            {QLatin1String("type"),        QLatin1String("string")},
            {QLatin1String("description"), QLatin1String("URL to fetch (https recommended)")}
        };

        ToolSpec s;
        s.name        = QStringLiteral("fetch_webpage");
        s.description = QStringLiteral(
            "Fetch the content of a web page. Returns the raw text content. "
            "Useful for reading documentation, READMEs, or API references online.");
        s.inputSchema = QJsonObject{
            {QLatin1String("type"),       QLatin1String("object")},
            {QLatin1String("properties"), props},
            {QLatin1String("required"),   QJsonArray{QLatin1String("url")}}
        };
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 15000;
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString url = args[QLatin1String("url")].toString().trimmed();
        if (url.isEmpty())
            return {false, {}, {}, QStringLiteral("url is required")};

        // Only allow http/https
        if (!url.startsWith(QStringLiteral("http://")) &&
            !url.startsWith(QStringLiteral("https://")))
            return {false, {}, {}, QStringLiteral("Only http/https URLs are supported")};

        QProcess proc;
        proc.start(QStringLiteral("curl"),
                   {QStringLiteral("-L"),          // follow redirects
                    QStringLiteral("--silent"),
                    QStringLiteral("--max-time"), QStringLiteral("10"),
                    QStringLiteral("--user-agent"), QStringLiteral("Exorcist/1.0"),
                    QStringLiteral("--max-filesize"), QStringLiteral("1048576"), // 1 MB
                    url});

        if (!proc.waitForFinished(12000)) {
            proc.kill();
            return {false, {}, {}, QStringLiteral("Request timed out")};
        }
        if (proc.exitCode() != 0) {
            return {false, {}, {},
                    QStringLiteral("curl error: %1")
                        .arg(QString::fromUtf8(proc.readAllStandardError()).trimmed())};
        }

        const QString body = QString::fromUtf8(proc.readAllStandardOutput());
        // Truncate to 50 KB to stay within context window budget
        constexpr int kMaxLen = 50000;
        const QString truncated = body.size() > kMaxLen
            ? body.left(kMaxLen) + QStringLiteral("\n[... truncated]")
            : body;

        ToolExecResult r;
        r.ok          = true;
        r.textContent = truncated;
        return r;
    }
};
