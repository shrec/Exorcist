#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QList>

class WorkspaceIndexer;

struct WorkspaceSymbol {
    enum Kind { Class, Function, Struct, Enum, Namespace, Variable, Method };
    QString name;
    QString filePath;
    int     line = 0;    // 1-based
    Kind    kind = Function;
};

class SymbolIndex : public QObject
{
    Q_OBJECT
public:
    explicit SymbolIndex(QObject *parent = nullptr);

    void indexFile(const QString &filePath, const QString &content);
    void removeFile(const QString &filePath);
    void clear();

    QList<WorkspaceSymbol> search(const QString &query, int maxResults = 50) const;
    QList<WorkspaceSymbol> symbolsInFile(const QString &filePath) const;

signals:
    void indexUpdated();

private:
    QHash<QString, QList<WorkspaceSymbol>> m_fileSymbols;  // filePath → symbols
};
