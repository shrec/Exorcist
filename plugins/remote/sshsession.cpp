#include "sshsession.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryFile>
#include <QTimer>

// ── Shell quoting for remote commands ────────────────────────────────────────
// Wraps a string in single quotes for POSIX shells, escaping embedded
// single quotes. Safe for paths with spaces, semicolons, and metacharacters.

static QString shellQuote(const QString &s)
{
    QString quoted = s;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

SshSession::SshSession(const SshProfile &profile, QObject *parent)
    : QObject(parent)
    , m_profile(profile)
{
}

SshSession::~SshSession()
{
    disconnectFromHost();
}

// ── Connection ────────────────────────────────────────────────────────────────

QStringList SshSession::sshBaseArgs() const
{
    QStringList args;

    if (m_profile.authMethod == QLatin1String("password")) {
        // Password auth: disable batch mode, disable pubkey to force password prompt
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=no")
             << QStringLiteral("-o") << QStringLiteral("PubkeyAuthentication=no")
             << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=password,keyboard-interactive");
    } else {
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
    }

    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=yes")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10")
         << QStringLiteral("-o")
         << QStringLiteral("UserKnownHostsFile=%1").arg(managedKnownHostsPath());

    if (m_profile.port != 22) {
        args << QStringLiteral("-p") << QString::number(m_profile.port);
    }

    if (m_profile.authMethod == QLatin1String("key") && !m_profile.privateKeyPath.isEmpty()) {
        args << QStringLiteral("-i") << m_profile.privateKeyPath;
    }

    return args;
}

QProcess *SshSession::startSshProcess(const QStringList &args)
{
    auto *proc = new QProcess(this);
    m_activeProcesses.append(proc);

    QStringList fullArgs = sshBaseArgs();
    fullArgs << QStringLiteral("%1@%2").arg(m_profile.user, m_profile.host);
    fullArgs.append(args);

    proc->setProgram(QStringLiteral("ssh"));
    proc->setArguments(fullArgs);

    if (m_profile.authMethod == QLatin1String("password")
        && !m_profile.password.isEmpty()) {
        setupPasswordEnv(proc);
    }

    proc->start();
    return proc;
}

QProcess *SshSession::startSftpBatch(const QString &batchCommands)
{
    // Write batch commands to a temp file
    auto *tmpFile = new QTemporaryFile(this);
    if (!tmpFile->open()) {
        m_lastError = tr("Failed to create temp file for SFTP batch");
        return nullptr;
    }
    tmpFile->write(batchCommands.toUtf8());
    tmpFile->flush();

    auto *proc = new QProcess(this);
    m_activeProcesses.append(proc);

    QStringList args;

    if (m_profile.authMethod == QLatin1String("password")) {
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=no")
             << QStringLiteral("-o") << QStringLiteral("PubkeyAuthentication=no")
             << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=password,keyboard-interactive");
    } else {
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
    }

    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=yes")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10")
         << QStringLiteral("-o")
         << QStringLiteral("UserKnownHostsFile=%1").arg(managedKnownHostsPath());

    if (m_profile.port != 22) {
        args << QStringLiteral("-P") << QString::number(m_profile.port); // SFTP uses -P (uppercase)
    }

    if (m_profile.authMethod == QLatin1String("key") && !m_profile.privateKeyPath.isEmpty()) {
        args << QStringLiteral("-i") << m_profile.privateKeyPath;
    }

    args << QStringLiteral("-b") << tmpFile->fileName();
    args << QStringLiteral("%1@%2").arg(m_profile.user, m_profile.host);

    proc->setProgram(QStringLiteral("sftp"));
    proc->setArguments(args);

    if (m_profile.authMethod == QLatin1String("password")
        && !m_profile.password.isEmpty()) {
        setupPasswordEnv(proc);
    }

    proc->start();

    // Clean up temp file when process finishes
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            tmpFile, &QObject::deleteLater);

    return proc;
}

