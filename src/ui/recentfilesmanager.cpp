#include "recentfilesmanager.h"

#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QSettings>

RecentFilesManager::RecentFilesManager(QObject *parent)
    : QObject(parent)
{
    load();
}

void RecentFilesManager::addFile(const QString &path)
{
    if (path.isEmpty()) return;

    const QString canonical = QDir::toNativeSeparators(
        QFileInfo(path).absoluteFilePath());

    m_files.removeAll(canonical);
    m_files.prepend(canonical);
    while (m_files.size() > MaxRecentFiles)
        m_files.removeLast();

    save();
    rebuildMenu();
}

QStringList RecentFilesManager::recentFiles() const
{
    return m_files;
}

void RecentFilesManager::clear()
{
    m_files.clear();
    save();
    rebuildMenu();
}

void RecentFilesManager::attachMenu(QMenu *menu)
{
    m_menu = menu;
    rebuildMenu();
}

void RecentFilesManager::save() const
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.setValue(QStringLiteral("recentFiles"), m_files);
}

void RecentFilesManager::load()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    m_files = s.value(QStringLiteral("recentFiles")).toStringList();

    // Remove entries that no longer exist
    m_files.erase(std::remove_if(m_files.begin(), m_files.end(),
        [](const QString &f) { return !QFileInfo::exists(f); }),
        m_files.end());
}

void RecentFilesManager::rebuildMenu()
{
    if (!m_menu) return;

    m_menu->clear();

    if (m_files.isEmpty()) {
        m_menu->addAction(tr("No recent files"))->setEnabled(false);
        return;
    }

    for (const QString &path : m_files) {
        const QString label = QFileInfo(path).fileName()
            + QStringLiteral("  —  ")
            + QDir::toNativeSeparators(QFileInfo(path).absolutePath());
        QAction *action = m_menu->addAction(label);
        connect(action, &QAction::triggered, this, [this, path]() {
            emit fileSelected(path);
        });
    }

    m_menu->addSeparator();
    QAction *clearAction = m_menu->addAction(tr("Clear Recent Files"));
    connect(clearAction, &QAction::triggered, this, &RecentFilesManager::clear);
}
