#pragma once

#include "../itool.h"
#include "../../core/ifilesystem.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimeZone>

// ── JsonParseFormatTool ──────────────────────────────────────────────────────
// JSON parse, format, validate. Provides ex.json.decode() / ex.json.encode()
// functionality for the AI agent. Useful for data transformation.

class JsonParseFormatTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("json_parse_format");
        s.description = QStringLiteral(
            "Parse, format, or validate JSON data. Operations:\n"
            "  'parse' — parse JSON string, return pretty-printed\n"
            "  'format' — pretty-print / minify JSON\n"
            "  'validate' — check if JSON is valid\n"
            "  'query' — extract a value by JSON pointer (e.g. '/key/0/name')\n"
            "Use this for JSON processing without writing Lua scripts.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("One of: parse, format, validate, query")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("parse"), QStringLiteral("format"),
                        QStringLiteral("validate"), QStringLiteral("query")}}
                }},
                {QStringLiteral("json"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("JSON string to process.")}
                }},
                {QStringLiteral("pointer"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("JSON pointer for 'query' operation (e.g. '/data/0/name').")}
                }},
                {QStringLiteral("compact"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("If true, 'format' produces compact (minified) output. Default: false.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("operation"), QStringLiteral("json")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString op   = args[QLatin1String("operation")].toString();
        const QString json = args[QLatin1String("json")].toString();

        if (json.isEmpty())
            return {false, {}, {}, QStringLiteral("'json' parameter is required.")};

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

        if (op == QLatin1String("validate")) {
            if (parseError.error == QJsonParseError::NoError)
                return {true, {}, QStringLiteral("Valid JSON."), {}};
            return {true, {},
                    QStringLiteral("Invalid JSON at offset %1: %2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString()), {}};
        }

        if (parseError.error != QJsonParseError::NoError)
            return {false, {}, {},
                    QStringLiteral("JSON parse error at offset %1: %2")
                        .arg(parseError.offset)
                        .arg(parseError.errorString())};

        if (op == QLatin1String("parse") || op == QLatin1String("format")) {
            const bool compact = args[QLatin1String("compact")].toBool(false);
            const QJsonDocument::JsonFormat fmt = compact
                ? QJsonDocument::Compact
                : QJsonDocument::Indented;
            return {true, {}, QString::fromUtf8(doc.toJson(fmt)), {}};
        }

        if (op == QLatin1String("query")) {
            const QString pointer = args[QLatin1String("pointer")].toString();
            if (pointer.isEmpty())
                return {false, {}, {}, QStringLiteral("'pointer' is required for 'query'.")};

            // Navigate JSON by pointer path segments
            const QStringList parts = pointer.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            QJsonValue current = doc.isObject()
                ? QJsonValue(doc.object())
                : QJsonValue(doc.array());

            for (const QString &part : parts) {
                if (current.isObject()) {
                    current = current.toObject()[part];
                } else if (current.isArray()) {
                    bool ok = false;
                    const int idx = part.toInt(&ok);
                    if (!ok)
                        return {false, {}, {},
                                QStringLiteral("Invalid array index: %1").arg(part)};
                    current = current.toArray().at(idx);
                } else {
                    return {false, {}, {},
                            QStringLiteral("Cannot navigate into non-container at '%1'").arg(part)};
                }
            }

            const QJsonDocument resultDoc = current.isObject()
                ? QJsonDocument(current.toObject())
                : (current.isArray()
                    ? QJsonDocument(current.toArray())
                    : QJsonDocument());

            if (!resultDoc.isNull())
                return {true, {}, QString::fromUtf8(resultDoc.toJson(QJsonDocument::Indented)), {}};

            // Scalar value
            if (current.isString())
                return {true, {}, current.toString(), {}};
            if (current.isDouble())
                return {true, {}, QString::number(current.toDouble()), {}};
            if (current.isBool())
                return {true, {}, current.toBool() ? QStringLiteral("true") : QStringLiteral("false"), {}};
            if (current.isNull())
                return {true, {}, QStringLiteral("null"), {}};

            return {false, {}, {}, QStringLiteral("Value not found at pointer: %1").arg(pointer)};
        }

        return {false, {}, {}, QStringLiteral("Unknown operation: %1").arg(op)};
    }
};

// ── CurrentTimeTool ──────────────────────────────────────────────────────────
// Returns current wall-clock time. Provides ex.time.now() for the agent.
// Useful for timestamps, logging, scheduling awareness.

class CurrentTimeTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("current_time");
        s.description = QStringLiteral(
            "Get the current date, time, and timezone. Returns ISO 8601 "
            "timestamp, Unix epoch, and human-readable format. Useful for "
            "timestamps, scheduling, and time-based logic.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("format"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional Qt date format string (e.g. 'yyyy-MM-dd HH:mm:ss'). "
                                    "Default: returns multiple formats.")}
                }},
                {QStringLiteral("utc"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("If true, return UTC time instead of local. Default: false.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const bool useUtc = args[QLatin1String("utc")].toBool(false);
        const QString customFormat = args[QLatin1String("format")].toString();

        const QDateTime now = useUtc ? QDateTime::currentDateTimeUtc()
                                     : QDateTime::currentDateTime();

        if (!customFormat.isEmpty())
            return {true, {}, now.toString(customFormat), {}};

        QJsonObject data;
        data[QLatin1String("iso8601")]       = now.toString(Qt::ISODateWithMs);
        data[QLatin1String("unix")]          = static_cast<qint64>(now.toSecsSinceEpoch());
        data[QLatin1String("unixMs")]        = now.toMSecsSinceEpoch();
        data[QLatin1String("date")]          = now.date().toString(Qt::ISODate);
        data[QLatin1String("time")]          = now.time().toString(QStringLiteral("HH:mm:ss.zzz"));
        data[QLatin1String("dayOfWeek")]     = now.date().dayOfWeek();
        data[QLatin1String("timezone")]      = QString::fromUtf8(now.timeZone().id());
        data[QLatin1String("utcOffset")]     = now.timeZone().offsetFromUtc(now);

        const QString text = QStringLiteral("%1 (%2)")
                                 .arg(now.toString(Qt::ISODateWithMs))
                                 .arg(QString::fromUtf8(now.timeZone().id()));

        return {true, data, text, {}};
    }
};

// ── RegexTestTool ────────────────────────────────────────────────────────────
// Validate regex patterns and test them against inputs.
// Useful for building and debugging regular expressions.

class RegexTestTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("regex_test");
        s.description = QStringLiteral(
            "Validate and test regular expressions against input strings. "
            "Operations:\n"
            "  'validate' — check if regex pattern is valid\n"
            "  'match' — find first match in input\n"
            "  'matchAll' — find all matches\n"
            "  'replace' — replace matches with a replacement string\n"
            "Returns match positions, captured groups, and results.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("One of: validate, match, matchAll, replace")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("validate"), QStringLiteral("match"),
                        QStringLiteral("matchAll"), QStringLiteral("replace")}}
                }},
                {QStringLiteral("pattern"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("The regex pattern to test.")}
                }},
                {QStringLiteral("input"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Input text to match against (required for match/matchAll/replace).")}
                }},
                {QStringLiteral("replacement"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Replacement string for 'replace' operation. "
                                    "Supports backreferences (\\1, \\2).")}
                }},
                {QStringLiteral("caseInsensitive"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("Case-insensitive matching. Default: false.")}
                }},
                {QStringLiteral("multiline"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("Multiline mode (^ and $ match line boundaries). Default: false.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("operation"), QStringLiteral("pattern")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString op      = args[QLatin1String("operation")].toString();
        const QString pattern = args[QLatin1String("pattern")].toString();

        if (pattern.isEmpty())
            return {false, {}, {}, QStringLiteral("'pattern' is required.")};

        QRegularExpression::PatternOptions opts;
        if (args[QLatin1String("caseInsensitive")].toBool(false))
            opts |= QRegularExpression::CaseInsensitiveOption;
        if (args[QLatin1String("multiline")].toBool(false))
            opts |= QRegularExpression::MultilineOption;

        QRegularExpression regex(pattern, opts);

        if (op == QLatin1String("validate")) {
            if (regex.isValid())
                return {true, {}, QStringLiteral("Valid regex pattern."), {}};
            return {true, {},
                    QStringLiteral("Invalid regex at offset %1: %2")
                        .arg(regex.patternErrorOffset())
                        .arg(regex.errorString()), {}};
        }

        if (!regex.isValid())
            return {false, {}, {},
                    QStringLiteral("Invalid regex at offset %1: %2")
                        .arg(regex.patternErrorOffset())
                        .arg(regex.errorString())};

        const QString input = args[QLatin1String("input")].toString();
        if (input.isEmpty())
            return {false, {}, {}, QStringLiteral("'input' is required for match/replace operations.")};

        if (op == QLatin1String("match")) {
            const QRegularExpressionMatch m = regex.match(input);
            if (!m.hasMatch())
                return {true, {}, QStringLiteral("No match found."), {}};

            return {true, {}, formatMatch(m), {}};
        }

        if (op == QLatin1String("matchAll")) {
            auto it = regex.globalMatch(input);
            if (!it.hasNext())
                return {true, {}, QStringLiteral("No matches found."), {}};

            QString result;
            int count = 0;
            constexpr int kMaxMatches = 100;
            while (it.hasNext() && count < kMaxMatches) {
                result += QStringLiteral("Match %1:\n%2\n")
                              .arg(++count)
                              .arg(formatMatch(it.next()));
            }
            if (it.hasNext())
                result += QStringLiteral("... (truncated at %1 matches)").arg(kMaxMatches);

            return {true, {},
                    QStringLiteral("%1 match(es) found:\n%2").arg(count).arg(result), {}};
        }

        if (op == QLatin1String("replace")) {
            const QString replacement = args[QLatin1String("replacement")].toString();
            const QString result = QString(input).replace(regex, replacement);
            return {true, {}, result, {}};
        }

        return {false, {}, {}, QStringLiteral("Unknown operation: %1").arg(op)};
    }

private:
    static QString formatMatch(const QRegularExpressionMatch &m)
    {
        QString text = QStringLiteral("  Matched: \"%1\" at position %2-%3")
                           .arg(m.captured(0))
                           .arg(m.capturedStart(0))
                           .arg(m.capturedEnd(0));
        for (int i = 1; i <= m.lastCapturedIndex(); ++i) {
            text += QStringLiteral("\n  Group %1: \"%2\"")
                        .arg(i).arg(m.captured(i));
        }
        return text;
    }
};

// ── EnvironmentVariablesTool ─────────────────────────────────────────────────
// Read system environment variables. Useful for understanding build configs,
// PATH, compiler locations, SDK paths, etc.

class EnvironmentVariablesTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("environment_variables");
        s.description = QStringLiteral(
            "Read system environment variables. Operations:\n"
            "  'get' — get a specific variable's value\n"
            "  'list' — list all variables (optional filter)\n"
            "  'path' — show PATH entries as a list\n"
            "Useful for checking compiler paths, SDK locations, and build configs.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("One of: get, list, path")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("get"), QStringLiteral("list"),
                        QStringLiteral("path")}}
                }},
                {QStringLiteral("name"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Variable name for 'get' operation.")}
                }},
                {QStringLiteral("filter"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Filter pattern for 'list' (case-insensitive substring match).")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString op = args[QLatin1String("operation")].toString();
        const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        if (op == QLatin1String("get")) {
            const QString name = args[QLatin1String("name")].toString();
            if (name.isEmpty())
                return {false, {}, {}, QStringLiteral("'name' is required for 'get'.")};
            if (!env.contains(name))
                return {true, {}, QStringLiteral("Variable '%1' is not set.").arg(name), {}};
            return {true, {}, env.value(name), {}};
        }

        if (op == QLatin1String("list")) {
            const QString filter = args[QLatin1String("filter")].toString();
            const QStringList keys = env.keys();
            QString result;
            int count = 0;
            for (const QString &key : keys) {
                if (!filter.isEmpty() &&
                    !key.contains(filter, Qt::CaseInsensitive) &&
                    !env.value(key).contains(filter, Qt::CaseInsensitive))
                    continue;
                const QString val = env.value(key);
                // Truncate very long values
                const QString display = val.size() > 200
                    ? val.left(200) + QStringLiteral("...")
                    : val;
                result += QStringLiteral("%1=%2\n").arg(key, display);
                ++count;
            }
            if (count == 0)
                return {true, {}, QStringLiteral("No variables found matching filter."), {}};
            return {true, {},
                    QStringLiteral("%1 variable(s):\n%2").arg(count).arg(result), {}};
        }

        if (op == QLatin1String("path")) {
            const QString pathVal = env.value(QStringLiteral("PATH"));
            if (pathVal.isEmpty())
                return {true, {}, QStringLiteral("PATH is not set."), {}};

#ifdef Q_OS_WIN
            const QChar sep = QLatin1Char(';');
#else
            const QChar sep = QLatin1Char(':');
#endif
            const QStringList entries = pathVal.split(sep, Qt::SkipEmptyParts);
            QString result;
            for (int i = 0; i < entries.size(); ++i)
                result += QStringLiteral("%1. %2\n").arg(i + 1).arg(entries[i]);

            return {true, {},
                    QStringLiteral("%1 PATH entries:\n%2").arg(entries.size()).arg(result), {}};
        }

        return {false, {}, {}, QStringLiteral("Unknown operation: %1").arg(op)};
    }
};