void SshSession::cleanupProcess(QProcess *proc)
{
    m_activeProcesses.removeOne(proc);
    proc->deleteLater();
}

QStringList SshSession::scpBaseArgs() const
{
    QStringList args;

    if (m_profile.authMethod == QLatin1String("password")) {
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=no")
             << QStringLiteral("-o") << QStringLiteral("PubkeyAuthentication=no")
             << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=password,keyboard-interactive");
    } else {
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
    }

    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=yes")
         << QStringLiteral("-o")
         << QStringLiteral("UserKnownHostsFile=%1").arg(managedKnownHostsPath());

    if (m_profile.port != 22) {
        args << QStringLiteral("-P") << QString::number(m_profile.port); // SCP uses -P (uppercase)
    }

    if (m_profile.authMethod == QLatin1String("key") && !m_profile.privateKeyPath.isEmpty()) {
        args << QStringLiteral("-i") << m_profile.privateKeyPath;
    }

    return args;
}

void SshSession::setupPasswordEnv(QProcess *proc)
{
    // Use SSH_ASKPASS to feed the password to SSH without using stdin.
    // Each invocation gets a unique temp file (auto-removed on session
    // destruction) to prevent reuse/race conditions between sessions.

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("EXORCIST_SSH_PASS"), m_profile.password);
    env.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));

#ifdef Q_OS_WIN
    auto tmpFile = std::make_unique<QTemporaryFile>(
        QDir::tempPath() + QStringLiteral("/exo_askpass_XXXXXX.cmd"));
    if (tmpFile->open()) {
        tmpFile->write("@echo off\r\necho %EXORCIST_SSH_PASS%\r\n");
        tmpFile->flush();
        tmpFile->setAutoRemove(true);
        env.insert(QStringLiteral("SSH_ASKPASS"), tmpFile->fileName());
    }
#else
    auto tmpFile = std::make_unique<QTemporaryFile>(
        QDir::tempPath() + QStringLiteral("/exo_askpass_XXXXXX.sh"));
    if (tmpFile->open()) {
        tmpFile->write("#!/bin/sh\necho \"$EXORCIST_SSH_PASS\"\n");
        tmpFile->flush();
        tmpFile->setAutoRemove(true);
        tmpFile->setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        env.insert(QStringLiteral("SSH_ASKPASS"), tmpFile->fileName());
    }
    env.insert(QStringLiteral("DISPLAY"), QStringLiteral(""));
#endif

    // Keep temp file alive until session is destroyed
    m_askpassFiles.push_back(std::move(tmpFile));
    proc->setProcessEnvironment(env);
}

bool SshSession::connectToHost()
{
    m_connected = false;
    m_lastError.clear();

    // Test connectivity: ssh user@host echo __EXORCIST_OK__
    auto *proc = startSshProcess({QStringLiteral("echo __EXORCIST_OK__")});
    if (!proc || !proc->waitForStarted(5000)) {
        m_lastError = tr("Failed to start ssh process");
        if (proc) cleanupProcess(proc);
        return false;
    }

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        cleanupProcess(proc);
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        const QString err = QString::fromUtf8(proc->readAllStandardError());
        if (exitCode == 0 && out.contains(QLatin1String("__EXORCIST_OK__"))) {
            m_connected = true;
            emit connectionEstablished();
        } else if (err.contains(QLatin1String("Host key verification failed"))) {
            // Host not in our managed known_hosts — ask the user.
            // Try to extract the fingerprint from stderr for display.
            m_hostKeyPending = true;
            QString fingerprint;
            const auto lines = err.split(QLatin1Char('\n'));
            for (const auto &line : lines) {
                if (line.contains(QLatin1String("SHA256:"))
                    || line.contains(QLatin1String("MD5:"))) {
                    fingerprint = line.trimmed();
                    break;
                }
            }
            emit hostKeyVerificationRequired(m_profile.host, fingerprint);
        } else {
            m_connected = false;
            m_lastError = err.isEmpty() ? tr("Connection failed (exit code %1)").arg(exitCode) : err.trimmed();
            emit connectionLost(m_lastError);
        }
    });

    return true;
}

