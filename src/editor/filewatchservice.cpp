#include "filewatchservice.h"

#include <QFileInfo>

FileWatchService::FileWatchService(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_debounce(new QTimer(this))
{
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(300);

    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &FileWatchService::onFileChanged);

    connect(m_debounce, &QTimer::timeout, this, [this] {
        const QStringList batch = m_pending;
        m_pending.clear();
        for (const QString &path : batch) {
            QFileInfo fi(path);
            if (!fi.exists()) {
                m_lastKnown.remove(path);
                emit fileDeletedExternally(path);
            } else {
                const QDateTime mod = fi.lastModified();
                if (m_lastKnown.value(path) != mod) {
                    m_lastKnown[path] = mod;
                    emit fileChangedExternally(path);
                }
                // Re-add to watcher (QFileSystemWatcher removes files after change)
                if (!m_watcher->files().contains(path))
                    m_watcher->addPath(path);
            }
        }
    });
}

void FileWatchService::watchFile(const QString &absPath)
{
    if (absPath.isEmpty()) return;
    QFileInfo fi(absPath);
    if (fi.exists()) {
        m_lastKnown[absPath] = fi.lastModified();
        m_watcher->addPath(absPath);
    }
}

void FileWatchService::unwatchFile(const QString &absPath)
{
    m_watcher->removePath(absPath);
    m_lastKnown.remove(absPath);
}

void FileWatchService::unwatchAll()
{
    const QStringList files = m_watcher->files();
    if (!files.isEmpty())
        m_watcher->removePaths(files);
    m_lastKnown.clear();
}

void FileWatchService::onFileChanged(const QString &path)
{
    if (!m_pending.contains(path))
        m_pending.append(path);
    m_debounce->start();
}
