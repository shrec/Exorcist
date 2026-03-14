#include "codegraphtool.h"

CodeGraphTool::CodeGraphTool(SymbolSearchFn symbolSearch,
                             SymbolsInFileFn symbolsInFile,
                             FindReferencesFn findRefs,
                             FindDefinitionFn findDef,
                             ChunkSearchFn chunkSearch)
    : m_symbolSearch(std::move(symbolSearch))
    , m_symbolsInFile(std::move(symbolsInFile))
    , m_findRefs(std::move(findRefs))
    , m_findDef(std::move(findDef))
    , m_chunkSearch(std::move(chunkSearch))
{}

ToolSpec CodeGraphTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("code_graph");
    s.description = QStringLiteral(
        "Unified code intelligence tool. Combines symbol search, "
        "reference lookup, and semantic code chunk search.\n\n"
        "Operations:\n"
        "- 'find_symbol': Search workspace symbols by name "
        "(classes, functions, structs, enums). Fast regex-based index.\n"
        "- 'find_references': Find all references to a symbol at a "
        "specific file:line:column position. Uses LSP (clangd).\n"
        "- 'find_definition': Go to definition of a symbol at "
        "file:line:column. Uses LSP.\n"
        "- 'symbols_in_file': List all symbols defined in a file.\n"
        "- 'semantic_chunks': Search code by keyword across semantic "
        "chunks (function-level boundaries). Returns relevant code "
        "snippets with file paths and line numbers.");
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
                    QStringLiteral("find_symbol"),
                    QStringLiteral("find_references"),
                    QStringLiteral("find_definition"),
                    QStringLiteral("symbols_in_file"),
                    QStringLiteral("semantic_chunks")
                }}
            }},
            {QStringLiteral("query"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Search query for find_symbol and semantic_chunks. "
                                "Symbol name or keyword to search for.")}
            }},
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("File path for find_references, find_definition, "
                                "and symbols_in_file.")}
            }},
            {QStringLiteral("line"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based line number for find_references and "
                                "find_definition.")}
            }},
            {QStringLiteral("column"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based column number for find_references and "
                                "find_definition.")}
            }},
            {QStringLiteral("maxResults"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum results to return (default: 30).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

ToolExecResult CodeGraphTool::invoke(const QJsonObject &args)
{
    const QString op = args[QLatin1String("operation")].toString();
    const int maxResults = args[QLatin1String("maxResults")].toInt(30);

    if (op == QLatin1String("find_symbol")) {
        const QString query = args[QLatin1String("query")].toString();
        if (query.isEmpty())
            return {false, {}, {}, QStringLiteral("'query' is required for find_symbol.")};
        const QString result = m_symbolSearch(query, maxResults);
        if (result.isEmpty())
            return {true, {}, QStringLiteral("No symbols matching '%1' found.").arg(query), {}};
        return {true, {}, result, {}};
    }

    if (op == QLatin1String("symbols_in_file")) {
        const QString filePath = args[QLatin1String("filePath")].toString();
        if (filePath.isEmpty())
            return {false, {}, {}, QStringLiteral("'filePath' is required for symbols_in_file.")};
        const QString result = m_symbolsInFile(filePath);
        if (result.isEmpty())
            return {true, {}, QStringLiteral("No symbols found in %1.").arg(filePath), {}};
        return {true, {}, result, {}};
    }

    if (op == QLatin1String("find_references")) {
        const QString filePath = args[QLatin1String("filePath")].toString();
        const int line   = args[QLatin1String("line")].toInt(-1);
        const int column = args[QLatin1String("column")].toInt(-1);
        if (filePath.isEmpty() || line < 1 || column < 1)
            return {false, {}, {},
                    QStringLiteral("'filePath', 'line', and 'column' are required for find_references.")};
        if (!m_findRefs)
            return {false, {}, {},
                    QStringLiteral("LSP not available. Start clangd or another language server.")};
        const QString result = m_findRefs(filePath, line, column);
        if (result.isEmpty())
            return {true, {}, QStringLiteral("No references found."), {}};
        return {true, {}, result, {}};
    }

    if (op == QLatin1String("find_definition")) {
        const QString filePath = args[QLatin1String("filePath")].toString();
        const int line   = args[QLatin1String("line")].toInt(-1);
        const int column = args[QLatin1String("column")].toInt(-1);
        if (filePath.isEmpty() || line < 1 || column < 1)
            return {false, {}, {},
                    QStringLiteral("'filePath', 'line', and 'column' are required for find_definition.")};
        if (!m_findDef)
            return {false, {}, {},
                    QStringLiteral("LSP not available.")};
        const QString result = m_findDef(filePath, line, column);
        if (result.isEmpty())
            return {true, {}, QStringLiteral("No definition found."), {}};
        return {true, {}, result, {}};
    }

    if (op == QLatin1String("semantic_chunks")) {
        const QString query = args[QLatin1String("query")].toString();
        if (query.isEmpty())
            return {false, {}, {}, QStringLiteral("'query' is required for semantic_chunks.")};
        if (!m_chunkSearch)
            return {false, {}, {},
                    QStringLiteral("Semantic index not built yet.")};
        const QString result = m_chunkSearch(query, maxResults);
        if (result.isEmpty())
            return {true, {}, QStringLiteral("No chunks matching '%1' found.").arg(query), {}};
        return {true, {}, result, {}};
    }

    return {false, {}, {},
            QStringLiteral("Unknown operation '%1'. Use: find_symbol, find_references, "
                           "find_definition, symbols_in_file, semantic_chunks").arg(op)};
}