// ── FileHashTool ─────────────────────────────────────────────────────────────
// Compute SHA256 or MD5 hash of a file for verification.

class FileHashTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("file_content_hash");
        s.description = QStringLiteral(
            "Compute a cryptographic hash (SHA256 or MD5) of a file. "
            "Useful for file verification, detecting changes, comparing "
            "files by content, and integrity checks.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 30000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("path"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("File path (absolute or workspace-relative).")}
                }},
                {QStringLiteral("algorithm"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Hash algorithm: sha256 (default) or md5.")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("sha256"), QStringLiteral("md5")}}
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
            return {false, {}, {}, QStringLiteral("'path' is required.")};

        const QString path = resolvePath(rawPath);
        QFile file(path);
        if (!file.exists())
            return {false, {}, {}, QStringLiteral("File not found: %1").arg(path)};
        if (!file.open(QIODevice::ReadOnly))
            return {false, {}, {}, QStringLiteral("Cannot open file: %1").arg(path)};

        const QString algo = args[QLatin1String("algorithm")].toString(QStringLiteral("sha256"));
        QCryptographicHash::Algorithm hashAlgo = algo == QLatin1String("md5")
            ? QCryptographicHash::Md5
            : QCryptographicHash::Sha256;

        QCryptographicHash hasher(hashAlgo);
        if (!hasher.addData(&file))
            return {false, {}, {}, QStringLiteral("Error reading file for hashing.")};

        const QString hash = QString::fromLatin1(hasher.result().toHex());
        const qint64 fileSize = file.size();

        QJsonObject data;
        data[QLatin1String("hash")]      = hash;
        data[QLatin1String("algorithm")] = algo.toUpper();
        data[QLatin1String("fileSize")]  = fileSize;
        data[QLatin1String("filePath")]  = path;

        return {true, data,
                QStringLiteral("%1 (%2, %3 bytes): %4")
                    .arg(QFileInfo(path).fileName(), algo.toUpper())
                    .arg(fileSize)
                    .arg(hash), {}};
    }

private:
    QString resolvePath(const QString &path) const
    {
        if (path.isEmpty() || QFileInfo(path).isAbsolute() || m_workspaceRoot.isEmpty())
            return path;
        return QDir(m_workspaceRoot).absoluteFilePath(path);
    }

    QString m_workspaceRoot;
};

