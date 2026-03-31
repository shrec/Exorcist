#include "remoteplugin.h"

#include "sshconnectionmanager.h"
#include "remotesyncservice.h"
#include "remotefilepanel.h"
#include "sshsession.h"
#include "sshprofile.h"

#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "sdk/icommandservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/iterminalservice.h"

#include <QFile>
#include <QFileInfo>
#include <QKeySequence>
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
        {PluginPermission::TerminalExecute, PluginPermission::NetworkAccess, PluginPermission::FilesystemRead}
    };
}

bool RemotePlugin::initializePlugin()
{
    m_connMgr = new SshConnectionManager(this);
    m_syncService = new RemoteSyncService(this);
    m_panel = new RemoteFilePanel(nullptr);
    m_panel->setConnectionManager(m_connMgr);

    registerService(QStringLiteral("remoteConnectionManager"), m_connMgr);
    registerService(QStringLiteral("remoteSyncService"), m_syncService);

    wirePanelSignals();
    registerCommands();
    installMenusAndToolBar();

    return true;
}

void RemotePlugin::shutdownPlugin()
{
    if (m_connMgr)
        m_connMgr->disconnectAll();
}

QWidget *RemotePlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId != QStringLiteral("RemoteDock"))
        return nullptr;

    if (!m_panel)
        return nullptr;

    m_panel->setParent(parent);
    return m_panel;
}

void RemotePlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("remote.showExplorer"),
                          tr("Show Remote Explorer"),
                          [this]() {
        showPanel(QStringLiteral("RemoteDock"));
    });

    cmds->registerCommand(QStringLiteral("remote.connect"),
                          tr("Connect to Remote Host"),
                          [this]() {
        showPanel(QStringLiteral("RemoteDock"));
        showStatusMessage(tr("Open the Remote panel to connect to an SSH host."), 3000);
    });
}

void RemotePlugin::installMenusAndToolBar()
{
    ensureMenu(QStringLiteral("remote"), tr("&Remote"));

    createToolBar(QStringLiteral("remote"), tr("Remote"));
    addToolBarCommand(QStringLiteral("remote"), tr("Remote Explorer"),
                      QStringLiteral("remote.showExplorer"), this);
    addToolBarCommand(QStringLiteral("remote"), tr("Connect"),
                      QStringLiteral("remote.connect"), this);

    addMenuCommand(QStringLiteral("remote"), tr("&Remote Explorer"),
                   QStringLiteral("remote.showExplorer"), this);
    addMenuCommand(QStringLiteral("remote"), tr("&Connect to Remote Host"),
                   QStringLiteral("remote.connect"), this,
                   QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_R));
}

void RemotePlugin::wirePanelSignals()
{
    if (!m_panel)
        return;

    connect(m_panel, &RemoteFilePanel::openRemoteFile,
            this, [this](const QString &remotePath, const QString &profileId) {
        auto *session = m_connMgr ? m_connMgr->activeSession(profileId) : nullptr;
        if (!session || !editor())
            return;

        const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString localPath = tmpDir + QStringLiteral("/exorcist_remote_")
            + QFileInfo(remotePath).fileName();

        connect(session, &SshSession::fileContentReady,
                this, [this, localPath](const QString &, const QByteArray &data) {
            QFile f(localPath);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                return;

            f.write(data);
            f.close();
            if (auto *ed = editor())
                ed->openFile(localPath);
        }, Qt::SingleShotConnection);

        session->readFile(remotePath);
    });

    connect(m_panel, &RemoteFilePanel::openRemoteTerminal,
            this, [this](const QString &profileId) {
        if (!m_connMgr || !terminal())
            return;

        const auto profiles = m_connMgr->profiles();
        for (const auto &p : profiles) {
            if (p.id != profileId)
                continue;

            QString cmd = QStringLiteral("ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10");
            if (p.port != 22)
                cmd += QStringLiteral(" -p %1").arg(p.port);
            if (!p.privateKeyPath.isEmpty())
                cmd += QStringLiteral(" -i \"%1\"").arg(p.privateKeyPath);
            cmd += QStringLiteral(" %1@%2").arg(
                p.user.isEmpty() ? QStringLiteral("root") : p.user, p.host);
            terminal()->runCommand(cmd);
            return;
        }
    });
}