void SshSession::disconnectFromHost()
{
    m_connected = false;
    for (auto *proc : m_activeProcesses) {
        proc->kill();
        proc->waitForFinished(2000);
        proc->deleteLater();
    }
    m_activeProcesses.clear();
}

// ── Remote command execution ──────────────────────────────────────────────────

int SshSession::runCommand(const QString &command, const QString &workDir)
{
    if (!m_connected)
        return -1;

    const int reqId = m_nextRequestId++;

    QString remoteCmd = command;
    if (!workDir.isEmpty())
        remoteCmd = QStringLiteral("cd %1 && %2").arg(shellQuote(workDir), command);

    auto *proc = startSshProcess({remoteCmd});
    if (!proc)
        return -1;

    // Stream output in real time
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc, reqId]() {
        const QString text = QString::fromUtf8(proc->readAllStandardOutput());
        emit commandOutput(reqId, text);
    });

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, reqId](int exitCode, QProcess::ExitStatus) {
        const QString stdOut = QString::fromUtf8(proc->readAllStandardOutput());
        const QString stdErr = QString::fromUtf8(proc->readAllStandardError());
        cleanupProcess(proc);
        emit commandFinished(reqId, exitCode, stdOut, stdErr);
    });

    return reqId;
}

// ── SFTP operations ───────────────────────────────────────────────────────────

void SshSession::listDirectory(const QString &remotePath)
{
    if (!m_connected) {
        emit directoryListed(remotePath, {}, tr("Not connected"));
        return;
    }

    // Use ssh + ls instead of sftp for simpler parsing
    const QString cmd = QStringLiteral("ls -1aF %1").arg(shellQuote(remotePath));
    auto *proc = startSshProcess({cmd});
    if (!proc) {
        emit directoryListed(remotePath, {}, tr("Failed to start ssh"));
        return;
    }

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, remotePath](int exitCode, QProcess::ExitStatus) {
        cleanupProcess(proc);
        if (exitCode != 0) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            emit directoryListed(remotePath, {}, err);
            return;
        }
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        QStringList entries;
        const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const auto &line : lines) {
            const QString trimmed = line.trimmed();
            if (trimmed == QLatin1String(".") || trimmed == QLatin1String("./")
                || trimmed == QLatin1String("..") || trimmed == QLatin1String("../"))
                continue;
            entries.append(trimmed);
        }
        emit directoryListed(remotePath, entries, {});
    });
}

void SshSession::readFile(const QString &remotePath)
{
    if (!m_connected) {
        emit fileContentReady(remotePath, {}, tr("Not connected"));
        return;
    }

    const QString cmd = QStringLiteral("cat %1").arg(shellQuote(remotePath));
    auto *proc = startSshProcess({cmd});
    if (!proc) {
        emit fileContentReady(remotePath, {}, tr("Failed to start ssh"));
        return;
    }

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, remotePath](int exitCode, QProcess::ExitStatus) {
        cleanupProcess(proc);
        if (exitCode != 0) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            emit fileContentReady(remotePath, {}, err);
            return;
        }
        emit fileContentReady(remotePath, proc->readAllStandardOutput(), {});
    });
}

