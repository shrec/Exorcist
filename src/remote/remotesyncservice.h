#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

/// Thin wrapper around rsync/scp for workspace ↔ remote file synchronisation.
///
/// Uses rsync when available (incremental, fast), falls back to scp for
/// individual file transfers.
class RemoteSyncService : public QObject
{
    Q_OBJECT

public:
    explicit RemoteSyncService(QObject *parent = nullptr);

    /// Set remote target information.  Must be called before any sync.
    void setRemote(const QString &host, int port, const QString &user,
                   const QString &privateKeyPath = {});

    /// Push local directory to remote (rsync --delete).
    void pushDirectory(const QString &localDir, const QString &remoteDir);

    /// Pull remote directory to local (rsync).
    void pullDirectory(const QString &remoteDir, const QString &localDir);

    /// Upload a single file via scp.
    void uploadFile(const QString &localPath, const QString &remotePath);

    /// Download a single file via scp.
    void downloadFile(const QString &remotePath, const QString &localPath);

    /// Cancel any running sync operation.
    void cancel();

    bool isSyncing() const { return m_process && m_process->state() != QProcess::NotRunning; }

signals:
    /// Emitted with rsync/scp stdout progress lines.
    void progress(const QString &line);
    /// Emitted when the operation finishes.
    void finished(bool success, const QString &errorMessage);

private:
    void startProcess(const QString &program, const QStringList &args);
    QStringList sshCommandOption() const;

    QString  m_host;
    int      m_port = 22;
    QString  m_user;
    QString  m_keyPath;
    QProcess *m_process = nullptr;
};
