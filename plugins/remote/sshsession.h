#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QMap>
#include <functional>

#include "sshprofile.h"

/// A session wrapping system SSH/SFTP commands via QProcess.
///
/// Uses the OS-provided `ssh` and `sftp` executables (available by default
/// on Windows 10+, macOS, and Linux). No native SSH library dependency.
///
/// All long-running operations are async — results delivered via signals or
/// callbacks.
class SshSession : public QObject
{
    Q_OBJECT

public:
    explicit SshSession(const SshProfile &profile, QObject *parent = nullptr);
    ~SshSession() override;

    const SshProfile &profile() const { return m_profile; }

    /// Attempt to connect (runs a quick `ssh ... echo ok` test).
    /// Returns true if the test command was started (result via signal).
    bool connectToHost();

    /// True if connection test succeeded and session is usable.
    bool isConnected() const { return m_connected; }

    /// Disconnect and terminate any running processes.
    void disconnectFromHost();

    /// Last error message.
    QString lastError() const { return m_lastError; }

    // ── Remote command execution ──────────────────────────────────────────

    /// Run a command on the remote host. Async — result via commandFinished.
    /// Returns a request ID (>0) or -1 on failure.
    int runCommand(const QString &command, const QString &workDir = {});

    // ── SFTP operations ───────────────────────────────────────────────────

    /// List files in a remote directory. Async — result via directoryListed.
    void listDirectory(const QString &remotePath);

    /// Read a remote file's content. Async — result via fileContentReady.
    void readFile(const QString &remotePath);

    /// Write content to a remote file. Async — result via fileWritten.
    void writeFile(const QString &remotePath, const QByteArray &content);

    /// Download a remote file to a local path. Async — result via fileTransferred.
    void downloadFile(const QString &remotePath, const QString &localPath);

    /// Upload a local file to a remote path. Async — result via fileTransferred.
    void uploadFile(const QString &localPath, const QString &remotePath);

    /// Check if a remote path exists. Async — result via existsResult.
    void exists(const QString &remotePath);

    // ── Port forwarding ───────────────────────────────────────────────────

    /// Start a local port forward: localhost:localPort → remoteHost:remotePort
    /// The SSH process runs in the background. Returns request ID (>0) or -1.
    int startLocalPortForward(int localPort, const QString &remoteHost, int remotePort);

    /// Stop a running port forward by request ID.
    void stopPortForward(int requestId);

signals:
    void connectionEstablished();
    void connectionLost(const QString &error);

    /// Emitted when a runCommand finishes.
    void commandFinished(int requestId, int exitCode,
                         const QString &stdOut, const QString &stdErr);

    /// Emitted when a command produces output in real time.
    void commandOutput(int requestId, const QString &text);

    /// Emitted when listDirectory completes.
    void directoryListed(const QString &remotePath,
                         const QStringList &entries, const QString &error);

    /// Emitted when readFile completes.
    void fileContentReady(const QString &remotePath,
                          const QByteArray &content, const QString &error);

    /// Emitted when writeFile completes.
    void fileWritten(const QString &remotePath, const QString &error);

    /// Emitted when download/upload completes.
    void fileTransferred(const QString &remotePath, const QString &localPath,
                         const QString &error);

    /// Emitted when exists check completes.
    void existsResult(const QString &remotePath, bool exists);

    /// Emitted when a port forward SSH process is running (tunnel ready).
    void portForwardReady(int requestId, int localPort);

    /// Emitted if port forward fails.
    void portForwardError(int requestId, const QString &error);

private:
    QStringList sshBaseArgs() const;
    QStringList scpBaseArgs() const;
    QProcess *startSshProcess(const QStringList &args);
    QProcess *startSftpBatch(const QString &batchCommands);
    void setupPasswordEnv(QProcess *proc);
    void cleanupProcess(QProcess *proc);

    SshProfile m_profile;
    bool       m_connected = false;
    QString    m_lastError;
    int        m_nextRequestId = 1;

    // Track running processes for cleanup
    QList<QProcess *> m_activeProcesses;
    QMap<int, QProcess *> m_portForwards;  // requestId → SSH -L process
};
