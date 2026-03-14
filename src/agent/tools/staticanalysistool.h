#pragma once

#include "../itool.h"

#include <functional>

// ── StaticAnalysisTool ──────────────────────────────────────────────────────
// Abstract static analysis interface. Dispatches to the right analyzer
// based on language: clang-tidy (C/C++), ESLint (JS/TS), Pylint/mypy (Python),
// clippy (Rust), etc. Agent calls one tool, backend picks the right checker.
//
// Similar pattern to FormatCodeTool — one interface, multiple backends.

class StaticAnalysisTool : public ITool
{
public:
    struct Finding {
        QString filePath;
        int     line;
        int     column;
        QString severity;    // "error", "warning", "info", "hint"
        QString message;
        QString ruleId;      // e.g. "clang-tidy:modernize-use-override"
        QString fixSuggestion;
    };

    struct AnalysisResult {
        bool    ok;
        QList<Finding> findings;
        int     errorCount;
        int     warningCount;
        QString analyzerUsed;  // e.g. "clang-tidy 17", "eslint 8.52"
        QString error;
    };

    // analyzer(filePath, checkers, fixMode) → AnalysisResult
    //   filePath: file or directory to analyze
    //   checkers: specific checks to run (empty = all defaults)
    //   fixMode: apply auto-fixes
    using StaticAnalyzer = std::function<AnalysisResult(
        const QString &filePath,
        const QStringList &checkers,
        bool fixMode)>;

    explicit StaticAnalysisTool(StaticAnalyzer analyzer);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    StaticAnalyzer m_analyzer;
};
