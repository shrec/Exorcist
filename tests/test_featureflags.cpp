#include <QTest>
#include <QSignalSpy>

#include "agent/featureflags.h"

class TestFeatureFlags : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultFlagsEnabled();
    void testTelemetryDefaultDisabled();
    void testSetAndGetFlag();
    void testSignalEmittedOnChange();
    void testNoSignalOnSameValue();
    void testGenericFlagAccess();
    void testCustomFlag();
    void testAllFlags();
};

void TestFeatureFlags::testDefaultFlagsEnabled()
{
    FeatureFlags flags;
    QVERIFY(flags.inlineChatEnabled());
    QVERIFY(flags.ghostTextEnabled());
    QVERIFY(flags.codeReviewEnabled());
    QVERIFY(flags.memoryEnabled());
    QVERIFY(flags.autoCompactionEnabled());
    QVERIFY(flags.renameSuggestionsEnabled());
    QVERIFY(flags.mcpEnabled());
}

void TestFeatureFlags::testTelemetryDefaultDisabled()
{
    FeatureFlags flags;
    QVERIFY(!flags.telemetryEnabled());
}

void TestFeatureFlags::testSetAndGetFlag()
{
    FeatureFlags flags;
    flags.setGhostTextEnabled(false);
    QVERIFY(!flags.ghostTextEnabled());

    flags.setGhostTextEnabled(true);
    QVERIFY(flags.ghostTextEnabled());
}

void TestFeatureFlags::testSignalEmittedOnChange()
{
    FeatureFlags flags;
    QSignalSpy spy(&flags, &FeatureFlags::flagChanged);

    flags.setMemoryEnabled(false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("memory"));
    QCOMPARE(spy.at(0).at(1).toBool(), false);
}

void TestFeatureFlags::testNoSignalOnSameValue()
{
    FeatureFlags flags;
    // Set to current value — should not emit
    QSignalSpy spy(&flags, &FeatureFlags::flagChanged);
    flags.setInlineChatEnabled(true); // already true by default
    QCOMPARE(spy.count(), 0);
}

void TestFeatureFlags::testGenericFlagAccess()
{
    FeatureFlags flags;
    // Access a non-existent flag with default
    QVERIFY(flags.flag(QStringLiteral("unknown")));
    QVERIFY(!flags.flag(QStringLiteral("unknown"), false));
}

void TestFeatureFlags::testCustomFlag()
{
    FeatureFlags flags;
    flags.setFlag(QStringLiteral("myCustomFlag"), true);
    QVERIFY(flags.flag(QStringLiteral("myCustomFlag")));

    flags.setFlag(QStringLiteral("myCustomFlag"), false);
    QVERIFY(!flags.flag(QStringLiteral("myCustomFlag")));
}

void TestFeatureFlags::testAllFlags()
{
    FeatureFlags flags;
    flags.setFlag(QStringLiteral("customA"), true);
    flags.setFlag(QStringLiteral("customB"), false);

    QStringList all = flags.allFlags();
    QVERIFY(all.contains(QStringLiteral("customA")));
    QVERIFY(all.contains(QStringLiteral("customB")));
}

QTEST_MAIN(TestFeatureFlags)
#include "test_featureflags.moc"
