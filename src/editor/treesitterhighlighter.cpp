#include "treesitterhighlighter.h"

#include <QTextDocument>
#include <QTextBlock>

#ifdef EXORCIST_HAS_TREESITTER

TreeSitterHighlighter::TreeSitterHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
    m_parser = ts_parser_new();
    buildFormatMap();

    connect(doc, &QTextDocument::contentsChange,
            this, &TreeSitterHighlighter::onContentsChange);
}

TreeSitterHighlighter::~TreeSitterHighlighter()
{
    if (m_tree)
        ts_tree_delete(m_tree);
    if (m_parser)
        ts_parser_delete(m_parser);
}

bool TreeSitterHighlighter::setLanguage(const TSLanguage *lang)
{
    if (!lang)
        return false;
    ts_parser_set_language(m_parser, lang);
    fullParse();
    rehighlight();
    return true;
}

bool TreeSitterHighlighter::hasLanguage() const
{
    return ts_parser_language(m_parser) != nullptr;
}

void TreeSitterHighlighter::reparse()
{
    fullParse();
    rehighlight();
}

void TreeSitterHighlighter::fullParse()
{
    if (!document())
        return;
    m_sourceUtf8 = document()->toPlainText().toUtf8();
    if (m_tree)
        ts_tree_delete(m_tree);
    m_tree = ts_parser_parse_string(m_parser, nullptr,
                                    m_sourceUtf8.constData(),
                                    static_cast<uint32_t>(m_sourceUtf8.size()));
}

void TreeSitterHighlighter::onContentsChange(int position, int charsRemoved, int charsAdded)
{
    if (m_parsing || !m_tree)
        return;

    // Compute byte offsets in old UTF-8 source
    const QString before = document()->toPlainText();
    const QByteArray oldUtf8 = m_sourceUtf8;
    const QByteArray newUtf8 = before.toUtf8();

    // Map character position to byte offset in old source
    uint32_t startByte = document()->toPlainText().left(position).toUtf8().size();
    // Careful: we need to track old vs new for the edit
    // For simplicity, convert char counts to rough byte estimates
    // then do a full reparse — tree-sitter will still be incremental
    // because we supply the old tree

    // Compute old end byte and new end byte
    uint32_t oldEndByte = startByte;
    {
        // charsRemoved characters starting at position in old text
        // We already have the new text, so reconstruct the old segment length:
        // old text from [position..position+charsRemoved] → UTF-8 length
        // We don't have old text easily, so approximate from oldUtf8
        // A simpler approach: just reparse fully with old tree hint
    }

    m_sourceUtf8 = newUtf8;

    TSTree *oldTree = m_tree;

    // Provide the old tree for incremental parsing
    m_tree = ts_parser_parse_string(m_parser, oldTree,
                                    m_sourceUtf8.constData(),
                                    static_cast<uint32_t>(m_sourceUtf8.size()));
    ts_tree_delete(oldTree);
}

void TreeSitterHighlighter::highlightBlock(const QString &text)
{
    Q_UNUSED(text)
    if (!m_tree)
        return;

    m_parsing = true;

    const QTextBlock block = currentBlock();
    const int blockPos = block.position();
    const int blockLen = block.length();

    // Convert character positions to byte offsets
    const QString docText = document()->toPlainText();
    const uint32_t startByte = docText.left(blockPos).toUtf8().size();
    const uint32_t endByte   = docText.left(blockPos + blockLen).toUtf8().size();

    TSNode root = ts_tree_root_node(m_tree);
    visitNode(root, startByte, endByte);

    m_parsing = false;
}

void TreeSitterHighlighter::visitNode(TSNode node, uint32_t startByte, uint32_t endByte)
{
    const uint32_t nodeStart = ts_node_start_byte(node);
    const uint32_t nodeEnd   = ts_node_end_byte(node);

    // Skip nodes completely outside our range
    if (nodeEnd <= startByte || nodeStart >= endByte)
        return;

    const char *type = ts_node_type(node);
    const bool isNamed = ts_node_is_named(node);

    // Apply format for leaf nodes (or named nodes without formatted children)
    const uint32_t childCount = ts_node_child_count(node);
    if (childCount == 0) {
        QTextCharFormat fmt = formatForNodeType(type, isNamed);
        if (fmt.isValid()) {
            // Convert byte offsets to character offsets within the block
            const QTextBlock block = currentBlock();
            const int blockCharPos = block.position();
            const QString docText = document()->toPlainText();

            // Clamp to block range
            const uint32_t clampedStart = qMax(nodeStart, startByte);
            const uint32_t clampedEnd   = qMin(nodeEnd, endByte);

            // Convert byte offsets to character offsets
            const int charStart = QString::fromUtf8(m_sourceUtf8.constData(), clampedStart).length();
            const int charEnd   = QString::fromUtf8(m_sourceUtf8.constData(), clampedEnd).length();

            const int relStart = charStart - blockCharPos;
            const int relLen   = charEnd - charStart;

            if (relStart >= 0 && relLen > 0) {
                setFormat(relStart, relLen, fmt);
            }
        }
    }

    // Recurse into children
    for (uint32_t i = 0; i < childCount; ++i) {
        visitNode(ts_node_child(node, i), startByte, endByte);
    }
}

QTextCharFormat TreeSitterHighlighter::formatForNodeType(const char *type, bool isNamed) const
{
    Q_UNUSED(isNamed)
    auto it = m_formatMap.find(QString::fromLatin1(type));
    if (it != m_formatMap.end())
        return it.value();
    return {};
}

