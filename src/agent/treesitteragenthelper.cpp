#include "treesitteragenthelper.h"

#include <QFile>
#include <QFileInfo>

#ifdef EXORCIST_HAS_TREESITTER
#include <tree_sitter/api.h>

extern "C" {
const TSLanguage *tree_sitter_c();
const TSLanguage *tree_sitter_cpp();
const TSLanguage *tree_sitter_python();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_typescript();
const TSLanguage *tree_sitter_rust();
const TSLanguage *tree_sitter_json();
const TSLanguage *tree_sitter_go();
const TSLanguage *tree_sitter_yaml();
const TSLanguage *tree_sitter_toml();
}

static const TSLanguage *languageForExt(const QString &ext)
{
    if (ext == QLatin1String("c"))
        return tree_sitter_c();
    if (ext == QLatin1String("cpp") || ext == QLatin1String("cxx") ||
        ext == QLatin1String("cc")  || ext == QLatin1String("h")   ||
        ext == QLatin1String("hpp") || ext == QLatin1String("hxx") ||
        ext == QLatin1String("inl") || ext == QLatin1String("mm"))
        return tree_sitter_cpp();
    if (ext == QLatin1String("py") || ext == QLatin1String("pyw"))
        return tree_sitter_python();
    if (ext == QLatin1String("js")  || ext == QLatin1String("mjs") ||
        ext == QLatin1String("cjs") || ext == QLatin1String("jsx"))
        return tree_sitter_javascript();
    if (ext == QLatin1String("ts") || ext == QLatin1String("tsx"))
        return tree_sitter_typescript();
    if (ext == QLatin1String("rs"))
        return tree_sitter_rust();
    if (ext == QLatin1String("json") || ext == QLatin1String("jsonc"))
        return tree_sitter_json();
    if (ext == QLatin1String("go"))
        return tree_sitter_go();
    if (ext == QLatin1String("yaml") || ext == QLatin1String("yml"))
        return tree_sitter_yaml();
    if (ext == QLatin1String("toml"))
        return tree_sitter_toml();
    return nullptr;
}

static QByteArray readFileUtf8(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

/// Recursively build an S-expression string from a TSNode.
/// maxDepth controls truncation (-1 = unlimited).
static void nodeToSexp(TSNode node, QString &out, int depth, int maxDepth)
{
    if (maxDepth >= 0 && depth > maxDepth) {
        out += QLatin1String("...");
        return;
    }

    const char *type = ts_node_type(node);
    const uint32_t childCount = ts_node_named_child_count(node);

    if (childCount == 0) {
        out += QLatin1Char('(');
        out += QString::fromUtf8(type);
        out += QLatin1Char(')');
        return;
    }

    out += QLatin1Char('(');
    out += QString::fromUtf8(type);
    for (uint32_t i = 0; i < childCount; ++i) {
        out += QLatin1Char(' ');
        nodeToSexp(ts_node_named_child(node, i), out, depth + 1, maxDepth);
    }
    out += QLatin1Char(')');
}

/// Extract text for a TSNode from the source buffer.
static QString nodeText(TSNode node, const QByteArray &source)
{
    const uint32_t start = ts_node_start_byte(node);
    const uint32_t end   = ts_node_end_byte(node);
    if (start >= static_cast<uint32_t>(source.size()))
        return {};
    const int len = qMin(static_cast<int>(end - start), source.size() - static_cast<int>(start));
    return QString::fromUtf8(source.mid(static_cast<int>(start), len));
}
#endif // EXORCIST_HAS_TREESITTER

// ── Construction ─────────────────────────────────────────────────────────────

TreeSitterAgentHelper::TreeSitterAgentHelper() = default;

// ── Implementation ───────────────────────────────────────────────────────────

QString TreeSitterAgentHelper::parseFile(const QString &filePath, int maxDepth)
{
#ifdef EXORCIST_HAS_TREESITTER
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const TSLanguage *lang = languageForExt(ext);
    if (!lang)
        return {};

    const QByteArray source = readFileUtf8(filePath);
    if (source.isEmpty())
        return {};

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, lang);

    TSTree *tree = ts_parser_parse_string(parser, nullptr,
                                          source.constData(),
                                          static_cast<uint32_t>(source.size()));
    if (!tree) {
        ts_parser_delete(parser);
        return {};
    }

    TSNode root = ts_tree_root_node(tree);
    QString result;
    result.reserve(source.size());
    nodeToSexp(root, result, 0, maxDepth);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return result;
#else
    Q_UNUSED(filePath)
    Q_UNUSED(maxDepth)
    return {};
#endif
}

