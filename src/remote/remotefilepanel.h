#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QToolButton;
class QTreeView;
class QSplitter;

class SshConnectionManager;
class SshSession;
class RemoteFileSystemModel;
class RemoteHostProber;

/// UI panel for managing SSH profiles and browsing remote file systems.
///
/// Contains a profile selector, connect/disconnect controls, and a
/// tree view for navigating the remote file system. Double-clicking
/// a file downloads and opens it in the editor.
class RemoteFilePanel : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteFilePanel(QWidget *parent = nullptr);

    void setConnectionManager(SshConnectionManager *mgr);

signals:
    /// Emitted when the user double-clicks a remote file to open it.
    void openRemoteFile(const QString &remotePath, const QString &profileId);

    /// Emitted when a remote terminal is requested.
    void openRemoteTerminal(const QString &profileId);

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onAddProfileClicked();
    void onEditProfileClicked();
    void onRemoveProfileClicked();
    void onFileDoubleClicked(const QModelIndex &index);
    void refreshProfileCombo();
    void updateConnectionState();

private:
    void setupUi();

    SshConnectionManager  *m_connMgr = nullptr;
    RemoteFileSystemModel *m_fileModel = nullptr;

    QComboBox    *m_profileCombo = nullptr;
    QPushButton  *m_connectBtn = nullptr;
    QPushButton  *m_disconnectBtn = nullptr;
    QToolButton  *m_addBtn = nullptr;
    QToolButton  *m_editBtn = nullptr;
    QToolButton  *m_removeBtn = nullptr;
    QToolButton  *m_terminalBtn = nullptr;
    QLabel       *m_statusLabel = nullptr;
    QLabel       *m_archLabel = nullptr;
    QTreeView    *m_treeView = nullptr;
    RemoteHostProber *m_prober = nullptr;
};
