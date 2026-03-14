#pragma once

#include "../itool.h"

#include <functional>

// ── RefactorTool ────────────────────────────────────────────────────────────
// LSP-powered semantic refactoring. Single tool with multiple operations.
// Backend dispatches to clangd (C++), tsserver (JS/TS), pyright (Python), etc.
// Unlike text-based replace, this understands scopes, types, and references.

class RefactorTool : public ITool
{
public:
    struct RefactorResult {
        bool    ok;
        int     filesChanged;
        int     editsApplied;
        QString summary;     // human-readable summary of what changed
        QString diff;        // unified diff of all changes
        QString error;
    };

    // refactor(operation, filePath, line, column, newName, selectedRange) → RefactorResult
    //   operation: "rename", "extract_function", "extract_variable", "inline"
    //   filePath + line + column: location of the symbol
    //   newName: new name for rename/extract
    //   rangeStartLine/rangeEndLine: for extract operations (what code to extract)
    using Refactorer = std::function<RefactorResult(
        const QString &operation,
        const QString &filePath,
        int line, int column,
        const QString &newName,
        int rangeStartLine, int rangeEndLine)>;

    explicit RefactorTool(Refactorer refactorer);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    Refactorer m_refactorer;
};
