#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QHash>
#include <QString>
#include <QVector>

#ifdef EXORCIST_HAS_TREESITTER
#include <tree_sitter/api.h>
#endif

class TreeSitterHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit TreeSitterHighlighter(QTextDocument *doc);
    ~TreeSitterHighlighter() override;

    /// Set the tree-sitter language for this highlighter.
    /// Returns false if lang is null.
    bool setLanguage(const TSLanguage *lang);

    /// Returns true if a tree-sitter language is active.
    bool hasLanguage() const;

    /// Force a full reparse (e.g., after bulk text replacement).
    void reparse();

protected:
    void highlightBlock(const QString &text) override;

private:
    void onContentsChange(int position, int charsRemoved, int charsAdded);
    void fullParse();
    void buildBlockByteOffsets();
    void visitNode(TSNode node, uint32_t startByte, uint32_t endByte);
    QTextCharFormat formatForNodeType(const char *type, bool isNamed) const;
    void buildFormatMap();

    // Byte↔char conversion helpers — no heap allocation.
    // charOffset is in Qt code units (UTF-16); for BMP-only content this equals
    // the Unicode code point count. 4-byte UTF-8 sequences (supplementary chars)
    // map to 2 Qt code units (surrogate pair).
    static uint32_t charToByteOffset(const QByteArray &utf8, int charOffset);
    static int      byteToCharOffset(const QByteArray &utf8, uint32_t byteOffset);

    friend class TestTreeSitterHighlighter;
#ifdef EXORCIST_HAS_TREESITTER
    static TSPoint  byteToPoint(const QByteArray &utf8, uint32_t byteOffset);
#endif

#ifdef EXORCIST_HAS_TREESITTER
    TSParser *m_parser = nullptr;
    TSTree   *m_tree   = nullptr;
#endif
    QByteArray        m_sourceUtf8;
    QVector<uint32_t> m_blockByteOffsets; // byte offset of each block's first char in m_sourceUtf8
    QHash<QString, QTextCharFormat> m_formatMap;
    bool m_parsing = false;
};
