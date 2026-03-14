#pragma once

#include "../itool.h"

#include <functional>

// ── ChangeImpactTool ────────────────────────────────────────────────────────
// Analyzes what would break or be affected by a code change BEFORE making it.
// Combines: reference graph, test mapping, build dependencies, and heuristic
// risk scoring. This is what separates a senior developer from a junior.

class ChangeImpactTool : public ITool
{
public:
    struct ImpactResult {
        bool    ok;

        // Reference impact
        int     directReferences;    // functions/classes that directly use this symbol
        int     transitiveReferences; // indirect dependents (2+ hops)
        QStringList affectedFiles;   // files that would need review

        // Test impact
        QStringList affectedTests;   // test files/functions that cover this code
        int         testCoverage;    // 0-100 coverage percentage for this area

        // Build impact
        QStringList affectedTargets; // build targets that would recompile
        int         rebuildFileCount; // number of files that would recompile

        // Risk assessment
        int     riskScore;           // 1-10: 1 = trivial rename, 10 = core API change
        QString riskExplanation;     // why this risk level
        QStringList warnings;        // specific risks (e.g. "public API change",
                                     // "no test coverage", "circular dependency")
        QString summary;             // human-readable overall summary
        QString error;
    };

    // analyzer(filePath, line, column, changeType) → ImpactResult
    //   changeType: "modify", "rename", "delete", "move", "signature_change"
    using ImpactAnalyzer = std::function<ImpactResult(
        const QString &filePath,
        int line, int column,
        const QString &changeType)>;

    explicit ChangeImpactTool(ImpactAnalyzer analyzer);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ImpactAnalyzer m_analyzer;
};
