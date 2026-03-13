#include "treesitterquerytool.h"

#include <QFileInfo>
#include <QJsonArray>

// ── Construction ─────────────────────────────────────────────────────────────

TreeSitterQueryTool::TreeSitterQueryTool(FileParserFn fileParser,
                                         QueryRunnerFn queryRunner,
                                         SymbolExtractorFn symbolExtractor,
                                         NodeAtPositionFn nodeAtPosition)
    : m_fileParser(std::move(fileParser))
    , m_queryRunner(std::move(queryRunner))
    , m_symbolExtractor(std::move(symbolExtractor))
    , m_nodeAtPosition(std::move(nodeAtPosition))
{
}

// ── Spec ─────────────────────────────────────────────────────────────────────

ToolSpec TreeSitterQueryTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("tree_sitter_query");
    s.description = QStringLiteral(
        "Rich tree-sitter AST access for structural code analysis.\n\n"
        "Operations:\n"
        "- 'parse_file': Parse a workspace file and return the AST as an "
        "S-expression. Detects language from file extension. Use maxDepth "
        "to limit output for large files.\n"
        "- 'query': Run a tree-sitter query pattern (S-expression) on a file "
        "and return all matches. Example: '(function_definition name: "
        "(identifier) @fn-name)' finds all function names.\n"
        "- 'symbols': Extract top-level declarations (functions, classes, "
        "structs, enums, methods) from a file with line numbers.\n"
        "- 'node_at': Get the AST node at a specific line:column position. "
        "Returns node type, range, text, and child count.\n\n"
        "Supports: C, C++, Python, JavaScript, TypeScript, Rust, JSON, Go, YAML, TOML.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 15000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The operation to perform.")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("parse_file"),
                    QStringLiteral("query"),
                    QStringLiteral("symbols"),
                    QStringLiteral("node_at")
                }}
            }},
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the file to analyze. Required for all operations.")}
            }},
            {QStringLiteral("queryPattern"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Tree-sitter query pattern (S-expression). Required for 'query' operation. "
                                "Example: '(function_definition name: (identifier) @fn-name)'")}
            }},
            {QStringLiteral("line"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based line number. Required for 'node_at'.")}
            }},
            {QStringLiteral("column"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based column number. Required for 'node_at'.")}
            }},
            {QStringLiteral("maxDepth"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum AST depth for 'parse_file' (default: unlimited).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("operation"), QStringLiteral("filePath")}}
    };
    return s;
}

// ── Dispatch ─────────────────────────────────────────────────────────────────

ToolExecResult TreeSitterQueryTool::invoke(const QJsonObject &args)
{
    const QString op = args[QLatin1String("operation")].toString();

    if (op == QLatin1String("parse_file"))
        return parseFile(args);
    if (op == QLatin1String("query"))
        return runQuery(args);
    if (op == QLatin1String("symbols"))
        return extractSymbols(args);
    if (op == QLatin1String("node_at"))
        return nodeAt(args);

    return {false, {}, {},
            QStringLiteral("Unknown operation '%1'. Use: parse_file, query, symbols, node_at.")
                .arg(op)};
}

// ── parse_file ───────────────────────────────────────────────────────────────

ToolExecResult TreeSitterQueryTool::parseFile(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};

    const int maxDepth = args[QLatin1String("maxDepth")].toInt(-1);

    const QString ast = m_fileParser(filePath, maxDepth);
    if (ast.isEmpty())
        return {false, {}, {},
                QStringLiteral("Failed to parse '%1'. File may not exist or "
                               "language is not supported by tree-sitter.")
                    .arg(QFileInfo(filePath).fileName())};

    // Truncate very large ASTs
    QString text = ast;
    constexpr int kMaxLen = 60 * 1024;
    if (text.size() > kMaxLen)
        text = text.left(kMaxLen) + QStringLiteral("\n... (truncated)");

    return {true, {}, text, {}};
}

// ── query ────────────────────────────────────────────────────────────────────

