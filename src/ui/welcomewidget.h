#pragma once

#include <QWidget>

class QEvent;
class QListWidget;
class QLabel;

/// Welcome page shown when no project is open.
/// VS Code–style layout: Start links on the left, info on the right.
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

    /// User clicked "New File...".
    void newFileRequested();

    /// User clicked "Open File...".
    void openFileRequested();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QListWidget *m_recentList = nullptr;
    QLabel *m_recentLabel = nullptr;
};