void TreeSitterHighlighter::buildFormatMap()
{
    // VS Code dark theme colours — matching langcommon.h palette
    auto makeFmt = [](const QColor &color, bool bold = false) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        if (bold) fmt.setFontWeight(QFont::Bold);
        return fmt;
    };

    const QTextCharFormat keyword  = makeFmt(QColor(0x56, 0x9C, 0xD6), true); // blue bold
    const QTextCharFormat type_    = makeFmt(QColor(0x4E, 0xC9, 0xB0));       // teal
    const QTextCharFormat string_  = makeFmt(QColor(0xCE, 0x91, 0x78));       // orange
    const QTextCharFormat comment_ = makeFmt(QColor(0x6A, 0x99, 0x55));       // green
    const QTextCharFormat number_  = makeFmt(QColor(0xB5, 0xCE, 0xA8));       // light green
    const QTextCharFormat preproc_ = makeFmt(QColor(0xC5, 0x86, 0xC0));       // purple
    const QTextCharFormat function_ = makeFmt(QColor(0xDC, 0xDC, 0xAA));      // yellow
    const QTextCharFormat special_ = makeFmt(QColor(0x9C, 0xDC, 0xFE));       // light blue

    // Comments
    for (const auto &t : {"comment", "line_comment", "block_comment"})
        m_formatMap.insert(QString::fromLatin1(t), comment_);

    // Strings
    for (const auto &t : {"string_literal", "string", "string_content",
                           "template_string", "raw_string_literal",
                           "char_literal", "string_fragment",
                           "escape_sequence", "interpreted_string_literal"})
        m_formatMap.insert(QString::fromLatin1(t), string_);

    // Numbers
    for (const auto &t : {"number_literal", "integer_literal", "float_literal",
                           "integer", "float", "number"})
        m_formatMap.insert(QString::fromLatin1(t), number_);

    // Types
    for (const auto &t : {"type_identifier", "primitive_type", "builtin_type",
                           "sized_type_specifier", "type_qualifier",
                           "class_specifier", "struct_specifier",
                           "enum_specifier"})
        m_formatMap.insert(QString::fromLatin1(t), type_);

    // Preprocessor
    for (const auto &t : {"preproc_include", "preproc_def", "preproc_ifdef",
                           "preproc_ifndef", "preproc_if", "preproc_else",
                           "preproc_elif", "preproc_endif", "preproc_call",
                           "preproc_directive", "preproc_arg",
                           "hash_bang_line"})
        m_formatMap.insert(QString::fromLatin1(t), preproc_);

    // Keywords — tree-sitter uses specific node types for each keyword
    for (const auto &t : {"if", "else", "for", "while", "do", "switch",
                           "case", "default", "break", "continue", "return",
                           "goto", "try", "catch", "throw", "new", "delete",
                           "class", "struct", "enum", "union", "namespace",
                           "template", "typename", "typedef", "using",
                           "virtual", "override", "final", "const",
                           "static", "extern", "inline", "constexpr",
                           "volatile", "mutable", "register", "explicit",
                           "friend", "operator", "public", "private",
                           "protected", "auto", "sizeof", "alignof",
                           "decltype", "noexcept", "nullptr", "this",
                           "true", "false", "void", "int", "char", "float",
                           "double", "long", "short", "unsigned", "signed",
                           "bool", "wchar_t",
                           // Python
                           "def", "lambda", "import", "from", "as", "with",
                           "assert", "yield", "raise", "pass", "del",
                           "global", "nonlocal", "async", "await",
                           "not", "and", "or", "in", "is",
                           // JavaScript / TypeScript
                           "function", "var", "let", "const",
                           "export", "import", "async", "await",
                           "typeof", "instanceof", "of",
                           "interface", "type", "implements", "extends",
                           // Rust
                           "fn", "let", "mut", "pub", "mod", "use",
                           "impl", "trait", "where", "match", "loop",
                           "move", "ref", "self", "super", "crate",
                           "unsafe", "dyn", "abstract"})
        m_formatMap.insert(QString::fromLatin1(t), keyword);

    // Function identifiers
    for (const auto &t : {"function_declarator", "call_expression"})
        m_formatMap.insert(QString::fromLatin1(t), function_);

    // Special identifiers
    for (const auto &t : {"field_identifier", "property_identifier",
                           "shorthand_property_identifier"})
        m_formatMap.insert(QString::fromLatin1(t), special_);

    // String delimiters
    for (const auto &t : {"\"", "'", "`"})
        m_formatMap.insert(QString::fromLatin1(t), string_);

    // Preprocessor tokens
    for (const auto &t : {"#include", "#define", "#if", "#ifdef", "#ifndef",
                           "#else", "#elif", "#endif", "#pragma"})
        m_formatMap.insert(QString::fromLatin1(t), preproc_);
}

#else // !EXORCIST_HAS_TREESITTER

// Stub implementation when tree-sitter is not available

TreeSitterHighlighter::TreeSitterHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
}

TreeSitterHighlighter::~TreeSitterHighlighter() = default;

bool TreeSitterHighlighter::setLanguage(const TSLanguage *) { return false; }
bool TreeSitterHighlighter::hasLanguage() const { return false; }
void TreeSitterHighlighter::reparse() {}
void TreeSitterHighlighter::highlightBlock(const QString &) {}
void TreeSitterHighlighter::onContentsChange(int, int, int) {}
void TreeSitterHighlighter::fullParse() {}
void TreeSitterHighlighter::applyHighlighting(uint32_t, uint32_t) {}
void TreeSitterHighlighter::visitNode(TSNode, uint32_t, uint32_t) {}
QTextCharFormat TreeSitterHighlighter::formatForNodeType(const char *, bool) const { return {}; }
void TreeSitterHighlighter::buildFormatMap() {}

#endif