ToolExecResult TreeSitterQueryTool::runQuery(const QJsonObject &args)
{
    const QString filePath     = args[QLatin1String("filePath")].toString();
    const QString queryPattern = args[QLatin1String("queryPattern")].toString();

    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};
    if (queryPattern.isEmpty())
        return {false, {}, {}, QStringLiteral("'queryPattern' is required for 'query' operation.")};

    const QueryResult result = m_queryRunner(filePath, queryPattern);

    if (!result.ok)
        return {false, {}, {}, result.error};

    if (result.matches.isEmpty())
        return {true, {}, QStringLiteral("No matches found."), {}};

    QStringList lines;
    QJsonArray matchArr;
    constexpr int kMaxMatches = 200;
    const int showCount = qMin(result.matchCount, kMaxMatches);

    for (int i = 0; i < showCount && i < result.matches.size(); ++i) {
        const auto &m = result.matches[i];
        lines << QStringLiteral("@%1  %2  L%3:%4-%5:%6  %7")
                     .arg(m.captureName, m.nodeType)
                     .arg(m.startLine + 1).arg(m.startCol + 1)
                     .arg(m.endLine + 1).arg(m.endCol + 1)
                     .arg(m.text.left(120));

        QJsonObject obj;
        obj[QLatin1String("capture")]   = m.captureName;
        obj[QLatin1String("nodeType")]  = m.nodeType;
        obj[QLatin1String("startLine")] = m.startLine + 1;
        obj[QLatin1String("startCol")]  = m.startCol + 1;
        obj[QLatin1String("endLine")]   = m.endLine + 1;
        obj[QLatin1String("endCol")]    = m.endCol + 1;
        obj[QLatin1String("text")]      = m.text.left(200);
        matchArr.append(obj);
    }

    QString text = QStringLiteral("%1 match(es):\n").arg(result.matchCount);
    text += lines.join('\n');
    if (result.matchCount > kMaxMatches)
        text += QStringLiteral("\n... (%1 more matches not shown)")
                    .arg(result.matchCount - kMaxMatches);

    QJsonObject data;
    data[QLatin1String("matchCount")] = result.matchCount;
    data[QLatin1String("matches")]    = matchArr;

    return {true, data, text, {}};
}

// ── symbols ──────────────────────────────────────────────────────────────────

ToolExecResult TreeSitterQueryTool::extractSymbols(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};

    const QList<SymbolInfo> symbols = m_symbolExtractor(filePath);

    if (symbols.isEmpty())
        return {true, {},
                QStringLiteral("No symbols found in '%1'.")
                    .arg(QFileInfo(filePath).fileName()), {}};

    QStringList lines;
    QJsonArray arr;
    for (const auto &sym : symbols) {
        lines << QStringLiteral("  %1  %2  L%3-%4")
                     .arg(sym.kind, -10)
                     .arg(sym.name)
                     .arg(sym.startLine + 1)
                     .arg(sym.endLine + 1);

        QJsonObject obj;
        obj[QLatin1String("name")]      = sym.name;
        obj[QLatin1String("kind")]      = sym.kind;
        obj[QLatin1String("startLine")] = sym.startLine + 1;
        obj[QLatin1String("endLine")]   = sym.endLine + 1;
        arr.append(obj);
    }

    QString text = QStringLiteral("%1 symbol(s) in %2:\n")
                       .arg(symbols.size())
                       .arg(QFileInfo(filePath).fileName());
    text += lines.join('\n');

    QJsonObject data;
    data[QLatin1String("symbolCount")] = symbols.size();
    data[QLatin1String("symbols")]     = arr;

    return {true, data, text, {}};
}

// ── node_at ──────────────────────────────────────────────────────────────────

ToolExecResult TreeSitterQueryTool::nodeAt(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    const int line   = args[QLatin1String("line")].toInt(0);
    const int column = args[QLatin1String("column")].toInt(0);

    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};
    if (line <= 0)
        return {false, {}, {}, QStringLiteral("'line' (1-based) is required for 'node_at'.")};

    // Convert from 1-based (agent) to 0-based (tree-sitter)
    const NodeInfo node = m_nodeAtPosition(filePath, line - 1, qMax(0, column - 1));

    if (node.type.isEmpty())
        return {false, {}, {},
                QStringLiteral("No AST node found at %1:%2:%3. "
                               "File may not exist or position is out of range.")
                    .arg(QFileInfo(filePath).fileName())
                    .arg(line).arg(column)};

    QString text = QStringLiteral("Node at L%1:%2:\n"
                                  "  type: %3\n"
                                  "  named: %4\n"
                                  "  range: L%5:%6 - L%7:%8\n"
                                  "  children: %9\n"
                                  "  text: %10")
                       .arg(line).arg(column)
                       .arg(node.type)
                       .arg(node.isNamed ? QStringLiteral("yes") : QStringLiteral("no"))
                       .arg(node.startLine + 1).arg(node.startCol + 1)
                       .arg(node.endLine + 1).arg(node.endCol + 1)
                       .arg(node.childCount)
                       .arg(node.text.left(500));

    QJsonObject data;
    data[QLatin1String("type")]       = node.type;
    data[QLatin1String("isNamed")]    = node.isNamed;
    data[QLatin1String("startLine")]  = node.startLine + 1;
    data[QLatin1String("startCol")]   = node.startCol + 1;
    data[QLatin1String("endLine")]    = node.endLine + 1;
    data[QLatin1String("endCol")]     = node.endCol + 1;
    data[QLatin1String("childCount")] = node.childCount;
    data[QLatin1String("text")]       = node.text.left(500);

    return {true, data, text, {}};
}
