#pragma once

#include <QObject>

#include "ifilewatcher.h"

class QFileSystemWatcher;

class QtFileWatcher : public QObject, public IFileWatcher
{
    Q_OBJECT

public:
    explicit QtFileWatcher(QObject *parent = nullptr);

    bool watchPath(const QString &path) override;
    bool unwatchPath(const QString &path) override;
    QStringList watchedFiles() const override;
    QStringList watchedDirectories() const override;

signals:
    void fileChanged(const QString &path);
    void directoryChanged(const QString &path);

private:
    QFileSystemWatcher *m_watcher;
};
