#pragma once

#include "../itool.h"

#include <functional>

// ── RenameSymbolTool ──────────────────────────────────────────────────────────
// Performs a semantic rename of a symbol across the entire workspace via LSP.
// Takes a file path, position, and new name; returns the list of changes made.

class RenameSymbolTool : public ITool
{
public:
    // Callback: (filePath, line, column, newName) → JSON string of workspace edits
    // The JSON contains {"changes": {"uri": [edits, ...]}} or error.
    using SymbolRenamer = std::function<QString(const QString &, int, int,
                                                const QString &)>;

    explicit RenameSymbolTool(SymbolRenamer renamer);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SymbolRenamer m_renamer;
};

// ── ListCodeUsagesTool ────────────────────────────────────────────────────────
// Finds all usages (references, definitions, implementations) of a symbol.
// Wraps LSP findReferences for better discoverability vs code_graph.

class ListCodeUsagesTool : public ITool
{
public:
    // Callback: (filePath, line, column) → formatted string of all usages
    using UsageFinder = std::function<QString(const QString &, int, int)>;

    explicit ListCodeUsagesTool(UsageFinder finder);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    UsageFinder m_finder;
};
