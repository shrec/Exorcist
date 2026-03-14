#include "changeimpacttool.h"

ChangeImpactTool::ChangeImpactTool(ImpactAnalyzer analyzer)
    : m_analyzer(std::move(analyzer))
{}

ToolSpec ChangeImpactTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("analyze_change_impact");
    s.description = QStringLiteral(
        "Analyze the impact of changing a symbol before making the change. "
        "Answers: who uses this? what tests cover it? what will recompile? "
        "how risky is this change?\n\n"
        "Returns:\n"
        "  - Direct and transitive reference counts\n"
        "  - List of affected files that need review\n"
        "  - Affected test files and coverage percentage\n"
        "  - Build targets that would recompile\n"
        "  - Risk score (1-10) with explanation\n"
        "  - Specific warnings (public API, no tests, etc.)\n\n"
        "ALWAYS use this before:\n"
        "  - Renaming public APIs or widely-used functions\n"
        "  - Deleting classes, functions, or files\n"
        "  - Changing function signatures\n"
        "  - Moving code between files/modules");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 60000; // may need to traverse large reference graphs
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the file containing the symbol to analyze.")}
            }},
            {QStringLiteral("line"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based line number of the symbol.")}
            }},
            {QStringLiteral("column"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based column number of the symbol.")}
            }},
            {QStringLiteral("changeType"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("modify"),
                    QStringLiteral("rename"),
                    QStringLiteral("delete"),
                    QStringLiteral("move"),
                    QStringLiteral("signature_change")
                }},
                {QStringLiteral("description"),
                 QStringLiteral("Type of change being considered. Affects risk "
                                "calculation: 'delete' and 'signature_change' are "
                                "higher risk than 'modify'.")}
            }},
            {QStringLiteral("symbolName"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Optional: symbol name if known. Helps when "
                                "line/column are approximate.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("filePath"),
            QStringLiteral("changeType")
        }}
    };
    return s;
}

ToolExecResult ChangeImpactTool::invoke(const QJsonObject &args)
{
    const QString filePath   = args[QLatin1String("filePath")].toString();
    const QString changeType = args[QLatin1String("changeType")].toString();
    const int line           = args[QLatin1String("line")].toInt(0);
    const int column         = args[QLatin1String("column")].toInt(0);

    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};
    if (changeType.isEmpty())
        return {false, {}, {}, QStringLiteral("'changeType' is required.")};

    const ImpactResult result = m_analyzer(filePath, line, column, changeType);

    if (!result.ok)
        return {false, {}, {},
                QStringLiteral("Impact analysis failed: %1").arg(result.error)};

    // Build structured data
    QJsonObject data;
    data[QLatin1String("riskScore")]            = result.riskScore;
    data[QLatin1String("riskExplanation")]       = result.riskExplanation;
    data[QLatin1String("directReferences")]      = result.directReferences;
    data[QLatin1String("transitiveReferences")]  = result.transitiveReferences;
    data[QLatin1String("testCoverage")]          = result.testCoverage;
    data[QLatin1String("rebuildFileCount")]       = result.rebuildFileCount;

    if (!result.affectedFiles.isEmpty()) {
        QJsonArray files;
        for (const QString &f : result.affectedFiles)
            files.append(f);
        data[QLatin1String("affectedFiles")] = files;
    }
    if (!result.affectedTests.isEmpty()) {
        QJsonArray tests;
        for (const QString &t : result.affectedTests)
            tests.append(t);
        data[QLatin1String("affectedTests")] = tests;
    }
    if (!result.affectedTargets.isEmpty()) {
        QJsonArray targets;
        for (const QString &t : result.affectedTargets)
            targets.append(t);
        data[QLatin1String("affectedTargets")] = targets;
    }
    if (!result.warnings.isEmpty()) {
        QJsonArray warns;
        for (const QString &w : result.warnings)
            warns.append(w);
        data[QLatin1String("warnings")] = warns;
    }

    // Human-readable summary
    QString text = QStringLiteral("Risk: %1/10 — %2\n")
                        .arg(result.riskScore)
                        .arg(result.riskExplanation);
    text += QStringLiteral("References: %1 direct, %2 transitive\n")
                .arg(result.directReferences)
                .arg(result.transitiveReferences);
    text += QStringLiteral("Affected files: %1, tests: %2, rebuild: %3 files\n")
                .arg(result.affectedFiles.size())
                .arg(result.affectedTests.size())
                .arg(result.rebuildFileCount);
    if (result.testCoverage >= 0)
        text += QStringLiteral("Test coverage: %1%\n").arg(result.testCoverage);
    for (const QString &w : result.warnings)
        text += QStringLiteral("  ⚠ %1\n").arg(w);
    if (!result.summary.isEmpty())
        text += result.summary;

    return {true, data, text, {}};
}
