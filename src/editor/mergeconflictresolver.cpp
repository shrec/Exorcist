#include "mergeconflictresolver.h"

#include <QDir>
#include <QFileInfo>

MergeConflictResolver::MergeConflictResolver(QObject *parent)
    : QObject(parent)
{
}

QList<MergeConflict> MergeConflictResolver::detectConflicts(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    const QStringList lines = in.readAll().split('\n');
    file.close();

    return parseConflicts(filePath, lines);
}

QList<MergeConflict> MergeConflictResolver::detectConflictsInText(const QString &filePath,
                                                                   const QString &text) const
{
    return parseConflicts(filePath, text.split('\n'));
}

bool MergeConflictResolver::hasConflicts(const QString &text)
{
    return text.contains(QStringLiteral("<<<<<<<"));
}

QString MergeConflictResolver::resolveOurs(const QString &text, const MergeConflict &conflict)
{
    return replaceConflict(text, conflict, conflict.oursContent);
}

QString MergeConflictResolver::resolveTheirs(const QString &text, const MergeConflict &conflict)
{
    return replaceConflict(text, conflict, conflict.theirsContent);
}

QString MergeConflictResolver::resolveBoth(const QString &text, const MergeConflict &conflict)
{
    return replaceConflict(text, conflict,
                           conflict.oursContent + QStringLiteral("\n") +
                           conflict.theirsContent);
}

QString MergeConflictResolver::resolveCustom(const QString &text,
                                              const MergeConflict &conflict,
                                              const QString &resolution)
{
    return replaceConflict(text, conflict, resolution);
}

QStringList MergeConflictResolver::findConflictFiles(const QString &workspaceRoot) const
{
    QStringList result;
    scanDir(workspaceRoot, result);
    return result;
}

QList<MergeConflict> MergeConflictResolver::parseConflicts(const QString &filePath,
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
            current.startLine = i + 1;
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

QString MergeConflictResolver::replaceConflict(const QString &text,
                                                const MergeConflict &conflict,
                                                const QString &resolution)
{
    QStringList lines = text.split('\n');
    if (conflict.startLine < 1 || conflict.endLine > lines.size())
        return text;

    const int start = conflict.startLine - 1;
    const int count = conflict.endLine - conflict.startLine + 1;

    for (int i = 0; i < count; ++i)
        lines.removeAt(start);

    const QStringList resLines = resolution.split('\n');
    for (int i = 0; i < resLines.size(); ++i)
        lines.insert(start + i, resLines[i]);

    return lines.join('\n');
}

void MergeConflictResolver::scanDir(const QString &dir, QStringList &result) const
{
    QDir d(dir);
    if (!d.exists()) return;

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
            const QByteArray head = f.read(65536);
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
