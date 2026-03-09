#include "treesitterhighlighter.h"

#include <QTextDocument>
#include <QTextBlock>

// ── Byte↔char conversion helpers ────────────────────────────────────────────
//
// UTF-8 encoding rules:
//   0xxxxxxx              → 1 byte  = 1 Qt char
//   110xxxxx 10xxxxxx     → 2 bytes = 1 Qt char
//   1110xxxx 10xxxxxx×2   → 3 bytes = 1 Qt char
//   11110xxx 10xxxxxx×3   → 4 bytes = 2 Qt chars (surrogate pair in UTF-16)
//
// charOffset is always in Qt UTF-16 code units (QString::length() units).

uint32_t TreeSitterHighlighter::charToByteOffset(const QByteArray &utf8, int charOffset)
{
    const char *data = utf8.constData();
    const int   size = utf8.size();
    int qtChars = 0;
    int i = 0;
    while (i < size && qtChars < charOffset) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x80)        { ++i; ++qtChars; }          // ASCII
        else if (c < 0xE0)   { i += 2; ++qtChars; }       // 2-byte
        else if (c < 0xF0)   { i += 3; ++qtChars; }       // 3-byte
        else                  { i += 4; qtChars += 2; }    // 4-byte → surrogate pair
    }
    return static_cast<uint32_t>(qMin(i, size));
}

int TreeSitterHighlighter::byteToCharOffset(const QByteArray &utf8, uint32_t byteOffset)
{
    const char *data = utf8.constData();
    const int   end  = qMin(static_cast<int>(byteOffset), utf8.size());
    int qtChars = 0;
    int i = 0;
    while (i < end) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x80)        { ++i; ++qtChars; }
        else if (c < 0xE0)   { i += 2; ++qtChars; }
        else if (c < 0xF0)   { i += 3; ++qtChars; }
        else                  { i += 4; qtChars += 2; }
    }
    return qtChars;
}

TSPoint TreeSitterHighlighter::byteToPoint(const QByteArray &utf8, uint32_t byteOffset)
{
    const char *data = utf8.constData();
    const int   end  = qMin(static_cast<int>(byteOffset), utf8.size());
    uint32_t row = 0, col = 0;
    for (int i = 0; i < end; ++i) {
        if (data[i] == '\n') { ++row; col = 0; }
        else                   ++col;
    }
    return {row, col};
}

// ── Main implementation ──────────────────────────────────────────────────────

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
    buildBlockByteOffsets();
}

void TreeSitterHighlighter::buildBlockByteOffsets()
{
    m_blockByteOffsets.clear();
    if (!document())
        return;

    uint32_t bytePos = 0;
    for (QTextBlock blk = document()->begin(); blk.isValid(); blk = blk.next()) {
        m_blockByteOffsets.append(bytePos);
        // block.text() excludes the paragraph separator; add the byte count
        // of the block text + 1 for the '\n' stored in m_sourceUtf8 (all but last block).
        bytePos += static_cast<uint32_t>(blk.text().toUtf8().size());
        if (blk.next().isValid())
            ++bytePos; // '\n' separator
    }
}

void TreeSitterHighlighter::onContentsChange(int position, int charsRemoved, int charsAdded)
{
    if (m_parsing || !m_tree || !m_parser)
        return;

    // m_sourceUtf8 still holds the OLD source at this point.
    const uint32_t startByte  = charToByteOffset(m_sourceUtf8, position);
    const uint32_t oldEndByte = charToByteOffset(m_sourceUtf8, position + charsRemoved);

    // Fetch the new text and compute the new end byte.
    const QByteArray newUtf8  = document()->toPlainText().toUtf8();
    const uint32_t newEndByte = charToByteOffset(newUtf8, position + charsAdded);

    // Tell tree-sitter exactly what changed BEFORE reparsing.
    // Without this call the old tree's byte ranges are stale and incremental
    // parsing produces incorrect results.
    TSInputEdit edit;
    edit.start_byte    = startByte;
    edit.old_end_byte  = oldEndByte;
    edit.new_end_byte  = newEndByte;
    edit.start_point   = byteToPoint(m_sourceUtf8, startByte);
    edit.old_end_point = byteToPoint(m_sourceUtf8, oldEndByte);
    edit.new_end_point = byteToPoint(newUtf8, newEndByte);
    ts_tree_edit(m_tree, &edit);

    // Update source, then reparse incrementally using the edited tree.
    m_sourceUtf8 = newUtf8;

    TSTree *oldTree = m_tree;
    m_tree = ts_parser_parse_string(m_parser, oldTree,
                                    m_sourceUtf8.constData(),
                                    static_cast<uint32_t>(m_sourceUtf8.size()));
    ts_tree_delete(oldTree);

    // Rebuild the per-block byte offset cache for the next highlightBlock() cycle.
    buildBlockByteOffsets();
}

void TreeSitterHighlighter::highlightBlock(const QString &text)
{
    Q_UNUSED(text)
    if (!m_tree)
        return;

    m_parsing = true;

    const QTextBlock block   = currentBlock();
    const int        blockNum = block.blockNumber();

    // Use the precomputed table — avoids toPlainText() on every block.
    const uint32_t startByte = (blockNum < m_blockByteOffsets.size())
                               ? m_blockByteOffsets[blockNum]
                               : static_cast<uint32_t>(m_sourceUtf8.size());
    const uint32_t endByte   = (blockNum + 1 < m_blockByteOffsets.size())
                               ? m_blockByteOffsets[blockNum + 1]
                               : static_cast<uint32_t>(m_sourceUtf8.size());

    TSNode root = ts_tree_root_node(m_tree);
    visitNode(root, startByte, endByte);

    m_parsing = false;
}

void TreeSitterHighlighter::visitNode(TSNode node, uint32_t startByte, uint32_t endByte)
{
    const uint32_t nodeStart = ts_node_start_byte(node);
    const uint32_t nodeEnd   = ts_node_end_byte(node);

    // Skip nodes completely outside our range.
    if (nodeEnd <= startByte || nodeStart >= endByte)
        return;

    const char *type     = ts_node_type(node);
    const bool  isNamed  = ts_node_is_named(node);

    const uint32_t childCount = ts_node_child_count(node);
    if (childCount == 0) {
        QTextCharFormat fmt = formatForNodeType(type, isNamed);
        if (fmt.isValid()) {
            const int blockCharPos = currentBlock().position();

            const uint32_t clampedStart = qMax(nodeStart, startByte);
            const uint32_t clampedEnd   = qMin(nodeEnd,   endByte);

            // Allocation-free byte→char conversion.
            const int charStart = byteToCharOffset(m_sourceUtf8, clampedStart);
            const int charEnd   = byteToCharOffset(m_sourceUtf8, clampedEnd);

            const int relStart = charStart - blockCharPos;
            const int relLen   = charEnd - charStart;

            if (relStart >= 0 && relLen > 0)
                setFormat(relStart, relLen, fmt);
        }
    }

    for (uint32_t i = 0; i < childCount; ++i)
        visitNode(ts_node_child(node, i), startByte, endByte);
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

// Stub implementations when tree-sitter is not compiled in.
// The byte↔char helpers above are compiled unconditionally (no tree-sitter types used).

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
void TreeSitterHighlighter::buildBlockByteOffsets() {}
void TreeSitterHighlighter::visitNode(TSNode, uint32_t, uint32_t) {}
QTextCharFormat TreeSitterHighlighter::formatForNodeType(const char *, bool) const { return {}; }
void TreeSitterHighlighter::buildFormatMap() {}

#endif
