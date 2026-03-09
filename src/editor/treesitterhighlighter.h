#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QHash>
#include <QString>

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
    void applyHighlighting(uint32_t startByte, uint32_t endByte);
    void visitNode(TSNode node, uint32_t startByte, uint32_t endByte);
    QTextCharFormat formatForNodeType(const char *type, bool isNamed) const;
    void buildFormatMap();

#ifdef EXORCIST_HAS_TREESITTER
    TSParser *m_parser = nullptr;
    TSTree   *m_tree   = nullptr;
#endif
    QByteArray m_sourceUtf8;
    QHash<QString, QTextCharFormat> m_formatMap;
    bool m_parsing = false;
};
