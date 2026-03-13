#include "qtfilewatcher.h"

#include <QFileSystemWatcher>

QtFileWatcher::QtFileWatcher(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
{
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &QtFileWatcher::fileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &QtFileWatcher::directoryChanged);
}

bool QtFileWatcher::watchPath(const QString &path)
{
    if (path.isEmpty())
        return false;
    return m_watcher->addPath(path);
}

bool QtFileWatcher::unwatchPath(const QString &path)
{
    if (path.isEmpty())
        return false;
    return m_watcher->removePath(path);
}

QStringList QtFileWatcher::watchedFiles() const
{
    return m_watcher->files();
}

QStringList QtFileWatcher::watchedDirectories() const
{
    return m_watcher->directories();
}
