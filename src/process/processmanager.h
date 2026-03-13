#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <memory>
#include <unordered_map>

// ── ProcessManager ────────────────────────────────────────────────────────────
//
// Centralized process lifecycle management for an Exorcist IDE instance.

#include "bridgeclient.h"

class ProcessManager : public QObject
{
    Q_OBJECT

public:
    explicit ProcessManager(QObject *parent = nullptr);

    void setBridgeClient(BridgeClient *client) { m_bridgeClient = client; }

    // ── Local processes ───────────────────────────────────────────────

    struct ProcessInfo {
        QString   id;
        QString   program;
        QStringList args;
        QString   workDir;
        qint64    pid       = -1;
        bool      running   = false;
        bool      shared    = false;
    };

    QString startLocal(const QString &program,
                       const QStringList &args = {},
                       const QString &workDir = {});
    void killLocal(const QString &id);
    void terminateLocal(const QString &id);
    ProcessInfo processInfo(const QString &id) const;
    QList<ProcessInfo> localProcesses() const;
    QProcess *localProcess(const QString &id) const;

    // ── Shared processes (via BridgeClient) ───────────────────────────

    void startShared(const QString &name,
                     const QString &program,
                     const QStringList &args = {},
                     const QString &workDir = {});
    void killShared(const QString &name);

    // ── Shutdown ──────────────────────────────────────────────────────

    void shutdownLocal();

signals:
    void processStarted(const QString &id);
    void processFinished(const QString &id, int exitCode, bool crashed);
    void processError(const QString &id, const QString &error);
    void sharedProcessStarted(const QString &name);

private:
    QString nextId()
    {
        return QStringLiteral("proc-%1").arg(m_nextLocalId++);
    }

    BridgeClient *m_bridgeClient = nullptr;
    int           m_nextLocalId  = 1;

    struct StdStringHash {
        std::size_t operator()(const QString &s) const { return qHash(s); }
    };
    std::unordered_map<QString, std::unique_ptr<QProcess>, StdStringHash> m_localProcesses;
    QHash<QString, ProcessInfo>               m_localInfos;
};
