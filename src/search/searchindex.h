#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

// ── SearchIndex ───────────────────────────────────────────────────────────────
//
// Lightweight file list index for workspace search.
// Maintains a filtered list of text files (skipping binary, build dirs, .git).
// No trigram index — the file list alone is enough for ripgrep-style
// line-by-line searching which is I/O-bound, not CPU-bound.

class SearchIndex
{
public:
    void build(const QString &rootPath);
    QStringList allFiles() const { return m_allFiles; }
    bool isReady() const { return !m_rootPath.isEmpty(); }
    QString rootPath() const { return m_rootPath; }

private:
    static bool shouldSkipDir(const QString &name);
    static bool isTextFile(const QString &suffix);

    QString m_rootPath;
    QStringList m_allFiles;
};
