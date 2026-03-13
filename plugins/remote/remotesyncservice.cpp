#include "remotesyncservice.h"

#include <QFileInfo>

RemoteSyncService::RemoteSyncService(QObject *parent)
    : QObject(parent)
{
}

void RemoteSyncService::setRemote(const QString &host, int port,
                                  const QString &user,
                                  const QString &privateKeyPath)
{
    m_host    = host;
    m_port    = port;
    m_user    = user;
    m_keyPath = privateKeyPath;
}

QStringList RemoteSyncService::sshCommandOption() const
{
    // Build the inner ssh command string that rsync will use via -e.
    QStringList parts;
    parts << QStringLiteral("ssh");
    parts << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
    parts << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10");
    if (m_port != 22 && m_port > 0)
        parts << QStringLiteral("-p") << QString::number(m_port);
    if (!m_keyPath.isEmpty())
        parts << QStringLiteral("-i") << m_keyPath;
    return parts;
}

void RemoteSyncService::pushDirectory(const QString &localDir, const QString &remoteDir)
{
    const QString src = localDir.endsWith(QLatin1Char('/')) ? localDir : localDir + QLatin1Char('/');
    const QString dst = QStringLiteral("%1@%2:%3").arg(m_user, m_host, remoteDir);

    QStringList args;
    args << QStringLiteral("-avz") << QStringLiteral("--delete") << QStringLiteral("--progress");
    args << QStringLiteral("-e") << sshCommandOption().join(QLatin1Char(' '));
    args << src << dst;

    startProcess(QStringLiteral("rsync"), args);
}

void RemoteSyncService::pullDirectory(const QString &remoteDir, const QString &localDir)
{
    const QString src = QStringLiteral("%1@%2:%3").arg(m_user, m_host,
        remoteDir.endsWith(QLatin1Char('/')) ? remoteDir : remoteDir + QLatin1Char('/'));
    const QString dst = localDir;

    QStringList args;
    args << QStringLiteral("-avz") << QStringLiteral("--progress");
    args << QStringLiteral("-e") << sshCommandOption().join(QLatin1Char(' '));
    args << src << dst;

    startProcess(QStringLiteral("rsync"), args);
}

void RemoteSyncService::uploadFile(const QString &localPath, const QString &remotePath)
{
    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
    args << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10");
    if (m_port != 22 && m_port > 0)
        args << QStringLiteral("-P") << QString::number(m_port);
    if (!m_keyPath.isEmpty())
        args << QStringLiteral("-i") << m_keyPath;
    args << localPath;
    args << QStringLiteral("%1@%2:%3").arg(m_user, m_host, remotePath);

    startProcess(QStringLiteral("scp"), args);
}

void RemoteSyncService::downloadFile(const QString &remotePath, const QString &localPath)
{
    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new");
    args << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10");
    if (m_port != 22 && m_port > 0)
        args << QStringLiteral("-P") << QString::number(m_port);
    if (!m_keyPath.isEmpty())
        args << QStringLiteral("-i") << m_keyPath;
    args << QStringLiteral("%1@%2:%3").arg(m_user, m_host, remotePath);
    args << localPath;

    startProcess(QStringLiteral("scp"), args);
}

void RemoteSyncService::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
}

void RemoteSyncService::startProcess(const QString &program, const QStringList &args)
{
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning)
            m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString out = QString::fromUtf8(m_process->readAllStandardOutput());
        const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines)
            emit progress(line);
    });

    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        const bool ok = (status == QProcess::NormalExit && exitCode == 0);
        const QString err = ok ? QString()
                               : tr("Process exited with code %1").arg(exitCode);
        emit finished(ok, err);
    });

    connect(m_process, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
        Q_UNUSED(error)
        emit finished(false, m_process->errorString());
    });

    m_process->start(program, args);
}
