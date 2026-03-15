#pragma once

#include "core/iworkspacemanager.h"

#include <QObject>

class MainWindow;

/// Concrete implementation of IWorkspaceManager.
/// Bridges the interface to MainWindow's existing folder/file operations.
/// Registered in ServiceRegistry under key "workspaceManager".
class WorkspaceManagerImpl : public QObject, public IWorkspaceManager
{
    Q_OBJECT

public:
    explicit WorkspaceManagerImpl(MainWindow *window, QObject *parent = nullptr);

    // ── IWorkspaceManager ───────────────────────────────────────────

    void openFolder(const QString &path = {}) override;
    void closeFolder() override;
    QString rootPath() const override;
    bool hasFolder() const override;

    void openFile(const QString &path) override;
    void openFile(const QString &path, int line, int column = 0) override;
    QStringList openFiles() const override;
    QString activeFile() const override;

    QStringList recentFolders() const override;
    QStringList recentFiles() const override;

    bool hasSolution() const override;
    QString solutionPath() const override;

private:
    MainWindow *m_window;
};
