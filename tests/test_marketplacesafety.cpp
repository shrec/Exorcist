#include <QTest>
#include <QDir>
#include <QFile>

#include "plugin/pluginmarketplaceservice.h"

class TestMarketplaceSafety : public QObject
{
    Q_OBJECT

private slots:
    void validPluginId()
    {
        PluginMarketplaceService svc;
        svc.setPluginsDirectory(QDir::tempPath() + QStringLiteral("/mk_test_plugins"));

        // Valid ID should not error during uninstall "not found" path
        QString err;
        bool ok = svc.uninstallPlugin(QStringLiteral("my-plugin"), &err);
        // Not installed, so false — but NOT rejected for invalid ID
        QVERIFY(!ok);
        QVERIFY(err.contains(QStringLiteral("not found")));
    }

    void pathTraversalBlockedInId()
    {
        PluginMarketplaceService svc;
        svc.setPluginsDirectory(QDir::tempPath() + QStringLiteral("/mk_test_plugins"));

        // Dot-dot traversal must be rejected
        QString err;
        bool ok = svc.uninstallPlugin(QStringLiteral("../../../etc"), &err);
        QVERIFY(!ok);
        QVERIFY(err.contains(QStringLiteral("Invalid plugin ID")));
    }

    void slashInIdBlocked()
    {
        PluginMarketplaceService svc;
        svc.setPluginsDirectory(QDir::tempPath() + QStringLiteral("/mk_test_plugins"));

        QString err;
        bool ok = svc.uninstallPlugin(QStringLiteral("foo/bar"), &err);
        QVERIFY(!ok);
        QVERIFY(err.contains(QStringLiteral("Invalid plugin ID")));
    }

    void backslashInIdBlocked()
    {
        PluginMarketplaceService svc;
        svc.setPluginsDirectory(QDir::tempPath() + QStringLiteral("/mk_test_plugins"));

        QString err;
        bool ok = svc.uninstallPlugin(QStringLiteral("foo\\bar"), &err);
        QVERIFY(!ok);
        QVERIFY(err.contains(QStringLiteral("Invalid plugin ID")));
    }

    void emptyIdBlocked()
    {
        PluginMarketplaceService svc;
        svc.setPluginsDirectory(QDir::tempPath() + QStringLiteral("/mk_test_plugins"));

        QString err;
        bool ok = svc.uninstallPlugin(QString(), &err);
        QVERIFY(!ok);
        QVERIFY(err.contains(QStringLiteral("Invalid plugin ID")));
    }

    void simpleIdWithDotsAllowed()
    {
        // A single dot (not "..") should be allowed: "com.example.plugin"
        PluginMarketplaceService svc;
        svc.setPluginsDirectory(QDir::tempPath() + QStringLiteral("/mk_test_plugins"));

        QString err;
        bool ok = svc.uninstallPlugin(QStringLiteral("com.example.plugin"), &err);
        QVERIFY(!ok);
        // Should fail with "not found", not "Invalid plugin ID"
        QVERIFY(err.contains(QStringLiteral("not found")));
    }
};

QTEST_MAIN(TestMarketplaceSafety)
#include "test_marketplacesafety.moc"
