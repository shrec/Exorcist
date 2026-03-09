#include "langcommon.h"

// ── Colour palette ──────────────────────────────────────────────────────────

static QTextCharFormat makeFmt(const QColor &fg, bool bold = false)
{
    QTextCharFormat f;
    f.setForeground(fg);
    if (bold)
        f.setFontWeight(QFont::Bold);
    return f;
}

namespace clr
{
const QTextCharFormat keyword   = makeFmt(QColor(0x56, 0x9C, 0xD6), true);  // blue
const QTextCharFormat type_     = makeFmt(QColor(0x4E, 0xC9, 0xB0));         // teal
const QTextCharFormat string_   = makeFmt(QColor(0xCE, 0x91, 0x78));         // orange
const QTextCharFormat comment_  = makeFmt(QColor(0x6A, 0x99, 0x55));         // green
const QTextCharFormat number_   = makeFmt(QColor(0xB5, 0xCE, 0xA8));         // light green
const QTextCharFormat preproc_  = makeFmt(QColor(0xC5, 0x86, 0xC0));         // purple
const QTextCharFormat function_ = makeFmt(QColor(0xDC, 0xDC, 0xAA));         // yellow
const QTextCharFormat special_  = makeFmt(QColor(0x9C, 0xDC, 0xFE));         // light blue
} // namespace clr

// ── Helpers ─────────────────────────────────────────────────────────────────

void addKeywords(QVector<SyntaxHighlighter::Rule> &rules,
                 const QStringList &words,
                 const QTextCharFormat &fmt)
{
    if (words.isEmpty())
        return;
    QString pattern = QStringLiteral("\\b(?:");
    for (int i = 0; i < words.size(); ++i) {
        if (i > 0)
            pattern += QLatin1Char('|');
        pattern += QRegularExpression::escape(words[i]);
    }
    pattern += QStringLiteral(")\\b");
    SyntaxHighlighter::Rule r;
    r.pattern = QRegularExpression(pattern);
    r.format  = fmt;
    rules.append(r);
}

void addRule(QVector<SyntaxHighlighter::Rule> &rules,
             const QString &pattern,
             const QTextCharFormat &fmt)
{
    SyntaxHighlighter::Rule r;
    r.pattern = QRegularExpression(pattern);
    r.format  = fmt;
    rules.append(r);
}
