#include <QTest>
#include "serviceregistry.h"

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
};

QTEST_MAIN(TestServiceRegistry)
#include "test_serviceregistry.moc"
