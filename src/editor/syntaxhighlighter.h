#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    struct Rule
    {
        QRegularExpression pattern;
        QTextCharFormat    format;
    };
    // Creates the right highlighter for the given file path.
    // Returns nullptr if no highlighter applies (plain text).
    // The highlighter is parented to `doc` and auto-deleted with it.
    static SyntaxHighlighter *create(const QString &filePath, QTextDocument *doc);

protected:
    explicit SyntaxHighlighter(QTextDocument *doc);
    void highlightBlock(const QString &text) override;

    QVector<Rule>      m_rules;
    QRegularExpression m_blockCommentStart;
    QRegularExpression m_blockCommentEnd;
    QTextCharFormat    m_blockCommentFmt;
    bool               m_hasBlockComments = false;
};
