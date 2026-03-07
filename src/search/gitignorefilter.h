#pragma once

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextStream>

// ─────────────────────────────────────────────────────────────────────────────
// GitignoreFilter — parses .gitignore files and filters paths.
//
// Supports nested .gitignore files, negation patterns (!), directory-only
// patterns (trailing /), wildcard (*) and double-star (**) patterns.
// Used by workspace indexing to skip ignored files.
// ─────────────────────────────────────────────────────────────────────────────

class GitignoreFilter
{
public:
    /// Load patterns from a .gitignore file
    void loadFile(const QString &gitignorePath)
    {
        QFile file(gitignorePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        const QString basePath = QFileInfo(gitignorePath).absolutePath();
        QTextStream stream(&file);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;
            addPattern(line, basePath);
        }
    }

    /// Load patterns from workspace root (recursive)
    void loadRecursive(const QString &rootPath)
    {
        loadFile(rootPath + QStringLiteral("/.gitignore"));
        scanForGitignores(QDir(rootPath), rootPath);
    }

    /// Add a single pattern
    void addPattern(const QString &pattern, const QString &basePath = {})
    {
        Pattern p;
        p.basePath = basePath;
        p.original = pattern;
        p.negated = pattern.startsWith(QLatin1Char('!'));

        QString pat = p.negated ? pattern.mid(1) : pattern;

        // Directory-only pattern
        if (pat.endsWith(QLatin1Char('/'))) {
            p.dirOnly = true;
            pat.chop(1);
        }

        // Convert glob to regex
        p.regex = globToRegex(pat);
        m_patterns.append(p);
    }

    /// Add extra exclude patterns (user-configured)
    void addExcludePatterns(const QStringList &patterns)
    {
        for (const auto &p : patterns)
            addPattern(p);
    }

    /// Check if a path should be ignored
    bool isIgnored(const QString &relativePath, bool isDir = false) const
    {
        bool ignored = false;
        for (const auto &p : m_patterns) {
            if (p.dirOnly && !isDir) continue;

            bool matches = false;
            if (p.regex.isValid()) {
                // Match against relative path and also just the filename
                matches = p.regex.match(relativePath).hasMatch() ||
                          p.regex.match(QFileInfo(relativePath).fileName()).hasMatch();
            }

            if (matches) {
                ignored = !p.negated;
            }
        }
        return ignored;
    }

    /// Filter a list of paths, returning only non-ignored ones
    QStringList filterPaths(const QStringList &paths, const QString &rootPath) const
    {
        QStringList result;
        for (const auto &path : paths) {
            QString rel = path;
            if (rel.startsWith(rootPath))
                rel = rel.mid(rootPath.size() + 1);
            if (!isIgnored(rel, QFileInfo(path).isDir()))
                result << path;
        }
        return result;
    }

    int patternCount() const { return m_patterns.size(); }

    void clear() { m_patterns.clear(); }

private:
    struct Pattern {
        QString basePath;
        QString original;
        QRegularExpression regex;
        bool negated = false;
        bool dirOnly = false;
    };

    static QRegularExpression globToRegex(const QString &glob)
    {
        QString rx;
        int i = 0;
        const int len = glob.size();

        // If pattern doesn't contain /, match against basename only
        bool hasSlash = glob.contains(QLatin1Char('/'));
        if (!hasSlash) rx += QStringLiteral("(?:^|/)");

        while (i < len) {
            const QChar ch = glob.at(i);
            if (ch == QLatin1Char('*')) {
                if (i + 1 < len && glob.at(i + 1) == QLatin1Char('*')) {
                    // ** matches everything including /
                    rx += QStringLiteral(".*");
                    i += 2;
                    if (i < len && glob.at(i) == QLatin1Char('/')) ++i;
                } else {
                    // * matches everything except /
                    rx += QStringLiteral("[^/]*");
                    ++i;
                }
            } else if (ch == QLatin1Char('?')) {
                rx += QStringLiteral("[^/]");
                ++i;
            } else if (ch == QLatin1Char('[')) {
                // Character class
                rx += QLatin1Char('[');
                ++i;
                while (i < len && glob.at(i) != QLatin1Char(']')) {
                    rx += glob.at(i);
                    ++i;
                }
                if (i < len) { rx += QLatin1Char(']'); ++i; }
            } else {
                // Escape regex special chars
                if (QStringLiteral(".+^${}()|\\").contains(ch))
                    rx += QLatin1Char('\\');
                rx += ch;
                ++i;
            }
        }
        rx += QLatin1Char('$');

        return QRegularExpression(rx);
    }

    void scanForGitignores(const QDir &dir, const QString &rootPath)
    {
        for (const auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (entry.fileName() == QStringLiteral(".git")) continue;
            const QString subPath = entry.filePath();
            const QString gitignore = subPath + QStringLiteral("/.gitignore");
            if (QFile::exists(gitignore))
                loadFile(gitignore);
            scanForGitignores(QDir(subPath), rootPath);
        }
    }

    QList<Pattern> m_patterns;
};
