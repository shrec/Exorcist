#include "formatcodetool.h"

FormatCodeTool::FormatCodeTool(CodeFormatter formatter)
    : m_formatter(std::move(formatter))
{}

ToolSpec FormatCodeTool::spec() const
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

ToolExecResult FormatCodeTool::invoke(const QJsonObject &args)
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
