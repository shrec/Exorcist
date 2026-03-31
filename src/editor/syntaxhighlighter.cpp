#include "syntaxhighlighter.h"

#include <QFileInfo>
#include <QVector>

// ── Contribution registry ─────────────────────────────────────────────────────

static QVector<SyntaxHighlighter::LanguageContribution> &registry()
{
    static QVector<SyntaxHighlighter::LanguageContribution> r;
    return r;
}

void SyntaxHighlighter::registerLanguage(LanguageContribution contrib)
{
    registry().append(std::move(contrib));
}

// ── Lookup helpers ────────────────────────────────────────────────────────────

static const SyntaxHighlighter::LanguageContribution *findByExtension(const QString &ext)
{
    for (const auto &c : registry()) {
        if (c.extensions.contains(ext))
            return &c;
    }
    return nullptr;
}

static const SyntaxHighlighter::LanguageContribution *findByFileName(const QString &name)
{
    for (const auto &c : registry()) {
        if (c.fileNames.contains(name))
            return &c;
    }
    return nullptr;
}

static const SyntaxHighlighter::LanguageContribution *findByShebang(QTextDocument *doc)
{
    if (!doc || doc->blockCount() < 1)
        return nullptr;
    const QString first = doc->firstBlock().text();
    if (!first.startsWith(QLatin1String("#!")))
        return nullptr;

    for (const auto &c : registry()) {
        for (const QString &shebang : c.shebangs) {
            if (first.contains(shebang))
                return &c;
        }
    }
    return nullptr;
}

// ── SyntaxHighlighter ────────────────────────────────────────────────────────

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
}

SyntaxHighlighter *SyntaxHighlighter::create(const QString &filePath, QTextDocument *doc)
{
    const QString ext  = QFileInfo(filePath).suffix().toLower();
    const QString name = QFileInfo(filePath).fileName().toLower();

    // 1) Try exact filename match
    const LanguageContribution *entry = findByFileName(name);

    // 2) Try file extension
    if (!entry)
        entry = findByExtension(ext);

    // 3) Try shebang line
    if (!entry)
        entry = findByShebang(doc);

    if (!entry || !entry->build)
        return nullptr;

    auto *h = new SyntaxHighlighter(doc);
    entry->build(h->m_rules, h->m_blockCommentStart,
                 h->m_blockCommentEnd, h->m_blockCommentFmt);
    h->m_hasBlockComments = entry->hasBlockComments;
    return h;
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    // ── Single-line rules ────────────────────────────────────────────────
    for (const Rule &rule : std::as_const(m_rules)) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), rule.format);
        }
    }

    // ── Multi-line block comments (state 0 = normal, 1 = inside block) ──
    if (!m_hasBlockComments)
        return;

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = static_cast<int>(
            m_blockCommentStart.match(text).capturedStart());

    while (startIndex >= 0) {
        const QRegularExpressionMatch endMatch =
            m_blockCommentEnd.match(text, startIndex + 2);

        int endIndex = static_cast<int>(endMatch.capturedStart());
        int commentLen;
        if (endIndex < 0) {
            setCurrentBlockState(1);
            commentLen = text.length() - startIndex;
        } else {
            commentLen = endIndex - startIndex + static_cast<int>(endMatch.capturedLength());
        }
        setFormat(startIndex, commentLen, m_blockCommentFmt);

        const QRegularExpressionMatch nextStart =
            m_blockCommentStart.match(text, startIndex + commentLen);
        startIndex = static_cast<int>(nextStart.capturedStart());
    }
}