TreeSitterQueryTool::QueryResult TreeSitterAgentHelper::runQuery(const QString &filePath,
                                                                  const QString &queryPattern)
{
#ifdef EXORCIST_HAS_TREESITTER
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const TSLanguage *lang = languageForExt(ext);
    if (!lang)
        return {false, {}, 0, QStringLiteral("No tree-sitter grammar for this file type.")};

    const QByteArray source = readFileUtf8(filePath);
    if (source.isEmpty())
        return {false, {}, 0, QStringLiteral("Could not read file.")};

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, lang);

    TSTree *tree = ts_parser_parse_string(parser, nullptr,
                                          source.constData(),
                                          static_cast<uint32_t>(source.size()));
    if (!tree) {
        ts_parser_delete(parser);
        return {false, {}, 0, QStringLiteral("Parse failed.")};
    }

    const QByteArray patternUtf8 = queryPattern.toUtf8();
    uint32_t errorOffset = 0;
    TSQueryError errorType = TSQueryErrorNone;

    TSQuery *query = ts_query_new(lang,
                                   patternUtf8.constData(),
                                   static_cast<uint32_t>(patternUtf8.size()),
                                   &errorOffset, &errorType);
    if (!query) {
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        return {false, {}, 0,
                QStringLiteral("Query parse error at offset %1 (type %2).")
                    .arg(errorOffset).arg(static_cast<int>(errorType))};
    }

    TSQueryCursor *cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query, ts_tree_root_node(tree));

    TSQueryMatch match;
    QList<TreeSitterQueryTool::QueryMatch> matches;
    constexpr int kMaxMatches = 500;

    while (ts_query_cursor_next_match(cursor, &match) && matches.size() < kMaxMatches) {
        for (uint16_t i = 0; i < match.capture_count; ++i) {
            uint32_t nameLen = 0;
            const char *capName = ts_query_capture_name_for_id(query, match.captures[i].index, &nameLen);
            const TSNode node = match.captures[i].node;
            const TSPoint start = ts_node_start_point(node);
            const TSPoint end   = ts_node_end_point(node);

            TreeSitterQueryTool::QueryMatch qm;
            qm.captureName = QString::fromUtf8(capName, static_cast<int>(nameLen));
            qm.nodeType    = QString::fromUtf8(ts_node_type(node));
            qm.startLine   = static_cast<int>(start.row);
            qm.startCol    = static_cast<int>(start.column);
            qm.endLine     = static_cast<int>(end.row);
            qm.endCol      = static_cast<int>(end.column);
            qm.text        = nodeText(node, source).left(200);
            matches.append(qm);
        }
    }

    const int totalMatches = matches.size();

    ts_query_cursor_delete(cursor);
    ts_query_delete(query);
    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return {true, matches, totalMatches, {}};
#else
    Q_UNUSED(filePath)
    Q_UNUSED(queryPattern)
    return {false, {}, 0, QStringLiteral("Tree-sitter not available.")};
#endif
}

