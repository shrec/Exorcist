#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QTextStream>

struct MergeConflict {
    QString filePath;
    int startLine = 0;
    int separatorLine = 0;
    int endLine = 0;
    QString oursLabel;
    QString theirsLabel;
    QString oursContent;
    QString theirsContent;

    bool isValid() const { return startLine > 0 && endLine > startLine; }
};

class MergeConflictResolver : public QObject
{
    Q_OBJECT

public:
    explicit MergeConflictResolver(QObject *parent = nullptr);

    QList<MergeConflict> detectConflicts(const QString &filePath) const;
    QList<MergeConflict> detectConflictsInText(const QString &filePath,
                                                const QString &text) const;

    static bool hasConflicts(const QString &text);
    static QString resolveOurs(const QString &text, const MergeConflict &conflict);
    static QString resolveTheirs(const QString &text, const MergeConflict &conflict);
    static QString resolveBoth(const QString &text, const MergeConflict &conflict);
    static QString resolveCustom(const QString &text,
                                  const MergeConflict &conflict,
                                  const QString &resolution);

    QStringList findConflictFiles(const QString &workspaceRoot) const;

signals:
    void conflictsFound(const QString &filePath, int count);
    void conflictResolved(const QString &filePath);

private:
    static QList<MergeConflict> parseConflicts(const QString &filePath,
                                                const QStringList &lines);
    static QString replaceConflict(const QString &text,
                                    const MergeConflict &conflict,
                                    const QString &resolution);
    void scanDir(const QString &dir, QStringList &result) const;
};
