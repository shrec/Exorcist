#pragma once

#include <QList>
#include <QString>
#include <functional>

#include "agent/tools/treesitterquerytool.h"

/// Helper that provides tree-sitter callbacks for the agent platform.
/// Reads files from disk, detects language from extension, and runs
/// tree-sitter operations (parse, query, symbol extraction, node lookup).
///
/// This is NOT a QObject. It is a stateless utility created once during
/// MainWindow bootstrap and captured by the agent callback lambdas.

class TreeSitterAgentHelper
{
public:
    TreeSitterAgentHelper();

    QString parseFile(const QString &filePath, int maxDepth);
    TreeSitterQueryTool::QueryResult runQuery(const QString &filePath,
                                               const QString &queryPattern);
    QList<TreeSitterQueryTool::SymbolInfo> extractSymbols(const QString &filePath);
    TreeSitterQueryTool::NodeInfo nodeAtPosition(const QString &filePath,
                                                  int line, int column);
};
