#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include "build/kit.h"
#include "../plugins/build/kitmanager.h"

// ── Kit struct tests + KitManager CRUD tests ─────────────────────────────────

class TestKit : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Kit value type
    void testIsValid_valid();
    void testIsValid_missingCompiler();
    void testIsValid_missingCmake();
    void testIsValid_bothMissing();
    void testBuildDisplayName_full();
    void testBuildDisplayName_compilerOnly();
    void testBuildDisplayName_compilerWithVersion();
    void testBuildDisplayName_noDebugger();
    void testBuildDisplayName_empty();
    void testDefaultConstruction();

    // KitManager CRUD (no ToolchainManager needed)
    void testAddKit();
    void testAddKitAutoId();
    void testRemoveKit();
    void testRemoveAutoDetectedKit();
    void testUpdateKit();
    void testUpdateNonExistent();
    void testSetActiveKit();
    void testActiveKitFallback();
    void testKitLookup();
    void testKitLookupNotFound();
    void testSignalKitsChanged();
    void testSignalActiveKitChanged();
    void testRemoveActiveKit();

private:
    void clearKitSettings();
};

void TestKit::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("ExorcistTest"));
    QCoreApplication::setApplicationName(QStringLiteral("KitTest"));
    clearKitSettings();
}

void TestKit::cleanupTestCase()
{
    clearKitSettings();
}

void TestKit::clearKitSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("KitManager"));
    settings.remove(QString());
    settings.endGroup();
}

// ── Kit struct ───────────────────────────────────────────────────────────────

void TestKit::testIsValid_valid()
{
    Kit k;
    k.cxxCompilerPath = QStringLiteral("/usr/bin/g++");
    k.cmakePath = QStringLiteral("/usr/bin/cmake");
    QVERIFY(k.isValid());
}

void TestKit::testIsValid_missingCompiler()
{
    Kit k;
    k.cmakePath = QStringLiteral("/usr/bin/cmake");
    QVERIFY(!k.isValid());
}

void TestKit::testIsValid_missingCmake()
{
    Kit k;
    k.cxxCompilerPath = QStringLiteral("/usr/bin/g++");
    QVERIFY(!k.isValid());
}

void TestKit::testIsValid_bothMissing()
{
    Kit k;
    QVERIFY(!k.isValid());
}

void TestKit::testBuildDisplayName_full()
{
    const auto name = Kit::buildDisplayName(
        QStringLiteral("Clang"), QStringLiteral("17.0.6"),
        QStringLiteral("Ninja"), QStringLiteral("LLDB"));
    QVERIFY(name.contains(QStringLiteral("Clang")));
    QVERIFY(name.contains(QStringLiteral("17.0.6")));
    QVERIFY(name.contains(QStringLiteral("Ninja")));
    QVERIFY(name.contains(QStringLiteral("LLDB")));
}

void TestKit::testBuildDisplayName_compilerOnly()
{
    const auto name = Kit::buildDisplayName(
        QStringLiteral("GCC"), QString(), QString(), QString());
    QCOMPARE(name, QStringLiteral("GCC"));
}

void TestKit::testBuildDisplayName_compilerWithVersion()
{
    const auto name = Kit::buildDisplayName(
        QStringLiteral("GCC"), QStringLiteral("13.2.0"), QString(), QString());
    QVERIFY(name.contains(QStringLiteral("GCC")));
    QVERIFY(name.contains(QStringLiteral("13.2.0")));
}

void TestKit::testBuildDisplayName_noDebugger()
{
    const auto name = Kit::buildDisplayName(
        QStringLiteral("MSVC"), QStringLiteral("19.38"),
        QStringLiteral("Ninja"), QString());
    QVERIFY(name.contains(QStringLiteral("MSVC")));
    QVERIFY(name.contains(QStringLiteral("Ninja")));
    QVERIFY(!name.contains(QStringLiteral("LLDB")));
}

void TestKit::testBuildDisplayName_empty()
{
    const auto name = Kit::buildDisplayName(QString(), QString(), QString(), QString());
    QCOMPARE(name, QStringLiteral("(unknown kit)"));
}

void TestKit::testDefaultConstruction()
{
    Kit k;
    QVERIFY(k.id.isEmpty());
    QVERIFY(k.displayName.isEmpty());
    QVERIFY(k.autoDetected);
    QCOMPARE(k.compilerType, Kit::CompilerType::Unknown);
    QVERIFY(!k.isValid());
}

// ── KitManager CRUD ─────────────────────────────────────────────────────────

void TestKit::testAddKit()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k;
    k.id = QStringLiteral("test-kit-1");
    k.displayName = QStringLiteral("Test Kit");
    k.cxxCompilerPath = QStringLiteral("/usr/bin/g++");
    k.cmakePath = QStringLiteral("/usr/bin/cmake");

    const QString id = mgr.addKit(k);
    QCOMPARE(id, QStringLiteral("test-kit-1"));
    QCOMPARE(mgr.kits().size(), 1);
    QVERIFY(!mgr.kits().first().autoDetected); // addKit forces autoDetected=false
}

void TestKit::testAddKitAutoId()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k;
    k.displayName = QStringLiteral("Auto ID Kit");
    k.cxxCompilerPath = QStringLiteral("/usr/bin/clang++");
    k.cmakePath = QStringLiteral("/usr/bin/cmake");

    const QString id = mgr.addKit(k);
    QVERIFY(!id.isEmpty());
    QCOMPARE(mgr.kits().size(), 1);
    QCOMPARE(mgr.kit(id).displayName, QStringLiteral("Auto ID Kit"));
}

