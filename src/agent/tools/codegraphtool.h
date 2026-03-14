#pragma once

#include "../itool.h"

#include <functional>

// ── CodeGraphTool ────────────────────────────────────────────────────────────
// Unified code intelligence tool for the AI agent.
// Combines symbol search, reference lookup, and semantic chunk search.
//
// Operations:
//   "find_symbol"     — search workspace symbols by name (class, function, etc.)
//   "find_references" — find all references to a symbol at file:line:column (LSP)
//   "find_definition" — go to definition of symbol at file:line:column (LSP)
//   "symbols_in_file" — list all symbols defined in a file
//   "semantic_chunks" — search code chunks by keyword (semantic index)

class CodeGraphTool : public ITool
{
public:
    // Symbol search: (query, maxResults) → formatted text
    using SymbolSearchFn    = std::function<QString(const QString &query, int maxResults)>;
    // Symbols in file: (filePath) → formatted text
    using SymbolsInFileFn   = std::function<QString(const QString &filePath)>;
    // References: (filePath, line, column) → formatted text (async collected)
    using FindReferencesFn  = std::function<QString(const QString &filePath, int line, int column)>;
    // Definition: (filePath, line, column) → formatted text
    using FindDefinitionFn  = std::function<QString(const QString &filePath, int line, int column)>;
    // Semantic chunk search: (query, maxResults) → formatted text
    using ChunkSearchFn     = std::function<QString(const QString &query, int maxResults)>;

    CodeGraphTool(SymbolSearchFn symbolSearch,
                  SymbolsInFileFn symbolsInFile,
                  FindReferencesFn findRefs,
                  FindDefinitionFn findDef,
                  ChunkSearchFn chunkSearch);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    SymbolSearchFn   m_symbolSearch;
    SymbolsInFileFn  m_symbolsInFile;
    FindReferencesFn m_findRefs;
    FindDefinitionFn m_findDef;
    ChunkSearchFn    m_chunkSearch;
};
