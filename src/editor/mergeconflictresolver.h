#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>

// ─────────────────────────────────────────────────────────────────────────────
// MergeConflictResolver — detects and helps resolve git merge conflicts.
//
// Scans files for <<<<<<< / ======= / >>>>>>> markers, extracts both sides,
// and provides methods for the AI agent to resolve conflicts.
// ─────────────────────────────────────────────────────────────────────────────

struct MergeConflict {
    QString filePath;
    int startLine = 0;       // line of <<<<<<<
    int separatorLine = 0;   // line of =======
    int endLine = 0;         // line of >>>>>>>
    QString oursLabel;       // label after <<<<<<< (e.g. HEAD)
    QString theirsLabel;     // label after >>>>>>> (e.g. feature-branch)
    QString oursContent;     // code between <<<<<<< and =======
    QString theirsContent;   // code between ======= and >>>>>>>

    bool isValid() const { return startLine > 0 && endLine > startLine; }
};

class MergeConflictResolver : public QObject
{
    Q_OBJECT

public:
    explicit MergeConflictResolver(QObject *parent = nullptr)
        : QObject(parent) {}

    /// Detect merge conflicts in a file by scanning for markers
    QList<MergeConflict> detectConflicts(const QString &filePath) const
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};

        QTextStream in(&file);
        const QStringList lines = in.readAll().split('\n');
        file.close();

        return parseConflicts(filePath, lines);
    }

    /// Detect conflicts in text content (for editor buffer)
    QList<MergeConflict> detectConflictsInText(const QString &filePath,
                                                const QString &text) const
    {
        return parseConflicts(filePath, text.split('\n'));
    }

    /// Check if file has any merge conflicts
    static bool hasConflicts(const QString &text)
    {
        return text.contains(QStringLiteral("<<<<<<<"));
    }

    /// Resolve a conflict by accepting "ours" (HEAD) side
    static QString resolveOurs(const QString &text, const MergeConflict &conflict)
    {
        return replaceConflict(text, conflict, conflict.oursContent);
    }

    /// Resolve a conflict by accepting "theirs" side
    static QString resolveTheirs(const QString &text, const MergeConflict &conflict)
    {
        return replaceConflict(text, conflict, conflict.theirsContent);
    }

    /// Resolve a conflict by accepting both sides concatenated
    static QString resolveBoth(const QString &text, const MergeConflict &conflict)
    {
        return replaceConflict(text, conflict,
                               conflict.oursContent + QStringLiteral("\n") +
                               conflict.theirsContent);
    }

    /// Resolve a conflict with custom resolution text
    static QString resolveCustom(const QString &text,
                                  const MergeConflict &conflict,
                                  const QString &resolution)
    {
        return replaceConflict(text, conflict, resolution);
    }

    /// Scan workspace for files with conflict markers
    QStringList findConflictFiles(const QString &workspaceRoot) const
    {
        QStringList result;
        scanDir(workspaceRoot, result);
        return result;
    }

signals:
    void conflictsFound(const QString &filePath, int count);
    void conflictResolved(const QString &filePath);

private:
    static QList<MergeConflict> parseConflicts(const QString &filePath,
                                                const QStringList &lines)
    {
        QList<MergeConflict> conflicts;
        MergeConflict current;
        QStringList oursLines, theirsLines;
        bool inOurs = false, inTheirs = false;

        for (int i = 0; i < lines.size(); ++i) {
            const QString &line = lines[i];

            if (line.startsWith(QStringLiteral("<<<<<<<"))) {
                current = {};
                current.filePath = filePath;
                current.startLine = i + 1; // 1-based
                current.oursLabel = line.mid(7).trimmed();
                oursLines.clear();
                theirsLines.clear();
                inOurs = true;
                inTheirs = false;
            } else if (line.startsWith(QStringLiteral("=======")) && inOurs) {
                current.separatorLine = i + 1;
                inOurs = false;
                inTheirs = true;
            } else if (line.startsWith(QStringLiteral(">>>>>>>"))) {
                if (inTheirs) {
                    current.endLine = i + 1;
                    current.theirsLabel = line.mid(7).trimmed();
                    current.oursContent = oursLines.join('\n');
                    current.theirsContent = theirsLines.join('\n');
                    conflicts.append(current);
                }
                inOurs = false;
                inTheirs = false;
            } else if (inOurs) {
                oursLines << line;
            } else if (inTheirs) {
                theirsLines << line;
            }
        }

        return conflicts;
    }

    static QString replaceConflict(const QString &text,
                                    const MergeConflict &conflict,
                                    const QString &resolution)
    {
        QStringList lines = text.split('\n');
        if (conflict.startLine < 1 || conflict.endLine > lines.size())
            return text;

        // Remove lines from startLine to endLine (1-based)
        const int start = conflict.startLine - 1;
        const int count = conflict.endLine - conflict.startLine + 1;

        for (int i = 0; i < count; ++i)
            lines.removeAt(start);

        // Insert resolution at the same position
        const QStringList resLines = resolution.split('\n');
        for (int i = 0; i < resLines.size(); ++i)
            lines.insert(start + i, resLines[i]);

        return lines.join('\n');
    }

    void scanDir(const QString &dir, QStringList &result) const
    {
        QDir d(dir);
        if (!d.exists()) return;

        // Skip common non-source directories
        const QString dirName = d.dirName();
        if (dirName == QStringLiteral(".git") ||
            dirName == QStringLiteral("node_modules") ||
            dirName == QStringLiteral("build") ||
            dirName == QStringLiteral("build-llvm"))
            return;

        const auto files = d.entryInfoList(QDir::Files);
        for (const QFileInfo &fi : files) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QByteArray head = f.read(65536); // Read first 64K
                if (head.contains("<<<<<<<")) {
                    result << fi.absoluteFilePath();
                }
                f.close();
            }
        }

        const auto dirs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &di : dirs)
            scanDir(di.absoluteFilePath(), result);
    }
};
