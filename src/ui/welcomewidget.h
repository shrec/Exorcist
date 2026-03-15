#pragma once

#include <QWidget>

class QListWidget;

/// Welcome page shown when no project is open.
/// Provides quick access to recent folders, Open Folder, and New Project.
class WelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget *parent = nullptr);

    /// Refresh the recent folders list from QSettings.
    void refreshRecent();

signals:
    /// User wants to open a specific folder from the recent list.
    void openFolderRequested(const QString &path);

    /// User clicked "Open Folder..." (no specific path).
    void openFolderBrowseRequested();

    /// User clicked "New Project...".
    void newProjectRequested();

private:
    QListWidget *m_recentList = nullptr;
};