QList<TreeSitterQueryTool::SymbolInfo> TreeSitterAgentHelper::extractSymbols(const QString &filePath)
{
#ifdef EXORCIST_HAS_TREESITTER
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const TSLanguage *lang = languageForExt(ext);
    if (!lang)
        return {};

    const QByteArray source = readFileUtf8(filePath);
    if (source.isEmpty())
        return {};

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, lang);

    TSTree *tree = ts_parser_parse_string(parser, nullptr,
                                          source.constData(),
                                          static_cast<uint32_t>(source.size()));
    if (!tree) {
        ts_parser_delete(parser);
        return {};
    }

    // Walk top-level children looking for declaration nodes
    TSNode root = ts_tree_root_node(tree);
    const uint32_t childCount = ts_node_named_child_count(root);

    QList<TreeSitterQueryTool::SymbolInfo> symbols;

    for (uint32_t i = 0; i < childCount; ++i) {
        TSNode child = ts_node_named_child(root, i);
        const QString type = QString::fromUtf8(ts_node_type(child));

        QString kind;
        if (type == QLatin1String("function_definition") ||
            type == QLatin1String("function_declaration") ||
            type == QLatin1String("function_item"))
            kind = QStringLiteral("function");
        else if (type == QLatin1String("class_specifier") ||
                 type == QLatin1String("class_definition") ||
                 type == QLatin1String("class_declaration"))
            kind = QStringLiteral("class");
        else if (type == QLatin1String("struct_specifier") ||
                 type == QLatin1String("struct_item"))
            kind = QStringLiteral("struct");
        else if (type == QLatin1String("enum_specifier") ||
                 type == QLatin1String("enum_item"))
            kind = QStringLiteral("enum");
        else if (type == QLatin1String("method_definition"))
            kind = QStringLiteral("method");
        else if (type == QLatin1String("declaration") ||
                 type == QLatin1String("global_statement"))
            kind = QStringLiteral("variable");
        else if (type == QLatin1String("impl_item"))
            kind = QStringLiteral("impl");
        else if (type == QLatin1String("namespace_definition"))
            kind = QStringLiteral("namespace");
        else
            continue;

        // Try to extract the name from a "name" or "declarator" field
        QString name;
        TSNode nameNode = ts_node_child_by_field_name(child, "name", 4);
        if (ts_node_is_null(nameNode))
            nameNode = ts_node_child_by_field_name(child, "declarator", 10);
        if (!ts_node_is_null(nameNode))
            name = nodeText(nameNode, source).trimmed();

        if (name.isEmpty())
            name = QStringLiteral("<anonymous>");

        const TSPoint start = ts_node_start_point(child);
        const TSPoint end   = ts_node_end_point(child);

        symbols.append({
            name, kind,
            static_cast<int>(start.row),
            static_cast<int>(end.row),
            static_cast<int>(start.column)
        });
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return symbols;
#else
    Q_UNUSED(filePath)
    return {};
#endif
}

TreeSitterQueryTool::NodeInfo TreeSitterAgentHelper::nodeAtPosition(const QString &filePath,
                                                                     int line, int column)
{
#ifdef EXORCIST_HAS_TREESITTER
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const TSLanguage *lang = languageForExt(ext);
    if (!lang)
        return {};

    const QByteArray source = readFileUtf8(filePath);
    if (source.isEmpty())
        return {};

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, lang);

    TSTree *tree = ts_parser_parse_string(parser, nullptr,
                                          source.constData(),
                                          static_cast<uint32_t>(source.size()));
    if (!tree) {
        ts_parser_delete(parser);
        return {};
    }

    TSPoint point;
    point.row    = static_cast<uint32_t>(line);
    point.column = static_cast<uint32_t>(column);

    TSNode node = ts_node_descendant_for_point_range(
        ts_tree_root_node(tree), point, point);

    TreeSitterQueryTool::NodeInfo info;

    if (!ts_node_is_null(node)) {
        // Walk up to find the deepest named node
        while (!ts_node_is_named(node) && !ts_node_is_null(ts_node_parent(node))) {
            node = ts_node_parent(node);
        }

        info.type       = QString::fromUtf8(ts_node_type(node));
        info.isNamed    = ts_node_is_named(node);
        info.startLine  = static_cast<int>(ts_node_start_point(node).row);
        info.startCol   = static_cast<int>(ts_node_start_point(node).column);
        info.endLine    = static_cast<int>(ts_node_end_point(node).row);
        info.endCol     = static_cast<int>(ts_node_end_point(node).column);
        info.text       = nodeText(node, source);
        info.childCount = static_cast<int>(ts_node_named_child_count(node));
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return info;
#else
    Q_UNUSED(filePath)
    Q_UNUSED(line)
    Q_UNUSED(column)
    return {};
#endif
}
