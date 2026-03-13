#pragma once

#include "../itool.h"

#include <QJsonArray>
#include <functional>

// ── TreeSitterQueryTool ──────────────────────────────────────────────────────
//
// Rich tree-sitter AST access for the agent. Provides structural code
// analysis beyond the basic S-expression dump of TreeSitterParseTool.
//
// Operations:
//   "parse_file"  — parse a workspace file, return AST (detects language)
//   "query"       — run a tree-sitter S-expression query pattern
//   "symbols"     — extract top-level declarations (functions, classes, etc.)
//   "node_at"     — get the AST node at a specific line:column
//
// All operations use callbacks injected at construction. The callbacks
// are wired from MainWindow where tree-sitter grammars are available.

class TreeSitterQueryTool : public ITool
{
public:
    // Result types
    struct SymbolInfo {
        QString name;
        QString kind;       // "function", "class", "struct", "enum", "method", "variable"
        int     startLine;  // 0-based
        int     endLine;    // 0-based
        int     startCol;
    };

    struct NodeInfo {
        QString type;
        bool    isNamed;
        int     startLine;
        int     startCol;
        int     endLine;
        int     endCol;
        QString text;       // node text (truncated if large)
        int     childCount;
    };

    struct QueryMatch {
        QString captureName;
        QString nodeType;
        int     startLine;
        int     startCol;
        int     endLine;
        int     endCol;
        QString text;
    };

    struct QueryResult {
        bool    ok;
        QList<QueryMatch> matches;
        int     matchCount;
        QString error;
    };

    // Callbacks
    using FileParserFn  = std::function<QString(const QString &filePath, int maxDepth)>;
    using QueryRunnerFn = std::function<QueryResult(const QString &filePath,
                                                     const QString &queryPattern)>;
    using SymbolExtractorFn = std::function<QList<SymbolInfo>(const QString &filePath)>;
    using NodeAtPositionFn  = std::function<NodeInfo(const QString &filePath,
                                                      int line, int column)>;

    TreeSitterQueryTool(FileParserFn fileParser,
                        QueryRunnerFn queryRunner,
                        SymbolExtractorFn symbolExtractor,
                        NodeAtPositionFn nodeAtPosition);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ToolExecResult parseFile(const QJsonObject &args);
    ToolExecResult runQuery(const QJsonObject &args);
    ToolExecResult extractSymbols(const QJsonObject &args);
    ToolExecResult nodeAt(const QJsonObject &args);

    FileParserFn      m_fileParser;
    QueryRunnerFn     m_queryRunner;
    SymbolExtractorFn m_symbolExtractor;
    NodeAtPositionFn  m_nodeAtPosition;
};
