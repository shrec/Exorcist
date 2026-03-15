#include "workspacemanagerimpl.h"

#include "mainwindow.h"
#include "editor/editormanager.h"

#include <QDir>
#include <QSettings>
#include <QTabWidget>

WorkspaceManagerImpl::WorkspaceManagerImpl(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

void WorkspaceManagerImpl::openFolder(const QString &path)
{
    // Delegate to MainWindow's existing openFolder (handles dialog if empty)
    QMetaObject::invokeMethod(m_window, "openFolder", Qt::QueuedConnection,
                              Q_ARG(QString, path));
}

void WorkspaceManagerImpl::closeFolder()
{
    // Open empty folder = close current
    openFolder(QString());
}

QString WorkspaceManagerImpl::rootPath() const
{
    return m_window->currentFolder();
}

bool WorkspaceManagerImpl::hasFolder() const
{
    return !m_window->currentFolder().isEmpty();
}

void WorkspaceManagerImpl::openFile(const QString &path)
{
    m_window->openFile(path);
}

void WorkspaceManagerImpl::openFile(const QString &path, int line, int column)
{
    Q_UNUSED(column)
    m_window->openFile(path);
    // TODO: navigate to line/column after open
}

QStringList WorkspaceManagerImpl::openFiles() const
{
    // Will be expanded as EditorManager gets a proper interface
    return {};
}

QString WorkspaceManagerImpl::activeFile() const
{
    return {};
}

QStringList WorkspaceManagerImpl::recentFolders() const
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    return s.value(QStringLiteral("recentFolders")).toStringList();
}

QStringList WorkspaceManagerImpl::recentFiles() const
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    return s.value(QStringLiteral("recentFiles")).toStringList();
}

bool WorkspaceManagerImpl::hasSolution() const
{
    return !solutionPath().isEmpty();
}

QString WorkspaceManagerImpl::solutionPath() const
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    return s.value(QStringLiteral("lastSolution")).toString();
}
