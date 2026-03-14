#include "staticanalysistool.h"

#include <QJsonArray>
#include <QJsonObject>

StaticAnalysisTool::StaticAnalysisTool(StaticAnalyzer analyzer)
    : m_analyzer(std::move(analyzer))
{
}

ToolSpec StaticAnalysisTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("static_analysis");
    s.description = QStringLiteral(
        "Run static analysis / linting on source code. Automatically "
        "detects the language and uses the appropriate analyzer:\n"
        "  C/C++: clang-tidy\n"
        "  JavaScript/TypeScript: ESLint\n"
        "  Python: pylint / mypy\n"
        "  Rust: clippy\n"
        "  Go: go vet / staticcheck\n\n"
        "Returns findings with severity, line, message, and fix suggestions. "
        "Can auto-fix simple issues when fixMode is true.\n\n"
        "Use after writing code to catch bugs, style violations, and "
        "potential security issues before building.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 60000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("File or directory to analyze.")}
            }},
            {QStringLiteral("checkers"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")}
                }},
                {QStringLiteral("description"),
                 QStringLiteral("Specific checkers to enable (e.g. "
                                "[\"modernize-*\", \"bugprone-*\"] for clang-tidy). "
                                "Empty = project defaults.")}
            }},
            {QStringLiteral("fixMode"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Auto-fix simple issues. Default: false.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath")}}
    };
    return s;
}

ToolExecResult StaticAnalysisTool::invoke(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};

    QStringList checkers;
    const QJsonArray arr = args[QLatin1String("checkers")].toArray();
    for (const auto &v : arr)
        checkers.append(v.toString());

    const bool fixMode = args[QLatin1String("fixMode")].toBool(false);

    const AnalysisResult result = m_analyzer(filePath, checkers, fixMode);

    if (!result.ok)
        return {false, {}, {},
                QStringLiteral("Analysis failed: %1").arg(result.error)};

    // Build response
    QString text;
    if (result.findings.isEmpty()) {
        text = QStringLiteral("No issues found (%1).").arg(result.analyzerUsed);
    } else {
        text = QStringLiteral("%1 findings (%2 errors, %3 warnings) — %4\n\n")
                   .arg(result.findings.size())
                   .arg(result.errorCount)
                   .arg(result.warningCount)
                   .arg(result.analyzerUsed);

        for (const Finding &f : result.findings) {
            text += QStringLiteral("%1:%2:%3 [%4] %5")
                        .arg(f.filePath)
                        .arg(f.line)
                        .arg(f.column)
                        .arg(f.severity)
                        .arg(f.message);
            if (!f.ruleId.isEmpty())
                text += QStringLiteral(" (%1)").arg(f.ruleId);
            text += QLatin1Char('\n');

            // Truncate if too large for model context
            if (text.size() > 30000) {
                text += QStringLiteral("\n... (%1 more findings truncated)")
                            .arg(result.findings.size());
                break;
            }
        }
    }

    QJsonObject data;
    data[QLatin1String("errorCount")]   = result.errorCount;
    data[QLatin1String("warningCount")] = result.warningCount;
    data[QLatin1String("totalFindings")] = result.findings.size();
    data[QLatin1String("analyzer")]     = result.analyzerUsed;

    return {true, data, text, {}};
}
