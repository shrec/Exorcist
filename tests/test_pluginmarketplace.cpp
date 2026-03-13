#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "plugin/pluginmarketplaceservice.h"

class TestPluginMarketplace : public QObject
{
    Q_OBJECT

private slots:
    // ── MarketplaceEntry serialization ───────────────────────────────────
    void entry_toJson();
    void entry_fromJson();
    void entry_roundTrip();
    void entry_fromJsonMissingFields();
    void entry_tagsPreserved();

    // ── PluginMarketplaceService — registry loading ──────────────────────
    void service_constructsCleanly();
    void service_loadFromFile_valid();
    void service_loadFromFile_invalid();
    void service_loadFromFile_empty();
    void service_loadFromFile_multipleEntries();
    void service_loadFromFile_nonexistent();
    void service_setPluginsDirectory();

    // ── Uninstall ────────────────────────────────────────────────────────
    void service_uninstallPlugin_exists();
    void service_uninstallPlugin_missing();

    // ── availableUpdates ─────────────────────────────────────────────────
    void service_availableUpdates_noManager();

    // ── downloadAndInstall — no URL ──────────────────────────────────────
    void service_downloadAndInstall_noUrl();

private:
    QByteArray makeRegistryJson(const QList<MarketplaceEntry> &entries)
    {
        QJsonArray arr;
        for (const auto &e : entries)
            arr.append(e.toJson());
        return QJsonDocument(arr).toJson();
    }
};

// ── MarketplaceEntry ─────────────────────────────────────────────────────────

void TestPluginMarketplace::entry_toJson()
{
    MarketplaceEntry e;
    e.id = QStringLiteral("org.test.plugin");
    e.name = QStringLiteral("Test Plugin");
    e.version = QStringLiteral("2.0.0");
    e.author = QStringLiteral("Author");
    e.description = QStringLiteral("A test plugin");
    e.homepage = QStringLiteral("https://example.com");
    e.downloadUrl = QStringLiteral("https://example.com/test.zip");
    e.minIdeVersion = QStringLiteral("1.0.0");
    e.tags = {QStringLiteral("test"), QStringLiteral("sample")};

    QJsonObject j = e.toJson();
    QCOMPARE(j["id"].toString(), QStringLiteral("org.test.plugin"));
    QCOMPARE(j["name"].toString(), QStringLiteral("Test Plugin"));
    QCOMPARE(j["version"].toString(), QStringLiteral("2.0.0"));
    QCOMPARE(j["tags"].toArray().size(), 2);
}

void TestPluginMarketplace::entry_fromJson()
{
    QJsonObject j;
    j["id"] = QStringLiteral("org.test.x");
    j["name"] = QStringLiteral("X");
    j["version"] = QStringLiteral("1.0.0");
    j["author"] = QStringLiteral("A");
    j["description"] = QStringLiteral("Desc");
    j["homepage"] = QStringLiteral("https://x.com");
    j["downloadUrl"] = QStringLiteral("https://x.com/x.zip");
    j["minIdeVersion"] = QStringLiteral("0.9.0");
    j["tags"] = QJsonArray{QStringLiteral("t1")};

    auto e = MarketplaceEntry::fromJson(j);
    QCOMPARE(e.id, QStringLiteral("org.test.x"));
    QCOMPARE(e.name, QStringLiteral("X"));
    QCOMPARE(e.downloadUrl, QStringLiteral("https://x.com/x.zip"));
    QCOMPARE(e.tags.size(), 1);
}

void TestPluginMarketplace::entry_roundTrip()
{
    MarketplaceEntry orig;
    orig.id = QStringLiteral("org.rt.test");
    orig.name = QStringLiteral("RoundTrip");
    orig.version = QStringLiteral("3.1.4");
    orig.author = QStringLiteral("RT");
    orig.description = QStringLiteral("Round trip test");
    orig.tags = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};

    auto restored = MarketplaceEntry::fromJson(orig.toJson());
    QCOMPARE(restored.id, orig.id);
    QCOMPARE(restored.name, orig.name);
    QCOMPARE(restored.version, orig.version);
    QCOMPARE(restored.tags, orig.tags);
}

void TestPluginMarketplace::entry_fromJsonMissingFields()
{
    QJsonObject j;
    j["name"] = QStringLiteral("NoId");

    auto e = MarketplaceEntry::fromJson(j);
    QVERIFY(e.id.isEmpty());
    QCOMPARE(e.name, QStringLiteral("NoId"));
    QVERIFY(e.version.isEmpty());
}

void TestPluginMarketplace::entry_tagsPreserved()
{
    MarketplaceEntry e;
    e.id = QStringLiteral("org.tags");
    e.tags = {QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("gamma")};

    QJsonObject j = e.toJson();
    QCOMPARE(j["tags"].toArray().size(), 3);

    auto restored = MarketplaceEntry::fromJson(j);
    QCOMPARE(restored.tags.size(), 3);
    QCOMPARE(restored.tags[1], QStringLiteral("beta"));
}

// ── PluginMarketplaceService ─────────────────────────────────────────────────

void TestPluginMarketplace::service_constructsCleanly()
{
    PluginMarketplaceService svc;
    QVERIFY(svc.entries().isEmpty());
    QVERIFY(!svc.pluginsDirectory().isEmpty());
}

