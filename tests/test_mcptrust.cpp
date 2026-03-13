#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

#include "mcp/mcpservermanager.h"

class TestMcpTrust : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        // Ensure QSettings uses a consistent scope for tests
        QCoreApplication::setOrganizationName(QStringLiteral("ExorcistTest"));
        QCoreApplication::setApplicationName(QStringLiteral("TestMcpTrust"));

        // Clear any persisted trust between tests
        QSettings s;
        s.remove(QStringLiteral("mcp/trustedWorkspaces"));
    }

    void untrustedWorkspaceBlocksStart()
    {
        MCPServerManager mgr;
        mgr.setWorkspaceRoot(QDir::tempPath() + QStringLiteral("/mcp_test_ws"));

        MCPServerConfig cfg;
        cfg.name = QStringLiteral("test-server");
        cfg.command = QStringLiteral("echo");
        cfg.transport = MCPServerConfig::Stdio;

        QSignalSpy trustSpy(&mgr, &MCPServerManager::trustRequired);

        bool started = mgr.startServer(cfg);

        QVERIFY(!started);
        QCOMPARE(trustSpy.count(), 1);
    }

    void trustedWorkspaceAllowsStart()
    {
        MCPServerManager mgr;
        const QString wsRoot = QDir::tempPath() + QStringLiteral("/mcp_test_ws_trusted");
        QDir().mkpath(wsRoot);
        mgr.setWorkspaceRoot(wsRoot);
        mgr.trustWorkspace();

        QVERIFY(mgr.isWorkspaceTrusted());
    }

    void untrustRevokesAccess()
    {
        MCPServerManager mgr;
        const QString wsRoot = QDir::tempPath() + QStringLiteral("/mcp_untrust_test");
        QDir().mkpath(wsRoot);
        mgr.setWorkspaceRoot(wsRoot);
        mgr.trustWorkspace();

        QVERIFY(mgr.isWorkspaceTrusted());

        mgr.untrustWorkspace();
        QVERIFY(!mgr.isWorkspaceTrusted());
    }

    void trustPersistedAcrossInstances()
    {
        const QString wsRoot = QDir::tempPath() + QStringLiteral("/mcp_persist_test");
        QDir().mkpath(wsRoot);

        {
            MCPServerManager mgr;
            mgr.setWorkspaceRoot(wsRoot);
            mgr.trustWorkspace();
        }

        MCPServerManager mgr2;
        mgr2.setWorkspaceRoot(wsRoot);
        QVERIFY(mgr2.isWorkspaceTrusted());

        // Cleanup
        mgr2.untrustWorkspace();
    }

    void noWorkspaceRootAlwaysTrusted()
    {
        MCPServerManager mgr;
        // No setWorkspaceRoot — should be trusted by default
        QVERIFY(mgr.isWorkspaceTrusted());
    }

    void loadConfigReadsValidJson()
    {
        const QString wsRoot = QDir::tempPath() + QStringLiteral("/mcp_config_test");
        QDir().mkpath(wsRoot);

        QJsonObject server;
        server[QStringLiteral("command")] = QStringLiteral("node");
        server[QStringLiteral("args")] = QJsonArray{QStringLiteral("index.js")};
        QJsonObject servers;
        servers[QStringLiteral("my-tool")] = server;
        QJsonObject root;
        root[QStringLiteral("servers")] = servers;

        QFile f(wsRoot + QStringLiteral("/.mcp.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
        f.close();

        MCPServerManager mgr;
        auto configs = mgr.loadConfig(wsRoot);
        QCOMPARE(configs.size(), 1);
        QCOMPARE(configs[0].name, QStringLiteral("my-tool"));
        QCOMPARE(configs[0].command, QStringLiteral("node"));
        QCOMPARE(configs[0].args.size(), 1);

        // Cleanup
        f.remove();
        QDir(wsRoot).removeRecursively();
    }
};

QTEST_MAIN(TestMcpTrust)
#include "test_mcptrust.moc"
