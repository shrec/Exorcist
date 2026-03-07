#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QHash>
#include <QDateTime>

class FileWatchService : public QObject
{
    Q_OBJECT
public:
    explicit FileWatchService(QObject *parent = nullptr);

    void watchFile(const QString &absPath);
    void unwatchFile(const QString &absPath);
    void unwatchAll();

signals:
    void fileChangedExternally(const QString &absPath);
    void fileDeletedExternally(const QString &absPath);

private slots:
    void onFileChanged(const QString &path);

private:
    QFileSystemWatcher *m_watcher;
    QTimer             *m_debounce;
    QHash<QString, QDateTime> m_lastKnown;  // last-modified timestamps
    QStringList m_pending;                   // debounce accumulator
};
