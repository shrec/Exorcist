#pragma once

#include "langcommon.h"

// ── C-family ────────────────────────────────────────────────────────────────
void buildCpp(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildCsharp(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── Python ──────────────────────────────────────────────────────────────────
void buildPython(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── JavaScript / TypeScript ─────────────────────────────────────────────────
void buildJavaScript(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildTypeScript(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── Web ─────────────────────────────────────────────────────────────────────
void buildXml(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildCss(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildPhp(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── JVM languages ───────────────────────────────────────────────────────────
void buildJava(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildKotlin(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildScala(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildDart(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── Systems languages ───────────────────────────────────────────────────────
void buildRust(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildGo(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildZig(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildSwift(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── Scripting languages ─────────────────────────────────────────────────────
void buildLua(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildRuby(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildPerl(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildR(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildElixir(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildHaskell(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── Shell / terminal ────────────────────────────────────────────────────────
void buildShell(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildPowerShell(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildBatch(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── Data / config formats ───────────────────────────────────────────────────
void buildJson(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildYaml(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildToml(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildIni(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildEnv(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildSql(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);

// ── Build systems / markup / misc ───────────────────────────────────────────
void buildCmake(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildMakefile(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildDockerfile(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildQmake(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildSln(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildGlsl(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildMarkdown(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildGraphql(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildProtobuf(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
void buildTerraform(QVector<SyntaxHighlighter::Rule> &rules, QRegularExpression &blockStart, QRegularExpression &blockEnd, QTextCharFormat &blockFmt);
