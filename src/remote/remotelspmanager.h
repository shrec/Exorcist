#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

class LspTransport;
class SshSession;
class SocketLspTransport;

/// Manages a remote LSP server lifecycle:
///   1. SSH connect → start clangd on remote host
///   2. SSH local port forward → tunnel LSP traffic
///   3. SocketLspTransport → connect LspClient to remote clangd
class RemoteLspManager : public QObject
{
    Q_OBJECT
public:
    explicit RemoteLspManager(QObject *parent = nullptr);

    /// Start a remote clangd session.
    /// @param session  An already-connected SshSession.
    /// @param remoteWorkDir  The remote project directory.
    /// @param clangdBinary  Path to clangd on the remote host (default: "clangd").
    /// @param localPort  Local port to bind the tunnel (0 = auto-select).
    void start(SshSession *session, const QString &remoteWorkDir,
               const QString &clangdBinary = QStringLiteral("clangd"),
               int localPort = 0);

    /// Stop the remote clangd and tear down the tunnel.
    void stop();

    /// The transport to pass to LspClient once ready() is emitted.
    LspTransport *transport() const;

    bool isRunning() const { return m_running; }

signals:
    void ready();
    void stopped();
    void error(const QString &message);

private:
    void startPortForward();
    void connectSocket();

    SshSession *m_session = nullptr;
    std::unique_ptr<SocketLspTransport> m_transport;
    QString m_remoteWorkDir;
    QString m_clangdBinary;
    int m_localPort = 0;
    int m_remotePort = 0;    // Port clangd listens on remotely
    int m_fwdRequestId = -1;
    int m_clangdCmdId = -1;
    bool m_running = false;
};
