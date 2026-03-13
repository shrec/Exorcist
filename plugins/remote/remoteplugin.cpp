#include "remoteplugin.h"
#include "sshconnectionmanager.h"
#include "remotesyncservice.h"
#include "remotefilepanel.h"
#include "sshsession.h"
#include "sshprofile.h"
#include "sdk/ihostservices.h"
#include "sdk/ieditorservice.h"
#include "sdk/iterminalservice.h"

#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

PluginInfo RemotePlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.remote"),
        QStringLiteral("Remote / SSH"),
        QStringLiteral("1.0.0"),
        QStringLiteral("SSH connections, remote file browsing, and sync"),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {}
    };
}

bool RemotePlugin::initialize(IHostServices *host)
{
    m_host = host;
    m_connMgr = new SshConnectionManager(this);
    m_syncService = new RemoteSyncService(this);
    return true;
}

void RemotePlugin::shutdown()
{
    if (m_connMgr)
        m_connMgr->disconnectAll();
}

QWidget *RemotePlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId != QStringLiteral("RemoteDock"))
        return nullptr;

    auto *panel = new RemoteFilePanel(parent);
    panel->setConnectionManager(m_connMgr);

    // Wire openRemoteFile: download to temp then open in editor
    connect(panel, &RemoteFilePanel::openRemoteFile,
            this, [this](const QString &remotePath, const QString &profileId) {
        auto *session = m_connMgr->activeSession(profileId);
        if (!session) return;

        const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString localPath = tmpDir + QStringLiteral("/exorcist_remote_")
            + QFileInfo(remotePath).fileName();

        connect(session, &SshSession::fileContentReady,
                this, [this, localPath](const QString &/*reqId*/, const QByteArray &data) {
            QFile f(localPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(data);
                f.close();
                if (m_host && m_host->editor())
                    m_host->editor()->openFile(localPath);
            }
        }, Qt::SingleShotConnection);

        session->readFile(remotePath);
    });

    // Wire openRemoteTerminal: run ssh command in terminal
    connect(panel, &RemoteFilePanel::openRemoteTerminal,
            this, [this](const QString &profileId) {
        if (!m_host || !m_host->terminal()) return;

        const auto profiles = m_connMgr->profiles();
        for (const auto &p : profiles) {
            if (p.id == profileId) {
                QString cmd = QStringLiteral("ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10");
                if (p.port != 22)
                    cmd += QStringLiteral(" -p %1").arg(p.port);
                if (!p.privateKeyPath.isEmpty())
                    cmd += QStringLiteral(" -i \"%1\"").arg(p.privateKeyPath);
                cmd += QStringLiteral(" %1@%2").arg(
                    p.user.isEmpty() ? QStringLiteral("root") : p.user, p.host);
                m_host->terminal()->runCommand(cmd);
                return;
            }
        }
    });

    return panel;
}