void TestPluginMarketplace::service_loadFromFile_valid()
{
    MarketplaceEntry e;
    e.id = QStringLiteral("org.test.valid");
    e.name = QStringLiteral("Valid");
    e.version = QStringLiteral("1.0");

    QTemporaryFile f;
    f.setFileTemplate(QDir::tempPath() + "/registry_XXXXXX.json");
    QVERIFY(f.open());
    f.write(makeRegistryJson({e}));
    f.close();

    PluginMarketplaceService svc;
    QSignalSpy spy(&svc, &PluginMarketplaceService::registryLoaded);

    bool ok = svc.loadRegistryFromFile(f.fileName());
    QVERIFY(ok);
    QCOMPARE(svc.entries().size(), 1);
    QCOMPARE(svc.entries().first().id, QStringLiteral("org.test.valid"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toInt(), 1);
}

void TestPluginMarketplace::service_loadFromFile_invalid()
{
    QTemporaryFile f;
    f.setFileTemplate(QDir::tempPath() + "/bad_XXXXXX.json");
    QVERIFY(f.open());
    f.write("not json at all {{{");
    f.close();

    PluginMarketplaceService svc;
    QSignalSpy errSpy(&svc, &PluginMarketplaceService::error);

    bool ok = svc.loadRegistryFromFile(f.fileName());
    QVERIFY(!ok);
    QCOMPARE(errSpy.count(), 1);
}

void TestPluginMarketplace::service_loadFromFile_empty()
{
    QTemporaryFile f;
    f.setFileTemplate(QDir::tempPath() + "/empty_XXXXXX.json");
    QVERIFY(f.open());
    f.write("[]");
    f.close();

    PluginMarketplaceService svc;
    QSignalSpy spy(&svc, &PluginMarketplaceService::registryLoaded);

    bool ok = svc.loadRegistryFromFile(f.fileName());
    QVERIFY(!ok); // returns false for empty
    QCOMPARE(svc.entries().size(), 0);
}

void TestPluginMarketplace::service_loadFromFile_multipleEntries()
{
    QList<MarketplaceEntry> entries;
    for (int i = 0; i < 5; ++i) {
        MarketplaceEntry e;
        e.id = QStringLiteral("org.test.%1").arg(i);
        e.name = QStringLiteral("Plugin %1").arg(i);
        e.version = QStringLiteral("1.%1.0").arg(i);
        entries.append(e);
    }

    QTemporaryFile f;
    f.setFileTemplate(QDir::tempPath() + "/multi_XXXXXX.json");
    QVERIFY(f.open());
    f.write(makeRegistryJson(entries));
    f.close();

    PluginMarketplaceService svc;
    svc.loadRegistryFromFile(f.fileName());
    QCOMPARE(svc.entries().size(), 5);
}

void TestPluginMarketplace::service_loadFromFile_nonexistent()
{
    PluginMarketplaceService svc;
    QSignalSpy errSpy(&svc, &PluginMarketplaceService::error);

    bool ok = svc.loadRegistryFromFile(QStringLiteral("/nonexistent/path/reg.json"));
    QVERIFY(!ok);
    QCOMPARE(errSpy.count(), 1);
}

void TestPluginMarketplace::service_setPluginsDirectory()
{
    PluginMarketplaceService svc;
    svc.setPluginsDirectory(QStringLiteral("/custom/path"));
    QCOMPARE(svc.pluginsDirectory(), QStringLiteral("/custom/path"));
}

// ── Uninstall ────────────────────────────────────────────────────────────────

void TestPluginMarketplace::service_uninstallPlugin_exists()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    PluginMarketplaceService svc;
    svc.setPluginsDirectory(tmpDir.path());

    // Create a fake plugin directory
    QDir(tmpDir.path()).mkdir(QStringLiteral("my-plugin"));
    QFile marker(tmpDir.path() + QStringLiteral("/my-plugin/plugin.json"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("{}");
    marker.close();

    QSignalSpy spy(&svc, &PluginMarketplaceService::pluginUninstalled);

    QString err;
    bool ok = svc.uninstallPlugin(QStringLiteral("my-plugin"), &err);
    QVERIFY(ok);
    QVERIFY(err.isEmpty());
    QCOMPARE(spy.count(), 1);
    QVERIFY(!QDir(tmpDir.path() + QStringLiteral("/my-plugin")).exists());
}

void TestPluginMarketplace::service_uninstallPlugin_missing()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    PluginMarketplaceService svc;
    svc.setPluginsDirectory(tmpDir.path());

    QString err;
    bool ok = svc.uninstallPlugin(QStringLiteral("nonexistent"), &err);
    QVERIFY(!ok);
    QVERIFY(!err.isEmpty());
}

// ── Updates ──────────────────────────────────────────────────────────────────

void TestPluginMarketplace::service_availableUpdates_noManager()
{
    PluginMarketplaceService svc;
    auto updates = svc.availableUpdates();
    QVERIFY(updates.isEmpty());
}

// ── Download ─────────────────────────────────────────────────────────────────

void TestPluginMarketplace::service_downloadAndInstall_noUrl()
{
    PluginMarketplaceService svc;
    QSignalSpy errSpy(&svc, &PluginMarketplaceService::error);

    MarketplaceEntry e;
    e.id = QStringLiteral("org.test.nourl");
    e.name = QStringLiteral("NoUrl");

    svc.downloadAndInstall(e);
    QCOMPARE(errSpy.count(), 1);
    QVERIFY(errSpy.first().first().toString().contains(QStringLiteral("No download URL")));
}

QTEST_MAIN(TestPluginMarketplace)
#include "test_pluginmarketplace.moc"
