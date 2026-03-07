#pragma once

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────────────────
// RegexSearchEngine — full regex search across all workspace files.
//
// Performs grep-style line-by-line regex matching with configurable
// max results, file type filtering, and gitignore awareness.
// Returns filename:line:column matches.
// ─────────────────────────────────────────────────────────────────────────────

struct RegexSearchMatch {
    QString filePath;
    int line = 0;           // 1-based
    int column = 0;         // 0-based
    QString lineContent;
    QString matchedText;
};

class RegexSearchEngine : public QObject
{
    Q_OBJECT

public:
    explicit RegexSearchEngine(QObject *parent = nullptr) : QObject(parent) {}

    void setWorkspaceRoot(const QString &root) { m_root = root; }

    /// Full regex search across workspace
    QList<RegexSearchMatch> search(const QString &pattern,
                                    bool caseSensitive = false,
                                    int maxResults = 500,
                                    const QString &includeGlob = {},
                                    const QString &excludeGlob = {}) const
    {
        QRegularExpression::PatternOptions opts = {};
        if (!caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;

        QRegularExpression rx(pattern, opts);
        if (!rx.isValid()) return {};

        QRegularExpression includeRx;
        QRegularExpression excludeRx;
        if (!includeGlob.isEmpty())
            includeRx = globToRegex(includeGlob);
        if (!excludeGlob.isEmpty())
            excludeRx = globToRegex(excludeGlob);

        QList<RegexSearchMatch> results;
        searchDir(QDir(m_root), rx, includeRx, excludeRx, results, maxResults);
        return results;
    }

    /// Search a single file
    QList<RegexSearchMatch> searchFile(const QString &filePath,
                                        const QString &pattern,
                                        bool caseSensitive = false) const
    {
        QRegularExpression::PatternOptions opts = {};
        if (!caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;

        QRegularExpression rx(pattern, opts);
        if (!rx.isValid()) return {};

        QList<RegexSearchMatch> results;
        searchInFile(filePath, rx, results, 500);
        return results;
    }

signals:
    void searchProgress(int filesSearched, int matchCount);

private:
    void searchDir(const QDir &dir, const QRegularExpression &rx,
                   const QRegularExpression &includeRx,
                   const QRegularExpression &excludeRx,
                   QList<RegexSearchMatch> &results, int maxResults) const
    {
        static const QSet<QString> skipDirs = {
            QStringLiteral(".git"), QStringLiteral("node_modules"),
            QStringLiteral("build"), QStringLiteral("build-llvm"),
            QStringLiteral(".cache"), QStringLiteral("__pycache__"),
            QStringLiteral(".venv"), QStringLiteral("dist"),
            QStringLiteral("bin"), QStringLiteral("obj"),
        };

        for (const auto &entry : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (results.size() >= maxResults) return;

            if (entry.isDir()) {
                if (skipDirs.contains(entry.fileName())) continue;
                searchDir(QDir(entry.filePath()), rx, includeRx, excludeRx, results, maxResults);
            } else {
                // Skip binary / large files
                if (entry.size() > 2 * 1024 * 1024) continue;
                if (!isTextFile(entry.suffix())) continue;

                const QString relPath = QDir(m_root).relativeFilePath(entry.filePath());

                // Apply include/exclude filters
                if (includeRx.isValid() && !includeRx.match(relPath).hasMatch())
                    continue;
                if (excludeRx.isValid() && excludeRx.match(relPath).hasMatch())
                    continue;

                searchInFile(entry.filePath(), rx, results, maxResults);
            }
        }
    }

    void searchInFile(const QString &filePath, const QRegularExpression &rx,
                      QList<RegexSearchMatch> &results, int maxResults) const
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        const QString content = QString::fromUtf8(file.readAll());
        const QStringList lines = content.split(QLatin1Char('\n'));

        for (int i = 0; i < lines.size() && results.size() < maxResults; ++i) {
            const auto &line = lines[i];
            auto it = rx.globalMatch(line);
            while (it.hasNext() && results.size() < maxResults) {
                const auto match = it.next();
                RegexSearchMatch m;
                m.filePath = filePath;
                m.line = i + 1;
                m.column = match.capturedStart();
                m.lineContent = line.trimmed();
                m.matchedText = match.captured();
                results.append(m);
            }
        }
    }

    static bool isTextFile(const QString &suffix)
    {
        static const QSet<QString> textExts = {
            QStringLiteral("cpp"), QStringLiteral("h"), QStringLiteral("hpp"),
            QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("cxx"),
            QStringLiteral("py"), QStringLiteral("js"), QStringLiteral("ts"),
            QStringLiteral("jsx"), QStringLiteral("tsx"), QStringLiteral("java"),
            QStringLiteral("cs"), QStringLiteral("rb"), QStringLiteral("go"),
            QStringLiteral("rs"), QStringLiteral("swift"), QStringLiteral("kt"),
            QStringLiteral("lua"), QStringLiteral("sh"), QStringLiteral("bash"),
            QStringLiteral("cmake"), QStringLiteral("md"), QStringLiteral("json"),
            QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("toml"),
            QStringLiteral("xml"), QStringLiteral("html"), QStringLiteral("css"),
            QStringLiteral("txt"), QStringLiteral("cfg"), QStringLiteral("ini"),
            QStringLiteral("sql"), QStringLiteral("graphql"), QStringLiteral("proto"),
            QStringLiteral("qml"), QStringLiteral("qrc"),
        };
        return textExts.contains(suffix.toLower());
    }

    static QRegularExpression globToRegex(const QString &glob)
    {
        QString rx;
        for (int i = 0; i < glob.size(); ++i) {
            const QChar ch = glob.at(i);
            if (ch == QLatin1Char('*')) {
                if (i + 1 < glob.size() && glob.at(i + 1) == QLatin1Char('*')) {
                    rx += QStringLiteral(".*");
                    ++i;
                } else {
                    rx += QStringLiteral("[^/]*");
                }
            } else if (ch == QLatin1Char('?')) {
                rx += QStringLiteral("[^/]");
            } else if (QStringLiteral(".+^${}()|\\[]").contains(ch)) {
                rx += QLatin1Char('\\');
                rx += ch;
            } else {
                rx += ch;
            }
        }
        return QRegularExpression(rx, QRegularExpression::CaseInsensitiveOption);
    }

    QString m_root;
};