void SshSession::writeFile(const QString &remotePath, const QByteArray &content)
{
    if (!m_connected) {
        emit fileWritten(remotePath, tr("Not connected"));
        return;
    }

    // Pipe content via stdin to ssh cat > file
    auto *proc = new QProcess(this);
    m_activeProcesses.append(proc);

    QStringList args = sshBaseArgs();
    args << QStringLiteral("%1@%2").arg(m_profile.user, m_profile.host);
    args << QStringLiteral("cat > %1").arg(shellQuote(remotePath));

    proc->setProgram(QStringLiteral("ssh"));
    proc->setArguments(args);

    // SSH_ASKPASS handles password auth without using stdin, so stdin
    // remains free for piping file content.
    if (m_profile.authMethod == QLatin1String("password")
        && !m_profile.password.isEmpty()) {
        setupPasswordEnv(proc);
    }

    proc->start();

    if (!proc->waitForStarted(5000)) {
        cleanupProcess(proc);
        emit fileWritten(remotePath, tr("Failed to start ssh"));
        return;
    }

    proc->write(content);
    proc->closeWriteChannel();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, remotePath](int exitCode, QProcess::ExitStatus) {
        cleanupProcess(proc);
        if (exitCode != 0) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            emit fileWritten(remotePath, err);
        } else {
            emit fileWritten(remotePath, {});
        }
    });
}

void SshSession::downloadFile(const QString &remotePath, const QString &localPath)
{
    if (!m_connected) {
        emit fileTransferred(remotePath, localPath, tr("Not connected"));
        return;
    }

    auto *proc = new QProcess(this);
    m_activeProcesses.append(proc);

    QStringList args = scpBaseArgs();
    args << QStringLiteral("%1@%2:%3").arg(m_profile.user, m_profile.host, remotePath);
    args << localPath;

    proc->setProgram(QStringLiteral("scp"));
    proc->setArguments(args);

    if (m_profile.authMethod == QLatin1String("password")
        && !m_profile.password.isEmpty()) {
        setupPasswordEnv(proc);
    }

    proc->start();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, remotePath, localPath](int exitCode, QProcess::ExitStatus) {
        cleanupProcess(proc);
        if (exitCode != 0) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            emit fileTransferred(remotePath, localPath, err);
        } else {
            emit fileTransferred(remotePath, localPath, {});
        }
    });
}

void SshSession::uploadFile(const QString &localPath, const QString &remotePath)
{
    if (!m_connected) {
        emit fileTransferred(remotePath, localPath, tr("Not connected"));
        return;
    }

    auto *proc = new QProcess(this);
    m_activeProcesses.append(proc);

    QStringList args = scpBaseArgs();
    args << localPath;
    args << QStringLiteral("%1@%2:%3").arg(m_profile.user, m_profile.host, remotePath);

    proc->setProgram(QStringLiteral("scp"));
    proc->setArguments(args);

    if (m_profile.authMethod == QLatin1String("password")
        && !m_profile.password.isEmpty()) {
        setupPasswordEnv(proc);
    }

    proc->start();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, remotePath, localPath](int exitCode, QProcess::ExitStatus) {
        cleanupProcess(proc);
        if (exitCode != 0) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            emit fileTransferred(remotePath, localPath, err);
        } else {
            emit fileTransferred(remotePath, localPath, {});
        }
    });
}

void SshSession::exists(const QString &remotePath)
{
    if (!m_connected) {
        emit existsResult(remotePath, false);
        return;
    }

    const QString cmd = QStringLiteral("test -e %1 && echo EXISTS || echo NOTFOUND").arg(shellQuote(remotePath));
    auto *proc = startSshProcess({cmd});
    if (!proc) {
        emit existsResult(remotePath, false);
        return;
    }

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, remotePath](int, QProcess::ExitStatus) {
        cleanupProcess(proc);
        const QString out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        emit existsResult(remotePath, out.contains(QLatin1String("EXISTS")));
    });
}

// ── Port forwarding ──────────────────────────────────────────────────────────

