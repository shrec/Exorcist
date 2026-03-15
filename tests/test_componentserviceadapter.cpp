#include <QtTest>
#include <QCoreApplication>
#include <memory>

#include "core/icomponentfactory.h"
#include "component/componentregistry.h"
#include "component/componentserviceadapter.h"

// ── Mock Component Factory (minimal) ────────────────────────────────────────

class MockFactory : public QObject, public IComponentFactory
{
    Q_OBJECT
    Q_INTERFACES(IComponentFactory)

public:
    explicit MockFactory(const QString &id, bool multi = true,
                          QObject *parent = nullptr)
        : QObject(parent), m_id(id), m_multi(multi) {}

    QString componentId() const override { return m_id; }
    QString displayName() const override { return QStringLiteral("Display %1").arg(m_id); }
    QString description() const override { return QStringLiteral("Desc"); }
    QString iconPath() const override { return {}; }
    DockArea preferredArea() const override { return DockArea::Bottom; }
    bool supportsMultipleInstances() const override { return m_multi; }
    QString category() const override { return QStringLiteral("Test"); }

    QWidget *createInstance(const QString &instanceId,
                            IHostServices *, QWidget *parent) override
    {
        auto w = std::make_unique<QWidget>(parent);
        w->setObjectName(instanceId);
        return w.release();
    }

    void destroyInstance(const QString &) override {}

private:
    QString m_id;
    bool m_multi;
};

// ── Test Class ──────────────────────────────────────────────────────────────

class TestComponentServiceAdapter : public QObject
{
    Q_OBJECT

private slots:
    void testAvailableComponents();
    void testDisplayName();
    void testSupportsMultipleInstances();
    void testHasComponent();
    void testCreateInstance();
    void testDestroyInstance();
    void testActiveInstances();
    void testInstancesOf();
    void testInstanceCount();
    void testInstanceWidget();
    void testEmptyRegistry();
};

// ── Tests ───────────────────────────────────────────────────────────────────

void TestComponentServiceAdapter::testAvailableComponents()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.a"), true, this);
    reg.registerFactory(&fac);

    QStringList avail = adapter.availableComponents();
    QCOMPARE(avail.size(), 1);
    QVERIFY(avail.contains(QStringLiteral("comp.a")));
}

void TestComponentServiceAdapter::testDisplayName()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.x"), true, this);
    reg.registerFactory(&fac);

    QCOMPARE(adapter.componentDisplayName(QStringLiteral("comp.x")),
             QStringLiteral("Display comp.x"));
    QVERIFY(adapter.componentDisplayName(QStringLiteral("none")).isEmpty());
}

void TestComponentServiceAdapter::testSupportsMultipleInstances()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory multi(QStringLiteral("multi"), true, this);
    MockFactory single(QStringLiteral("single"), false, this);
    reg.registerFactory(&multi);
    reg.registerFactory(&single);

    QVERIFY(adapter.supportsMultipleInstances(QStringLiteral("multi")));
    QVERIFY(!adapter.supportsMultipleInstances(QStringLiteral("single")));
    QVERIFY(!adapter.supportsMultipleInstances(QStringLiteral("absent")));
}

void TestComponentServiceAdapter::testHasComponent()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.z"), true, this);
    reg.registerFactory(&fac);

    QVERIFY(adapter.hasComponent(QStringLiteral("comp.z")));
    QVERIFY(!adapter.hasComponent(QStringLiteral("comp.absent")));
}

void TestComponentServiceAdapter::testCreateInstance()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.t"), true, this);
    reg.registerFactory(&fac);

    QString id = adapter.createInstance(QStringLiteral("comp.t"));
    QVERIFY(!id.isEmpty());
    QCOMPARE(adapter.instanceCount(QStringLiteral("comp.t")), 1);

    // Create nonexistent
    QVERIFY(adapter.createInstance(QStringLiteral("nonexistent")).isEmpty());
}

void TestComponentServiceAdapter::testDestroyInstance()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.d"), true, this);
    reg.registerFactory(&fac);

    QString id = adapter.createInstance(QStringLiteral("comp.d"));
    QVERIFY(!id.isEmpty());
    QVERIFY(adapter.destroyInstance(id));
    QCOMPARE(adapter.instanceCount(QStringLiteral("comp.d")), 0);

    // Destroy nonexistent
    QVERIFY(!adapter.destroyInstance(QStringLiteral("fake-id")));
}

void TestComponentServiceAdapter::testActiveInstances()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.ai"), true, this);
    reg.registerFactory(&fac);

    QVERIFY(adapter.activeInstances().isEmpty());

    adapter.createInstance(QStringLiteral("comp.ai"));
    adapter.createInstance(QStringLiteral("comp.ai"));

    QCOMPARE(adapter.activeInstances().size(), 2);
}

void TestComponentServiceAdapter::testInstancesOf()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory facA(QStringLiteral("comp.a"), true, this);
    MockFactory facB(QStringLiteral("comp.b"), true, this);
    reg.registerFactory(&facA);
    reg.registerFactory(&facB);

    adapter.createInstance(QStringLiteral("comp.a"));
    adapter.createInstance(QStringLiteral("comp.a"));
    adapter.createInstance(QStringLiteral("comp.b"));

    QCOMPARE(adapter.instancesOf(QStringLiteral("comp.a")).size(), 2);
    QCOMPARE(adapter.instancesOf(QStringLiteral("comp.b")).size(), 1);
    QVERIFY(adapter.instancesOf(QStringLiteral("comp.c")).isEmpty());
}

void TestComponentServiceAdapter::testInstanceCount()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.cnt"), true, this);
    reg.registerFactory(&fac);

    QCOMPARE(adapter.instanceCount(QStringLiteral("comp.cnt")), 0);

    adapter.createInstance(QStringLiteral("comp.cnt"));
    QCOMPARE(adapter.instanceCount(QStringLiteral("comp.cnt")), 1);

    adapter.createInstance(QStringLiteral("comp.cnt"));
    QCOMPARE(adapter.instanceCount(QStringLiteral("comp.cnt")), 2);
}

void TestComponentServiceAdapter::testInstanceWidget()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    MockFactory fac(QStringLiteral("comp.w"), true, this);
    reg.registerFactory(&fac);

    QString id = adapter.createInstance(QStringLiteral("comp.w"));
    QWidget *w = adapter.instanceWidget(id);
    QVERIFY(w != nullptr);
    QCOMPARE(w->objectName(), id);

    QVERIFY(adapter.instanceWidget(QStringLiteral("fake")) == nullptr);
}

void TestComponentServiceAdapter::testEmptyRegistry()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    ComponentServiceAdapter adapter(&reg, this);

    QVERIFY(adapter.availableComponents().isEmpty());
    QVERIFY(adapter.activeInstances().isEmpty());
    QVERIFY(!adapter.hasComponent(QStringLiteral("x")));
    QVERIFY(adapter.createInstance(QStringLiteral("x")).isEmpty());
    QVERIFY(!adapter.destroyInstance(QStringLiteral("x")));
    QCOMPARE(adapter.instanceCount(QStringLiteral("x")), 0);
    QVERIFY(adapter.instanceWidget(QStringLiteral("x")) == nullptr);
}

QTEST_MAIN(TestComponentServiceAdapter)
#include "test_componentserviceadapter.moc"
