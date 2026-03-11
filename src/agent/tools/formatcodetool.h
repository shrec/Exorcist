#pragma once

#include "../itool.h"

#include <QJsonArray>
#include <functional>

// ── FormatCodeTool ──────────────────────────────────────────────────────────
// Abstract code formatting interface. The agent calls this single tool;
// the backend dispatches to the correct formatter based on language/file
// extension (clang-format, prettier, black, rustfmt, gofmt, etc.).
//
// The host provides a CodeFormatter callback that:
//   1. Detects language from file extension (or explicit language param)
//   2. Finds the appropriate formatter (clang-format, prettier, etc.)
//   3. Respects project config (.clang-format, .prettierrc, .editorconfig)
//   4. Returns formatted text + what changed
//
// The agent never needs to know which formatter runs underneath.

class FormatCodeTool : public ITool
{
public:
    struct FormatResult {
        bool    ok;
        QString formatted;    // formatted text (empty if in-place file format)
        QString diff;         // unified diff of changes (empty if no changes)
        QString error;
        QString formatterUsed; // e.g. "clang-format 17", "prettier 3.2"
    };

    // formatter(filePath, content, language, options) → FormatResult
    //   filePath: used to detect language & find config; may be empty for raw text
    //   content:  text to format; if empty, reads from filePath
    //   language: override language detection ("cpp", "js", "py", etc.); empty = auto
    //   rangeStart/rangeEnd: 0 = format entire file, otherwise 1-based line range
    using CodeFormatter = std::function<FormatResult(
        const QString &filePath,
        const QString &content,
        const QString &language,
        int rangeStart,
        int rangeEnd)>;

    explicit FormatCodeTool(CodeFormatter formatter)
        : m_formatter(std::move(formatter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("format_code");
        s.description = QStringLiteral(
            "Format source code using the project's configured formatter. "
            "Automatically detects the language from file extension and uses "
            "the appropriate tool (clang-format for C/C++, prettier for "
            "JS/TS/HTML/CSS, black for Python, rustfmt for Rust, gofmt for Go, "
            "etc.). Respects project config files (.clang-format, .prettierrc, "
            ".editorconfig). Use after writing or modifying code to ensure "
            "consistent style.\n\n"
            "Modes:\n"
            "  1. Format file: provide filePath only\n"
            "  2. Format text: provide content (+ optional language hint)\n"
            "  3. Format range: provide filePath + rangeStart/rangeEnd lines");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 15000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("filePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Path to the file to format. Used for language "
                                    "detection and config lookup.")}
                }},
                {QStringLiteral("content"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Raw source code to format. If omitted, reads "
                                    "from filePath.")}
                }},
                {QStringLiteral("language"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Override language detection. Values: cpp, c, h, "
                                    "js, ts, jsx, tsx, py, rs, go, java, cs, html, "
                                    "css, json, yaml, xml, sql, lua, proto.")}
                }},
                {QStringLiteral("rangeStart"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based start line for partial formatting.")}
                }},
                {QStringLiteral("rangeEnd"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based end line for partial formatting.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString filePath = args[QLatin1String("filePath")].toString();
        const QString content  = args[QLatin1String("content")].toString();
        const QString language = args[QLatin1String("language")].toString();
        const int rangeStart   = args[QLatin1String("rangeStart")].toInt(0);
        const int rangeEnd     = args[QLatin1String("rangeEnd")].toInt(0);

        if (filePath.isEmpty() && content.isEmpty())
            return {false, {}, {},
                    QStringLiteral("Either 'filePath' or 'content' is required.")};

        const FormatResult result = m_formatter(filePath, content, language,
                                                rangeStart, rangeEnd);

        if (!result.ok)
            return {false, {}, {},
                    QStringLiteral("Format failed: %1").arg(result.error)};

        // Build response
        QString text;
        if (result.diff.isEmpty()) {
            text = QStringLiteral("Already formatted — no changes needed.");
        } else {
            text = QStringLiteral("Formatted with %1").arg(result.formatterUsed);
            if (!filePath.isEmpty())
                text += QStringLiteral(" → %1").arg(filePath);
        }

        QJsonObject data;
        if (!result.formatted.isEmpty())
            data[QLatin1String("formatted")] = result.formatted;
        if (!result.diff.isEmpty())
            data[QLatin1String("diff")] = result.diff;
        if (!result.formatterUsed.isEmpty())
            data[QLatin1String("formatter")] = result.formatterUsed;

        return {true, data, text, {}};
    }

private:
    CodeFormatter m_formatter;
};
