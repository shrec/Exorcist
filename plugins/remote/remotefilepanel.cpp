#include "remotefilepanel.h"
#include "remotefilesystemmodel.h"
#include "remotehostinfo.h"
#include "sshconnectionmanager.h"
#include "sshprofile.h"
#include "sshsession.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QTreeView>
#include <QUuid>
#include <QVBoxLayout>

RemoteFilePanel::RemoteFilePanel(QWidget *parent)
    : QWidget(parent)
    , m_fileModel(new RemoteFileSystemModel(this))
    , m_prober(new RemoteHostProber(this))
{
    setupUi();

    connect(m_prober, &RemoteHostProber::probeFinished,
            this, [this](const RemoteHostInfo &info) {
        const QString label = QStringLiteral("%1 %2")
            .arg(RemoteHostInfo::archLabel(info.arch), info.displayString());
        m_archLabel->setText(label);
        m_archLabel->setVisible(true);

        // Persist detected arch in the profile
        const QString profileId = m_profileCombo->currentData().toString();
        if (m_connMgr && !profileId.isEmpty()) {
            SshProfile prof = m_connMgr->profile(profileId);
            prof.detectedArch = info.archString;
            prof.detectedOs = info.osName;
            prof.detectedDistro = info.distro;
            if (!info.distroVersion.isEmpty())
                prof.detectedDistro += QLatin1Char(' ') + info.distroVersion;
            m_connMgr->updateProfile(prof);
        }
    });
}

void RemoteFilePanel::setConnectionManager(SshConnectionManager *mgr)
{
    m_connMgr = mgr;
    connect(mgr, &SshConnectionManager::profilesChanged,
            this, &RemoteFilePanel::refreshProfileCombo);
    connect(mgr, &SshConnectionManager::connected,
            this, &RemoteFilePanel::updateConnectionState);
    connect(mgr, &SshConnectionManager::disconnected,
            this, &RemoteFilePanel::updateConnectionState);
    connect(mgr, &SshConnectionManager::connectionError,
            this, [this](const QString &, const QString &err) {
        m_statusLabel->setText(tr("Error: %1").arg(err));
        updateConnectionState();
    });
    refreshProfileCombo();
}

void RemoteFilePanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ── Profile selector bar ──────────────────────────────────────────────
    auto *profileBar = new QHBoxLayout;
    profileBar->setSpacing(2);

    m_profileCombo = new QComboBox(this);
    m_profileCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    profileBar->addWidget(m_profileCombo);

    m_addBtn = new QToolButton(this);
    m_addBtn->setText(QStringLiteral("+"));
    m_addBtn->setToolTip(tr("Add SSH profile"));
    connect(m_addBtn, &QToolButton::clicked, this, &RemoteFilePanel::onAddProfileClicked);
    profileBar->addWidget(m_addBtn);

    m_editBtn = new QToolButton(this);
    m_editBtn->setText(QStringLiteral("✎"));
    m_editBtn->setToolTip(tr("Edit profile"));
    connect(m_editBtn, &QToolButton::clicked, this, &RemoteFilePanel::onEditProfileClicked);
    profileBar->addWidget(m_editBtn);

    m_removeBtn = new QToolButton(this);
    m_removeBtn->setText(QStringLiteral("−"));
    m_removeBtn->setToolTip(tr("Remove profile"));
    connect(m_removeBtn, &QToolButton::clicked, this, &RemoteFilePanel::onRemoveProfileClicked);
    profileBar->addWidget(m_removeBtn);

    layout->addLayout(profileBar);

    // ── Connect / disconnect bar ──────────────────────────────────────────
    auto *connBar = new QHBoxLayout;
    connBar->setSpacing(4);

    m_connectBtn = new QPushButton(tr("Connect"), this);
    connect(m_connectBtn, &QPushButton::clicked, this, &RemoteFilePanel::onConnectClicked);
    connBar->addWidget(m_connectBtn);

    m_disconnectBtn = new QPushButton(tr("Disconnect"), this);
    m_disconnectBtn->setEnabled(false);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &RemoteFilePanel::onDisconnectClicked);
    connBar->addWidget(m_disconnectBtn);

    m_terminalBtn = new QToolButton(this);
    m_terminalBtn->setText(QStringLiteral(">_"));
    m_terminalBtn->setToolTip(tr("Open remote terminal"));
    m_terminalBtn->setEnabled(false);
    connect(m_terminalBtn, &QToolButton::clicked, this, [this]() {
        const QString profileId = m_profileCombo->currentData().toString();
        if (!profileId.isEmpty())
            emit openRemoteTerminal(profileId);
    });
    connBar->addWidget(m_terminalBtn);

    layout->addLayout(connBar);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("Not connected"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    layout->addWidget(m_statusLabel);

    m_archLabel = new QLabel(this);
    m_archLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #6ca; font-size: 11px; padding: 2px 0; }"));
    m_archLabel->setVisible(false);
    m_archLabel->setWordWrap(true);
    layout->addWidget(m_archLabel);

    // ── File tree ─────────────────────────────────────────────────────────
    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_fileModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setAnimated(true);
    m_treeView->setIndentation(16);
    connect(m_treeView, &QTreeView::doubleClicked,
            this, &RemoteFilePanel::onFileDoubleClicked);
    layout->addWidget(m_treeView, 1);

    connect(m_fileModel, &RemoteFileSystemModel::loadError,
            this, [this](const QString &, const QString &err) {
        m_statusLabel->setText(tr("Error: %1").arg(err));
    });
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void RemoteFilePanel::onConnectClicked()
{
    if (!m_connMgr)
        return;

    const QString profileId = m_profileCombo->currentData().toString();
    if (profileId.isEmpty())
        return;

    m_statusLabel->setText(tr("Connecting..."));
    m_connectBtn->setEnabled(false);

    auto *session = m_connMgr->connect(profileId);
    if (!session) {
        m_statusLabel->setText(tr("Connection failed"));
        m_connectBtn->setEnabled(true);
        return;
    }

    // When connected, set up the file model
    connect(session, &SshSession::connectionEstablished,
            this, [this, session, profileId]() {
        m_fileModel->setSession(session);
        const auto prof = m_connMgr->profile(profileId);
        const QString root = prof.remoteWorkDir.isEmpty()
            ? QStringLiteral("~") : prof.remoteWorkDir;
        m_fileModel->setRootPath(root);
        m_statusLabel->setText(tr("Connected to %1").arg(prof.name));
        updateConnectionState();

        // Auto-probe remote host architecture & toolchains
        m_prober->probe(session);
    }, Qt::SingleShotConnection);
}

void RemoteFilePanel::onDisconnectClicked()
{
    if (!m_connMgr)
        return;

    const QString profileId = m_profileCombo->currentData().toString();
    m_connMgr->disconnect(profileId);
    m_fileModel->setRootPath({});
    m_statusLabel->setText(tr("Disconnected"));
    m_archLabel->setVisible(false);
    updateConnectionState();
}

void RemoteFilePanel::onAddProfileClicked()
{
    if (!m_connMgr)
        return;

    // Simple multi-step input for MVP
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New SSH Profile"),
                                                tr("Profile name:"), QLineEdit::Normal,
                                                QStringLiteral("My Server"), &ok);
    if (!ok || name.isEmpty())
        return;

    const QString host = QInputDialog::getText(this, tr("New SSH Profile"),
                                                tr("Host (hostname or IP):"), QLineEdit::Normal,
                                                {}, &ok);
    if (!ok || host.isEmpty())
        return;

    const QString user = QInputDialog::getText(this, tr("New SSH Profile"),
                                                tr("Username:"), QLineEdit::Normal,
                                                QStringLiteral("root"), &ok);
    if (!ok || user.isEmpty())
        return;

    // Auth method selection
    const QStringList authMethods = {tr("SSH Key"), tr("Password"), tr("SSH Agent")};
    const QString authChoice = QInputDialog::getItem(this, tr("New SSH Profile"),
                                                      tr("Authentication method:"),
                                                      authMethods, 0, false, &ok);
    if (!ok)
        return;

    SshProfile profile;
    profile.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    profile.name = name;
    profile.host = host;
    profile.user = user;

    if (authChoice == authMethods[0]) {
        // SSH Key
        profile.authMethod = QStringLiteral("key");
        profile.privateKeyPath = QInputDialog::getText(this, tr("New SSH Profile"),
                                                        tr("Private key path:"),
                                                        QLineEdit::Normal,
                                                        QStringLiteral("~/.ssh/id_rsa"), &ok);
        if (!ok) return;
    } else if (authChoice == authMethods[1]) {
        // Password
        profile.authMethod = QStringLiteral("password");
        profile.password = QInputDialog::getText(this, tr("New SSH Profile"),
                                                  tr("Password:"),
                                                  QLineEdit::Password,
                                                  {}, &ok);
        if (!ok || profile.password.isEmpty()) return;
    } else {
        profile.authMethod = QStringLiteral("agent");
    }

    const QString remoteDir = QInputDialog::getText(this, tr("New SSH Profile"),
                                                     tr("Remote working directory:"),
                                                     QLineEdit::Normal,
                                                     QStringLiteral("~"), &ok);
    if (!ok)
        return;

    profile.remoteWorkDir = remoteDir;

    m_connMgr->addProfile(profile);
}

