#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

class SearchIndex
{
public:
    void build(const QString &rootPath);
    QStringList candidateFiles(const QString &pattern, bool caseSensitive) const;
    QStringList allFiles() const;
    bool isReady() const;
    QString rootPath() const;

private:
    static QStringList extractTrigrams(const QString &text);

    QString m_rootPath;
    QHash<QString, QSet<QString>> m_trigramToFiles;
    QStringList m_allFiles;
};
