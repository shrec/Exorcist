#pragma once

#include "../syntaxhighlighter.h"

#include <QRegularExpression>
#include <QTextCharFormat>
#include <QVector>

// ── Shared colour palette (VS Code dark theme) ──────────────────────────────

namespace clr
{
extern const QTextCharFormat keyword;    // blue, bold
extern const QTextCharFormat type_;      // teal
extern const QTextCharFormat string_;    // orange
extern const QTextCharFormat comment_;   // green
extern const QTextCharFormat number_;    // light green
extern const QTextCharFormat preproc_;   // purple
extern const QTextCharFormat function_;  // yellow
extern const QTextCharFormat special_;   // light blue
} // namespace clr

// ── Shared helpers ──────────────────────────────────────────────────────────

void addKeywords(QVector<SyntaxHighlighter::Rule> &rules,
                 const QStringList &words,
                 const QTextCharFormat &fmt);

void addRule(QVector<SyntaxHighlighter::Rule> &rules,
             const QString &pattern,
             const QTextCharFormat &fmt);

// ── Unified builder signature (alias to SyntaxHighlighter::LangBuildFn) ────

using LangBuildFn = SyntaxHighlighter::LangBuildFn;
