#include "remotelspmanager.h"
#include "sshsession.h"
#include "../lsp/socketlsptransport.h"

#include <QRandomGenerator>
#include <QTimer>

// POSIX shell quoting — wraps in single quotes, escapes embedded quotes.
static QString shellQuote(const QString &s)
{
    QString quoted = s;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

RemoteLspManager::RemoteLspManager(QObject *parent)
    : QObject(parent)
{
}

void RemoteLspManager::start(SshSession *session, const QString &remoteWorkDir,
                             const QString &clangdBinary, int localPort)
{
    if (m_running) {
        stop();
    }

    m_session = session;
    m_remoteWorkDir = remoteWorkDir;
    m_clangdBinary = clangdBinary;
    m_localPort = (localPort > 0) ? localPort
                                  : (10000 + QRandomGenerator::global()->bounded(50000));
    m_remotePort = m_localPort; // Use same port on remote side for simplicity
    m_running = true;

    m_transport = std::make_unique<SocketLspTransport>(this);

    // Start remote clangd via SSH port forward + socat bridge
    // Remote: socat TCP-LISTEN:${remotePort},fork EXEC:"clangd ..."
    // All user-supplied values are shell-quoted to prevent injection.
    const QString socatCmd = QStringLiteral(
        "cd %1 && socat TCP-LISTEN:%2,reuseaddr,fork "
        "EXEC:\"%3 --log=error --background-index=false\""
    ).arg(shellQuote(m_remoteWorkDir)).arg(m_remotePort).arg(shellQuote(m_clangdBinary));

    m_clangdCmdId = m_session->runCommand(socatCmd);
    if (m_clangdCmdId < 0) {
        m_running = false;
        emit error(tr("Failed to start remote clangd"));
        return;
    }

    // Monitor for clangd crash
    connect(m_session, &SshSession::commandFinished,
            this, [this](int reqId, int exitCode, const QString &, const QString &stdErr) {
        if (reqId == m_clangdCmdId) {
            m_running = false;
            if (exitCode != 0)
                emit error(tr("Remote clangd exited with code %1: %2").arg(exitCode).arg(stdErr));
            emit stopped();
        }
    });

    // Step 2: Set up SSH port forward after a brief delay for socat to bind
    QTimer::singleShot(1500, this, &RemoteLspManager::startPortForward);
}

void RemoteLspManager::startPortForward()
{
    if (!m_running || !m_session) return;

    m_fwdRequestId = m_session->startLocalPortForward(
        m_localPort, QStringLiteral("127.0.0.1"), m_remotePort);

    if (m_fwdRequestId < 0) {
        m_running = false;
        emit error(tr("Failed to start SSH port forward"));
        return;
    }

    connect(m_session, &SshSession::portForwardReady,
            this, [this](int reqId, int port) {
        if (reqId == m_fwdRequestId) {
            Q_UNUSED(port);
            connectSocket();
        }
    });

    connect(m_session, &SshSession::portForwardError,
            this, [this](int reqId, const QString &err) {
        if (reqId == m_fwdRequestId) {
            m_running = false;
            emit error(tr("Port forward failed: %1").arg(err));
        }
    });
}

void RemoteLspManager::connectSocket()
{
    if (!m_running) return;

    m_transport->connectToServer(QStringLiteral("127.0.0.1"), m_localPort);

    // Detect connection success
    connect(m_transport.get(), &SocketLspTransport::transportError,
            this, [this](const QString &err) {
        if (m_running) {
            m_running = false;
            emit error(tr("LSP socket error: %1").arg(err));
        }
    });

    // Socket connected → ready
    // Use a small delay to ensure the connection is fully established
    QTimer::singleShot(500, this, [this]() {
        if (m_running && m_transport->isConnected()) {
            emit ready();
        } else if (m_running) {
            // Retry once
            m_transport->connectToServer(QStringLiteral("127.0.0.1"), m_localPort);
            QTimer::singleShot(1000, this, [this]() {
                if (m_running && m_transport->isConnected()) {
                    emit ready();
                } else if (m_running) {
                    m_running = false;
                    emit error(tr("Could not connect to remote clangd"));
                }
            });
        }
    });
}

void RemoteLspManager::stop()
{
    if (!m_running) return;
    m_running = false;

    // Stop port forward
    if (m_session && m_fwdRequestId > 0) {
        m_session->stopPortForward(m_fwdRequestId);
        m_fwdRequestId = -1;
    }

    // Stop transport
    if (m_transport)
        m_transport->stop();

    // Kill the specific remote clangd socat that we started on our port.
    // Scoped to our exact port to avoid killing other users' processes.
    if (m_session && m_session->isConnected()) {
        m_session->runCommand(
            QStringLiteral("pkill -f 'socat.*TCP-LISTEN:%1.*clangd' 2>/dev/null; true")
                .arg(m_remotePort));
    }

    m_transport.reset();
    emit stopped();
}

LspTransport *RemoteLspManager::transport() const
{
    return m_transport.get();
}
