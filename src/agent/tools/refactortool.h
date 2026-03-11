#pragma once

#include "../itool.h"

#include <QJsonArray>
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

    explicit RefactorTool(Refactorer refactorer)
        : m_refactorer(std::move(refactorer)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("refactor");
        s.description = QStringLiteral(
            "Perform semantic refactoring using the language server. Unlike "
            "text-based find-replace, this understands scopes, types, and "
            "references — guaranteed correct across the entire project.\n\n"
            "Operations:\n"
            "  rename — Rename a symbol everywhere it's referenced. Provide "
            "filePath, line, column of the symbol and newName.\n"
            "  extract_function — Extract selected code into a new function. "
            "Provide filePath, rangeStartLine, rangeEndLine, and newName for "
            "the new function.\n"
            "  extract_variable — Extract an expression into a named variable. "
            "Provide filePath, line, column of the expression, and newName.\n"
            "  inline — Inline a variable or function call at the cursor. "
            "Provide filePath, line, column.\n\n"
            "Always prefer this over replace_string for renaming symbols.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 30000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("rename"),
                        QStringLiteral("extract_function"),
                        QStringLiteral("extract_variable"),
                        QStringLiteral("inline")
                    }},
                    {QStringLiteral("description"),
                     QStringLiteral("The refactoring operation to perform.")}
                }},
                {QStringLiteral("filePath"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Path to the file containing the symbol.")}
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
                {QStringLiteral("newName"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("New name for rename/extract operations.")}
                }},
                {QStringLiteral("rangeStartLine"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based start line of code to extract.")}
                }},
                {QStringLiteral("rangeEndLine"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("1-based end line of code to extract.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("operation"),
                QStringLiteral("filePath")
            }}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString operation = args[QLatin1String("operation")].toString();
        const QString filePath  = args[QLatin1String("filePath")].toString();
        const int line          = args[QLatin1String("line")].toInt(0);
        const int column        = args[QLatin1String("column")].toInt(0);
        const QString newName   = args[QLatin1String("newName")].toString();
        const int rangeStart    = args[QLatin1String("rangeStartLine")].toInt(0);
        const int rangeEnd      = args[QLatin1String("rangeEndLine")].toInt(0);

        if (operation.isEmpty())
            return {false, {}, {}, QStringLiteral("'operation' is required.")};
        if (filePath.isEmpty())
            return {false, {}, {}, QStringLiteral("'filePath' is required.")};

        if (operation == QLatin1String("rename") && newName.isEmpty())
            return {false, {}, {}, QStringLiteral("'newName' is required for rename.")};
        if (operation == QLatin1String("extract_function") && newName.isEmpty())
            return {false, {}, {}, QStringLiteral("'newName' is required for extract_function.")};

        const RefactorResult result = m_refactorer(
            operation, filePath, line, column, newName, rangeStart, rangeEnd);

        if (!result.ok)
            return {false, {}, {},
                    QStringLiteral("Refactor failed: %1").arg(result.error)};

        QJsonObject data;
        data[QLatin1String("filesChanged")] = result.filesChanged;
        data[QLatin1String("editsApplied")] = result.editsApplied;
        if (!result.diff.isEmpty())
            data[QLatin1String("diff")] = result.diff;

        return {true, data, result.summary, {}};
    }

private:
    Refactorer m_refactorer;
};
