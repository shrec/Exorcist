#pragma once

#include <QRegularExpression>
#include <QStringList>
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

    /// Callback type for populating a language's highlighting rules.
    using LangBuildFn = void (*)(QVector<Rule> &rules,
                                 QRegularExpression &blockCommentStart,
                                 QRegularExpression &blockCommentEnd,
                                 QTextCharFormat    &blockCommentFmt);

    /// Language contribution registered by language-pack plugins.
    struct LanguageContribution
    {
        QStringList extensions;         ///< lowercase extensions without dot
        QStringList fileNames;          ///< lowercase exact file names
        QStringList shebangs;           ///< substrings to match in shebang line
        bool        hasBlockComments = false;
        LangBuildFn build            = nullptr;
    };

    /// Register a language — called by language-pack plugins during activate().
    static void registerLanguage(LanguageContribution contrib);

    /// Creates the right highlighter for the given file path.
    /// Returns nullptr if no highlighter applies (plain text).
    /// The highlighter is parented to `doc` and auto-deleted with it.
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
