#include <QTest>
#include "serviceregistry.h"

// ── Typed service stubs ──────────────────────────────────────────────────────

class StubServiceA : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
};

class StubServiceB : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
};

class TestServiceRegistry : public QObject
{
    Q_OBJECT

private slots:
    void registerAndRetrieve()
    {
        ServiceRegistry reg;
        auto *svc = new QObject(&reg);
        svc->setObjectName(QStringLiteral("dummy"));

        reg.registerService(QStringLiteral("mySvc"), svc);
        QCOMPARE(reg.service(QStringLiteral("mySvc")), svc);
    }

    void unknownKeyReturnsNull()
    {
        ServiceRegistry reg;
        QVERIFY(!reg.service(QStringLiteral("nonexistent")));
    }

    void keysListsRegistered()
    {
        ServiceRegistry reg;
        auto *a = new QObject(&reg);
        auto *b = new QObject(&reg);

        reg.registerService(QStringLiteral("alpha"), a);
        reg.registerService(QStringLiteral("beta"),  b);

        QStringList k = reg.keys();
        k.sort();
        QCOMPARE(k, (QStringList{QStringLiteral("alpha"), QStringLiteral("beta")}));
    }

    void overwriteService()
    {
        ServiceRegistry reg;
        auto *a = new QObject(&reg);
        auto *b = new QObject(&reg);

        reg.registerService(QStringLiteral("svc"), a);
        QCOMPARE(reg.service(QStringLiteral("svc")), a);

        reg.registerService(QStringLiteral("svc"), b);
        QCOMPARE(reg.service(QStringLiteral("svc")), b);
    }

    // ── hasService ───────────────────────────────────────────────────────

    void hasServiceCheck()
    {
        ServiceRegistry reg;
        QVERIFY(!reg.hasService(QStringLiteral("x")));
        auto *s = new QObject(&reg);
        reg.registerService(QStringLiteral("x"), s);
        QVERIFY(reg.hasService(QStringLiteral("x")));
    }

    // ── typed resolve ────────────────────────────────────────────────────

    void typedResolveCorrectType()
    {
        ServiceRegistry reg;
        auto *a = new StubServiceA(&reg);
        reg.registerService(QStringLiteral("a"), a);
        QCOMPARE(reg.service<StubServiceA>(QStringLiteral("a")), a);
    }

    void typedResolveWrongTypeReturnsNull()
    {
        ServiceRegistry reg;
        auto *a = new StubServiceA(&reg);
        reg.registerService(QStringLiteral("a"), a);
        QVERIFY(reg.service<StubServiceB>(QStringLiteral("a")) == nullptr);
    }

    // ── contract / versioning ────────────────────────────────────────────

    void defaultContractIsV1_0()
    {
        ServiceRegistry reg;
        auto *s = new QObject(&reg);
        reg.registerService(QStringLiteral("s"), s);
        auto c = reg.contract(QStringLiteral("s"));
        QCOMPARE(c.majorVersion, 1);
        QCOMPARE(c.minorVersion, 0);
        QVERIFY(c.description.isEmpty());
    }

    void registerWithExplicitContract()
    {
        ServiceRegistry reg;
        auto *s = new QObject(&reg);
        ServiceContract sc{2, 3, QStringLiteral("test svc")};
        reg.registerService(QStringLiteral("s"), s, sc);
        auto c = reg.contract(QStringLiteral("s"));
        QCOMPARE(c.majorVersion, 2);
        QCOMPARE(c.minorVersion, 3);
        QCOMPARE(c.description, QStringLiteral("test svc"));
    }

    void contractForUnknownReturnsDefault()
    {
        ServiceRegistry reg;
        auto c = reg.contract(QStringLiteral("x"));
        QCOMPARE(c.majorVersion, 1);
        QCOMPARE(c.minorVersion, 0);
    }

    void isCompatibleExactMatch()
    {
        ServiceRegistry reg;
        auto *s = new QObject(&reg);
        reg.registerService(QStringLiteral("s"), s, {2, 5, {}});
        QVERIFY(reg.isCompatible(QStringLiteral("s"), 2, 5));
        QVERIFY(reg.isCompatible(QStringLiteral("s"), 2, 0));
        QVERIFY(reg.isCompatible(QStringLiteral("s"), 2, 3));
    }

    void isCompatibleMinorTooHigh()
    {
        ServiceRegistry reg;
        auto *s = new QObject(&reg);
        reg.registerService(QStringLiteral("s"), s, {2, 3, {}});
        QVERIFY(!reg.isCompatible(QStringLiteral("s"), 2, 4));
    }

    void isCompatibleMajorMismatch()
    {
        ServiceRegistry reg;
        auto *s = new QObject(&reg);
        reg.registerService(QStringLiteral("s"), s, {2, 0, {}});
        QVERIFY(!reg.isCompatible(QStringLiteral("s"), 1));
        QVERIFY(!reg.isCompatible(QStringLiteral("s"), 3));
    }

    void isCompatibleUnknownReturnsFalse()
    {
        ServiceRegistry reg;
        QVERIFY(!reg.isCompatible(QStringLiteral("missing"), 1));
    }

    void overwriteContractUpdates()
    {
        ServiceRegistry reg;
        auto *s = new QObject(&reg);
        reg.registerService(QStringLiteral("s"), s, {1, 0, {}});
        QVERIFY(reg.isCompatible(QStringLiteral("s"), 1));

        reg.registerService(QStringLiteral("s"), s, {2, 1, QStringLiteral("v2")});
        QVERIFY(!reg.isCompatible(QStringLiteral("s"), 1));
        QVERIFY(reg.isCompatible(QStringLiteral("s"), 2, 0));
        QVERIFY(reg.isCompatible(QStringLiteral("s"), 2, 1));
        QCOMPARE(reg.contract(QStringLiteral("s")).description, QStringLiteral("v2"));
    }

    void legacyRegisterIsCompatibleWithV1()
    {
        ServiceRegistry reg;
        auto *s = new QObject(&reg);
        reg.registerService(QStringLiteral("s"), s);   // no contract
        QVERIFY(reg.isCompatible(QStringLiteral("s"), 1, 0));
        QVERIFY(!reg.isCompatible(QStringLiteral("s"), 2));
    }
};

QTEST_MAIN(TestServiceRegistry)
#include "test_serviceregistry.moc"