// ── WorkspaceConfigTool ──────────────────────────────────────────────────────
// Read workspace configuration files like .exorcist/, CMakePresets.json,
// .editorconfig, .clang-format, etc.

class WorkspaceConfigTool : public ITool
{
public:
    explicit WorkspaceConfigTool(IFileSystem *fs) : m_fs(fs) {}

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("workspace_config");
        s.description = QStringLiteral(
            "Read workspace configuration files. Automatically finds and reads "
            "common config files:\n"
            "  'scan' — list all config files found in workspace root\n"
            "  'read' — read a specific config file\n"
            "Recognized: .editorconfig, .clang-format, .clang-tidy, "
            "CMakePresets.json, CMakeLists.txt, .gitignore, "
            ".exorcist/, tsconfig.json, package.json, Cargo.toml, etc.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("One of: scan, read")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("scan"), QStringLiteral("read")}}
                }},
                {QStringLiteral("file"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Config file to read (for 'read' operation). "
                                    "Can be a name from 'scan' results or relative path.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        if (m_workspaceRoot.isEmpty())
            return {false, {}, {}, QStringLiteral("No workspace root set.")};

        const QString op = args[QLatin1String("operation")].toString();

        if (op == QLatin1String("scan"))
            return scanConfigs();
        if (op == QLatin1String("read"))
            return readConfig(args[QLatin1String("file")].toString());

        return {false, {}, {}, QStringLiteral("Unknown operation: %1").arg(op)};
    }

private:
    ToolExecResult scanConfigs() const
    {
        static const QStringList kConfigFiles = {
            QStringLiteral(".editorconfig"),
            QStringLiteral(".clang-format"),
            QStringLiteral(".clang-tidy"),
            QStringLiteral(".gitignore"),
            QStringLiteral(".gitattributes"),
            QStringLiteral("CMakeLists.txt"),
            QStringLiteral("CMakePresets.json"),
            QStringLiteral("Makefile"),
            QStringLiteral("meson.build"),
            QStringLiteral("package.json"),
            QStringLiteral("tsconfig.json"),
            QStringLiteral("Cargo.toml"),
            QStringLiteral("go.mod"),
            QStringLiteral("pyproject.toml"),
            QStringLiteral("setup.py"),
            QStringLiteral("requirements.txt"),
            QStringLiteral(".eslintrc.json"),
            QStringLiteral(".prettierrc"),
            QStringLiteral("Dockerfile"),
            QStringLiteral("docker-compose.yml"),
            QStringLiteral("sonar-project.properties"),
        };

        QStringList found;
        const QDir root(m_workspaceRoot);
        for (const QString &name : kConfigFiles) {
            if (QFileInfo::exists(root.absoluteFilePath(name)))
                found << name;
        }

        // Check for .exorcist/ directory
        if (QFileInfo(root.absoluteFilePath(QStringLiteral(".exorcist"))).isDir()) {
            QDir exoDir(root.absoluteFilePath(QStringLiteral(".exorcist")));
            const QStringList entries = exoDir.entryList(QDir::Files);
            for (const QString &e : entries)
                found << QStringLiteral(".exorcist/") + e;
        }

        if (found.isEmpty())
            return {true, {}, QStringLiteral("No recognized config files found."), {}};

        return {true, {},
                QStringLiteral("%1 config file(s) found:\n%2")
                    .arg(found.size())
                    .arg(found.join(QLatin1Char('\n'))), {}};
    }

    ToolExecResult readConfig(const QString &file) const
    {
        if (file.isEmpty())
            return {false, {}, {}, QStringLiteral("'file' is required for 'read'.")};

        const QString path = QDir(m_workspaceRoot).absoluteFilePath(file);

        // Security: don't allow reading outside workspace
        const QString canonical = QFileInfo(path).canonicalFilePath();
        const QString wsCanonical = QFileInfo(m_workspaceRoot).canonicalFilePath();
        if (!canonical.isEmpty() && !canonical.startsWith(wsCanonical))
            return {false, {}, {}, QStringLiteral("Path is outside workspace.")};

        QString error;
        const QString content = m_fs->readTextFile(path, &error);
        if (!error.isEmpty())
            return {false, {}, {}, error};

        // Truncate very large configs
        constexpr int kMaxSize = 50 * 1024;
        if (content.size() > kMaxSize) {
            return {true, {},
                    content.left(kMaxSize) + QStringLiteral("\n... (truncated at 50KB)"), {}};
        }

        return {true, {}, content, {}};
    }

    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};