void RemoteFilePanel::onEditProfileClicked()
{
    if (!m_connMgr)
        return;

    const QString profileId = m_profileCombo->currentData().toString();
    if (profileId.isEmpty())
        return;

    SshProfile prof = m_connMgr->profile(profileId);
    bool ok = false;

    prof.name = QInputDialog::getText(this, tr("Edit SSH Profile"),
                                       tr("Profile name:"), QLineEdit::Normal,
                                       prof.name, &ok);
    if (!ok) return;

    prof.host = QInputDialog::getText(this, tr("Edit SSH Profile"),
                                       tr("Host:"), QLineEdit::Normal,
                                       prof.host, &ok);
    if (!ok) return;

    prof.user = QInputDialog::getText(this, tr("Edit SSH Profile"),
                                       tr("Username:"), QLineEdit::Normal,
                                       prof.user, &ok);
    if (!ok) return;

    // Auth method selection
    const QStringList authMethods = {tr("SSH Key"), tr("Password"), tr("SSH Agent")};
    int currentAuth = 2; // default: agent
    if (prof.authMethod == QLatin1String("key"))      currentAuth = 0;
    else if (prof.authMethod == QLatin1String("password")) currentAuth = 1;

    const QString authChoice = QInputDialog::getItem(this, tr("Edit SSH Profile"),
                                                      tr("Authentication method:"),
                                                      authMethods, currentAuth, false, &ok);
    if (!ok) return;

    if (authChoice == authMethods[0]) {
        prof.authMethod = QStringLiteral("key");
        prof.privateKeyPath = QInputDialog::getText(this, tr("Edit SSH Profile"),
                                                     tr("Private key path:"), QLineEdit::Normal,
                                                     prof.privateKeyPath, &ok);
        if (!ok) return;
        prof.password.clear();
    } else if (authChoice == authMethods[1]) {
        prof.authMethod = QStringLiteral("password");
        prof.password = QInputDialog::getText(this, tr("Edit SSH Profile"),
                                               tr("Password:"),
                                               QLineEdit::Password,
                                               {}, &ok);
        if (!ok || prof.password.isEmpty()) return;
        prof.privateKeyPath.clear();
    } else {
        prof.authMethod = QStringLiteral("agent");
        prof.privateKeyPath.clear();
        prof.password.clear();
    }

    prof.remoteWorkDir = QInputDialog::getText(this, tr("Edit SSH Profile"),
                                                tr("Remote working directory:"),
                                                QLineEdit::Normal,
                                                prof.remoteWorkDir, &ok);
    if (!ok) return;

    m_connMgr->updateProfile(prof);
}

void RemoteFilePanel::onRemoveProfileClicked()
{
    if (!m_connMgr)
        return;

    const QString profileId = m_profileCombo->currentData().toString();
    if (profileId.isEmpty())
        return;

    const auto prof = m_connMgr->profile(profileId);
    const auto result = QMessageBox::question(
        this, tr("Remove Profile"),
        tr("Remove SSH profile \"%1\"?").arg(prof.name));

    if (result != QMessageBox::Yes)
        return;

    m_connMgr->removeProfile(profileId);
}

void RemoteFilePanel::onFileDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid() || m_fileModel->isDir(index))
        return;

    const QString remotePath = m_fileModel->filePath(index);
    const QString profileId = m_profileCombo->currentData().toString();
    emit openRemoteFile(remotePath, profileId);
}

void RemoteFilePanel::refreshProfileCombo()
{
    m_profileCombo->clear();
    if (!m_connMgr)
        return;

    for (const auto &p : m_connMgr->profiles()) {
        QString label = QStringLiteral("%1 (%2@%3)").arg(p.name, p.user, p.host);
        if (!p.detectedArch.isEmpty())
            label += QStringLiteral(" [%1]").arg(p.detectedArch);
        m_profileCombo->addItem(label, p.id);
    }

    updateConnectionState();
}

void RemoteFilePanel::updateConnectionState()
{
    const QString profileId = m_profileCombo->currentData().toString();
    const bool connected = m_connMgr && m_connMgr->isConnected(profileId);

    m_connectBtn->setEnabled(!connected && !profileId.isEmpty());
    m_disconnectBtn->setEnabled(connected);
    m_terminalBtn->setEnabled(connected);
    m_editBtn->setEnabled(!profileId.isEmpty());
    m_removeBtn->setEnabled(!profileId.isEmpty());
}
