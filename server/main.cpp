#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QStandardPaths>

#include "process/exobridgecore.h"

#include "mcpbridgeservice.h"
#include "gitwatchservice.h"
#include "authtokenservice.h"

static QString pidFilePath()
{
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::TempLocation);
    const QString user = qEnvironmentVariable("USERNAME",
                             qEnvironmentVariable("USER",
                                 QStringLiteral("default")));
    return dir + QStringLiteral("/exobridge-%1.lock").arg(user);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Exorcist"));
    app.setApplicationName(QStringLiteral("exobridge"));

    // ── Command-line options ───────────────────────────────────────
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("ExoBridge — Exorcist shared daemon"));
    parser.addHelpOption();

    QCommandLineOption idleOpt(
        QStringLiteral("idle-timeout"),
        QStringLiteral("Shut down after <seconds> with no clients (0 = never)."),
        QStringLiteral("seconds"),
        QStringLiteral("0"));
    parser.addOption(idleOpt);

    parser.process(app);

    const int idleTimeout = parser.value(idleOpt).toInt();

    // ── PID / lock file ──────────────────────────────────────────────
    QLockFile lockFile(pidFilePath());
    if (!lockFile.tryLock(0)) {
        qint64 pid = 0;
        QString hostname, appName;
        lockFile.getLockInfo(&pid, &hostname, &appName);
        qInfo("[ExoBridge] Already running (PID %lld) — exiting",
              static_cast<long long>(pid));
        return 0;
    }

    // ── ExoBridge core ────────────────────────────────────────────────────
    ExoBridgeCore bridge;

    // ── Shared services ──────────────────────────────────────────────────
    McpBridgeService mcpService(&bridge);
    bridge.installServiceHandler(
        QStringLiteral("mcp"),
        [&mcpService](const QString &method, const QJsonObject &args,
                      std::function<void(bool, QJsonObject)> respond) {
            mcpService.handleCall(method, args, std::move(respond));
        });

    GitWatchBridgeService gitWatchService(&bridge);
    bridge.installServiceHandler(
        QStringLiteral("gitwatch"),
        [&gitWatchService](const QString &method, const QJsonObject &args,
                           std::function<void(bool, QJsonObject)> respond) {
            gitWatchService.handleCall(method, args, std::move(respond));
        });
    // Broadcast git change notifications to all IDE clients
    QObject::connect(&gitWatchService, &GitWatchBridgeService::repoChanged,
                     &bridge, [&bridge](const QString &repoPath) {
        QJsonObject event;
        event[QLatin1String("type")]     = QStringLiteral("gitChanged");
        event[QLatin1String("repoPath")] = repoPath;
        bridge.broadcastServiceEvent(QStringLiteral("gitwatch"), event);
    });

    AuthTokenBridgeService authService(&bridge);
    bridge.installServiceHandler(
        QStringLiteral("auth"),
        [&authService](const QString &method, const QJsonObject &args,
                       std::function<void(bool, QJsonObject)> respond) {
            authService.handleCall(method, args, std::move(respond));
        });

    if (idleTimeout > 0) {
        bridge.setPersistent(false);
        bridge.setIdleTimeout(idleTimeout);
    }
    // Default: persistent (idleTimeout == 0)

    QObject::connect(&bridge, &ExoBridgeCore::shutdownRequested,
                     &app, &QCoreApplication::quit);

    if (!bridge.start()) {
        qCritical("Failed to start ExoBridge");
        return 1;
    }

    qInfo("[ExoBridge] PID %lld, persistent=%s, idle-timeout=%d s",
          static_cast<long long>(QCoreApplication::applicationPid()),
          bridge.isPersistent() ? "true" : "false",
          idleTimeout);

    const int ret = app.exec();
    mcpService.shutdown();
    gitWatchService.shutdown();
    bridge.stop();
    return ret;
}