int SshSession::startLocalPortForward(int localPort, const QString &remoteHost, int remotePort)
{
    if (!m_connected) {
        return -1;
    }

    const int requestId = m_nextRequestId++;
    const QString fwdSpec = QStringLiteral("%1:%2:%3")
                                .arg(localPort)
                                .arg(remoteHost)
                                .arg(remotePort);

    QStringList args = sshBaseArgs();
    args << QStringLiteral("-N")   // No remote command
         << QStringLiteral("-L") << fwdSpec;

    auto *proc = startSshProcess(args);
    if (!proc) {
        emit portForwardError(requestId, QStringLiteral("Failed to start SSH port forward"));
        return -1;
    }

    m_portForwards.insert(requestId, proc);

    // Detect when tunnel is ready (SSH connects and stays open)
    // We use a short timer: if the process is still running after 2s,
    // the tunnel is established.
    auto *readyTimer = new QTimer(proc);
    readyTimer->setSingleShot(true);
    connect(readyTimer, &QTimer::timeout, this, [this, requestId, localPort]() {
        emit portForwardReady(requestId, localPort);
    });
    readyTimer->start(2000);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, requestId, readyTimer](int exitCode, QProcess::ExitStatus) {
        readyTimer->stop();
        m_portForwards.remove(requestId);
        cleanupProcess(proc);
        if (exitCode != 0) {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            emit portForwardError(requestId, err.isEmpty() ? QStringLiteral("SSH tunnel exited") : err);
        }
    });

    return requestId;
}

void SshSession::stopPortForward(int requestId)
{
    auto *proc = m_portForwards.value(requestId);
    if (!proc) return;

    m_portForwards.remove(requestId);
    proc->terminate();
    if (!proc->waitForFinished(2000)) {
        proc->kill();
        proc->waitForFinished(1000);
    }
    cleanupProcess(proc);
}

// ── Managed known_hosts & host key verification ──────────────────────────────

QString SshSession::managedKnownHostsPath() const
{
    // Use a per-app known_hosts file so the IDE manages keys independently
    // of the user's ~/.ssh/known_hosts.
    const QString configDir = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/.exorcist");
    QDir().mkpath(configDir);
    return configDir + QStringLiteral("/known_hosts");
}

void SshSession::acceptHostKey()
{
    if (!m_hostKeyPending)
        return;
    m_hostKeyAccepted = true;
    m_hostKeyPending = false;

    // Re-connect with StrictHostKeyChecking=accept-new for this ONE attempt
    // so ssh adds the key to our managed known_hosts file.
    auto *proc = new QProcess(this);
    m_activeProcesses.append(proc);

    QStringList args;
    if (m_profile.authMethod == QLatin1String("password")) {
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=no")
             << QStringLiteral("-o") << QStringLiteral("PubkeyAuthentication=no")
             << QStringLiteral("-o") << QStringLiteral("PreferredAuthentications=password,keyboard-interactive");
    } else {
        args << QStringLiteral("-o") << QStringLiteral("BatchMode=yes");
    }
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10")
         << QStringLiteral("-o")
         << QStringLiteral("UserKnownHostsFile=%1").arg(managedKnownHostsPath());

    if (m_profile.port != 22)
        args << QStringLiteral("-p") << QString::number(m_profile.port);
    if (m_profile.authMethod == QLatin1String("key") && !m_profile.privateKeyPath.isEmpty())
        args << QStringLiteral("-i") << m_profile.privateKeyPath;

    args << QStringLiteral("%1@%2").arg(m_profile.user, m_profile.host)
         << QStringLiteral("echo __EXORCIST_OK__");

    proc->setProgram(QStringLiteral("ssh"));
    proc->setArguments(args);

    if (m_profile.authMethod == QLatin1String("password")
        && !m_profile.password.isEmpty()) {
        setupPasswordEnv(proc);
    }

    proc->start();

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        cleanupProcess(proc);
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        if (exitCode == 0 && out.contains(QLatin1String("__EXORCIST_OK__"))) {
            m_connected = true;
            emit hostKeyAccepted(m_profile.host);
            emit connectionEstablished();
        } else {
            const QString err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
            m_lastError = err.isEmpty()
                ? tr("Connection failed after accepting host key") : err;
            emit connectionLost(m_lastError);
        }
    });
}

void SshSession::rejectHostKey()
{
    m_hostKeyPending = false;
    m_hostKeyAccepted = false;
    m_lastError = tr("Host key rejected by user");
    emit connectionLost(m_lastError);
}