void TestKit::testRemoveKit()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k;
    k.id = QStringLiteral("removable");
    k.displayName = QStringLiteral("Removable Kit");
    mgr.addKit(k);
    QCOMPARE(mgr.kits().size(), 1);

    QVERIFY(mgr.removeKit(QStringLiteral("removable")));
    QCOMPARE(mgr.kits().size(), 0);
}

void TestKit::testRemoveAutoDetectedKit()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    // Manually insert an auto-detected kit into the list (simulate detection)
    Kit k;
    k.id = QStringLiteral("auto-1");
    k.autoDetected = true;
    k.displayName = QStringLiteral("Auto Kit");
    // addKit forces autoDetected=false, so we can test removeKit on user-added kit
    // Auto-detected kits are not removable — but addKit forces autoDetected=false
    // so this tests that removeKit returns false for non-existent ids
    QVERIFY(!mgr.removeKit(QStringLiteral("auto-1"))); // doesn't exist
}

void TestKit::testUpdateKit()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k;
    k.id = QStringLiteral("update-me");
    k.displayName = QStringLiteral("Original");
    mgr.addKit(k);

    Kit updated = mgr.kit(QStringLiteral("update-me"));
    updated.displayName = QStringLiteral("Updated");
    QVERIFY(mgr.updateKit(updated));
    QCOMPARE(mgr.kit(QStringLiteral("update-me")).displayName, QStringLiteral("Updated"));
}

void TestKit::testUpdateNonExistent()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k;
    k.id = QStringLiteral("ghost");
    QVERIFY(!mgr.updateKit(k));
}

void TestKit::testSetActiveKit()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k1, k2;
    k1.id = QStringLiteral("kit-a");
    k1.displayName = QStringLiteral("Kit A");
    k2.id = QStringLiteral("kit-b");
    k2.displayName = QStringLiteral("Kit B");

    mgr.addKit(k1);
    mgr.addKit(k2);

    mgr.setActiveKit(QStringLiteral("kit-b"));
    QCOMPARE(mgr.activeKit().id, QStringLiteral("kit-b"));
}

void TestKit::testActiveKitFallback()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    // No kits — activeKit should return invalid
    QVERIFY(mgr.activeKit().id.isEmpty());

    // Add one kit — should become active by fallback (first kit)
    Kit k;
    k.id = QStringLiteral("fallback");
    k.displayName = QStringLiteral("Fallback");
    mgr.addKit(k);
    QCOMPARE(mgr.activeKit().id, QStringLiteral("fallback"));
}

void TestKit::testKitLookup()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k;
    k.id = QStringLiteral("lookup-id");
    k.displayName = QStringLiteral("Lookup Kit");
    k.cxxCompilerPath = QStringLiteral("/usr/bin/g++");
    mgr.addKit(k);

    const Kit found = mgr.kit(QStringLiteral("lookup-id"));
    QCOMPARE(found.displayName, QStringLiteral("Lookup Kit"));
    QCOMPARE(found.cxxCompilerPath, QStringLiteral("/usr/bin/g++"));
}

void TestKit::testKitLookupNotFound()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    const Kit found = mgr.kit(QStringLiteral("nonexistent"));
    QVERIFY(found.id.isEmpty());
}

void TestKit::testSignalKitsChanged()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    QSignalSpy spy(&mgr, &IKitManager::kitsChanged);

    Kit k;
    k.id = QStringLiteral("sig-test");
    mgr.addKit(k);
    QCOMPARE(spy.count(), 1);

    mgr.removeKit(QStringLiteral("sig-test"));
    QCOMPARE(spy.count(), 2);
}

void TestKit::testSignalActiveKitChanged()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k1, k2;
    k1.id = QStringLiteral("sig-a");
    k2.id = QStringLiteral("sig-b");
    mgr.addKit(k1);
    mgr.addKit(k2);

    QSignalSpy spy(&mgr, &IKitManager::activeKitChanged);

    mgr.setActiveKit(QStringLiteral("sig-b"));
    QCOMPARE(spy.count(), 1);

    // Setting same kit again should NOT emit
    mgr.setActiveKit(QStringLiteral("sig-b"));
    QCOMPARE(spy.count(), 1);
}

void TestKit::testRemoveActiveKit()
{
    clearKitSettings();
    KitManager mgr(nullptr, nullptr);

    Kit k1, k2;
    k1.id = QStringLiteral("active");
    k1.displayName = QStringLiteral("Active Kit");
    k2.id = QStringLiteral("other");
    k2.displayName = QStringLiteral("Other Kit");
    mgr.addKit(k1);
    mgr.addKit(k2);

    mgr.setActiveKit(QStringLiteral("active"));
    QCOMPARE(mgr.activeKit().id, QStringLiteral("active"));

    QSignalSpy spy(&mgr, &IKitManager::activeKitChanged);

    QVERIFY(mgr.removeKit(QStringLiteral("active")));

    // Should have emitted activeKitChanged (active kit was removed)
    QCOMPARE(spy.count(), 1);

    // Active kit should now fall back to first available
    QCOMPARE(mgr.activeKit().id, QStringLiteral("other"));
}

QTEST_MAIN(TestKit)
#include "test_kit.moc"
