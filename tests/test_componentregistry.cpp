#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>

#include "core/icomponentfactory.h"
#include "component/componentregistry.h"

// ── Mock Component Factory ──────────────────────────────────────────────────

class MockComponentFactory : public QObject, public IComponentFactory
{
    Q_OBJECT
    Q_INTERFACES(IComponentFactory)

public:
    explicit MockComponentFactory(const QString &id,
                                   bool multiInstance = true,
                                   QObject *parent = nullptr)
        : QObject(parent), m_id(id), m_multi(multiInstance) {}

    QString componentId() const override { return m_id; }
    QString displayName() const override { return QStringLiteral("Mock %1").arg(m_id); }
    QString description() const override { return QStringLiteral("Mock component"); }
    QString iconPath() const override { return {}; }
    DockArea preferredArea() const override { return DockArea::Bottom; }
    bool supportsMultipleInstances() const override { return m_multi; }
    QString category() const override { return QStringLiteral("Test"); }

    QWidget *createInstance(const QString &instanceId,
                            IHostServices *hostServices,
                            QWidget *parent) override
    {
        Q_UNUSED(hostServices);
        auto *w = new QWidget(parent);
        w->setObjectName(instanceId);
        m_created.append(instanceId);
        return w;
    }

    void destroyInstance(const QString &instanceId) override
    {
        m_destroyed.append(instanceId);
    }

    QStringList m_created;
    QStringList m_destroyed;

private:
    QString m_id;
    bool m_multi;
};

// ── Test Class ──────────────────────────────────────────────────────────────

class TestComponentRegistry : public QObject
{
    Q_OBJECT

private slots:
    void testRegisterFactory();
    void testRegisterDuplicate();
    void testRegisterNull();
    void testCreateSingleInstance();
    void testCreateMultipleInstances();
    void testSingleInstanceComponent();
    void testDestroyInstance();
    void testDestroyAllInstances();
    void testAvailableComponents();
    void testHasComponent();
    void testInstanceCount();
    void testInstancesOf();
    void testInstanceWidget();
    void testLoadEmptyDirectory();
    void testLoadNonexistentDirectory();
    void testSignals();
};

void TestComponentRegistry::testRegisterFactory()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.widget"), true, this);

    QVERIFY(reg.registerFactory(fac.get()));
    QVERIFY(reg.hasComponent(QStringLiteral("test.widget")));
    QCOMPARE(reg.availableComponents().size(), 1);
}

void TestComponentRegistry::testRegisterDuplicate()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac1 = std::make_unique<MockComponentFactory>(QStringLiteral("test.dup"), true, this);
    auto fac2 = std::make_unique<MockComponentFactory>(QStringLiteral("test.dup"), true, this);

    QVERIFY(reg.registerFactory(fac1.get()));
    QVERIFY(!reg.registerFactory(fac2.get())); // duplicate
    QCOMPARE(reg.availableComponents().size(), 1);
}

void TestComponentRegistry::testRegisterNull()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    QVERIFY(!reg.registerFactory(nullptr));
    QCOMPARE(reg.availableComponents().size(), 0);
}

void TestComponentRegistry::testCreateSingleInstance()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.panel"), true, this);
    reg.registerFactory(fac.get());

    QString id = reg.createInstance(QStringLiteral("test.panel"));
    QVERIFY(!id.isEmpty());
    QCOMPARE(reg.instanceCount(QStringLiteral("test.panel")), 1);
    QVERIFY(reg.instanceWidget(id) != nullptr);
    QCOMPARE(fac->m_created.size(), 1);
}

void TestComponentRegistry::testCreateMultipleInstances()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.multi"), true, this);
    reg.registerFactory(fac.get());

    QString id1 = reg.createInstance(QStringLiteral("test.multi"));
    QString id2 = reg.createInstance(QStringLiteral("test.multi"));
    QString id3 = reg.createInstance(QStringLiteral("test.multi"));

    QVERIFY(!id1.isEmpty());
    QVERIFY(!id2.isEmpty());
    QVERIFY(!id3.isEmpty());
    QVERIFY(id1 != id2);
    QVERIFY(id2 != id3);
    QCOMPARE(reg.instanceCount(QStringLiteral("test.multi")), 3);
    QCOMPARE(fac->m_created.size(), 3);
}

void TestComponentRegistry::testSingleInstanceComponent()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.single"), false, this);
    reg.registerFactory(fac.get());

    QString id1 = reg.createInstance(QStringLiteral("test.single"));
    QVERIFY(!id1.isEmpty());

    // Second instance must fail
    QString id2 = reg.createInstance(QStringLiteral("test.single"));
    QVERIFY(id2.isEmpty());
    QCOMPARE(reg.instanceCount(QStringLiteral("test.single")), 1);
}

void TestComponentRegistry::testDestroyInstance()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.destroy"), true, this);
    reg.registerFactory(fac.get());

    QString id = reg.createInstance(QStringLiteral("test.destroy"));
    QCOMPARE(reg.instanceCount(QStringLiteral("test.destroy")), 1);

    QVERIFY(reg.destroyInstance(id));
    QCOMPARE(reg.instanceCount(QStringLiteral("test.destroy")), 0);
    QVERIFY(reg.instanceWidget(id) == nullptr);
    QCOMPARE(fac->m_destroyed.size(), 1);

    // Double destroy must fail
    QVERIFY(!reg.destroyInstance(id));
}

void TestComponentRegistry::testDestroyAllInstances()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.all"), true, this);
    reg.registerFactory(fac.get());

    reg.createInstance(QStringLiteral("test.all"));
    reg.createInstance(QStringLiteral("test.all"));
    reg.createInstance(QStringLiteral("test.all"));
    QCOMPARE(reg.instanceCount(QStringLiteral("test.all")), 3);

    reg.destroyAllInstances(QStringLiteral("test.all"));
    QCOMPARE(reg.instanceCount(QStringLiteral("test.all")), 0);
}

void TestComponentRegistry::testAvailableComponents()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac1 = std::make_unique<MockComponentFactory>(QStringLiteral("a.comp"), true, this);
    auto fac2 = std::make_unique<MockComponentFactory>(QStringLiteral("b.comp"), false, this);

    reg.registerFactory(fac1.get());
    reg.registerFactory(fac2.get());

    auto comps = reg.availableComponents();
    QCOMPARE(comps.size(), 2);
    QVERIFY(comps.contains(QStringLiteral("a.comp")));
    QVERIFY(comps.contains(QStringLiteral("b.comp")));
}

void TestComponentRegistry::testHasComponent()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    QVERIFY(!reg.hasComponent(QStringLiteral("nonexistent")));

    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.exists"), true, this);
    reg.registerFactory(fac.get());

    QVERIFY(reg.hasComponent(QStringLiteral("test.exists")));
    QVERIFY(!reg.hasComponent(QStringLiteral("test.nope")));
}

void TestComponentRegistry::testInstanceCount()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    QCOMPARE(reg.instanceCount(QStringLiteral("test.count")), 0);

    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.count"), true, this);
    reg.registerFactory(fac.get());

    QCOMPARE(reg.instanceCount(QStringLiteral("test.count")), 0);
    reg.createInstance(QStringLiteral("test.count"));
    QCOMPARE(reg.instanceCount(QStringLiteral("test.count")), 1);
    reg.createInstance(QStringLiteral("test.count"));
    QCOMPARE(reg.instanceCount(QStringLiteral("test.count")), 2);
}

void TestComponentRegistry::testInstancesOf()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto facA = std::make_unique<MockComponentFactory>(QStringLiteral("type.a"), true, this);
    auto facB = std::make_unique<MockComponentFactory>(QStringLiteral("type.b"), true, this);
    reg.registerFactory(facA.get());
    reg.registerFactory(facB.get());

    reg.createInstance(QStringLiteral("type.a"));
    reg.createInstance(QStringLiteral("type.a"));
    reg.createInstance(QStringLiteral("type.b"));

    QCOMPARE(reg.instancesOf(QStringLiteral("type.a")).size(), 2);
    QCOMPARE(reg.instancesOf(QStringLiteral("type.b")).size(), 1);
    QCOMPARE(reg.instancesOf(QStringLiteral("type.c")).size(), 0);
}

void TestComponentRegistry::testInstanceWidget()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.widget"), true, this);
    reg.registerFactory(fac.get());

    QVERIFY(reg.instanceWidget(QStringLiteral("nonexistent")) == nullptr);

    QString id = reg.createInstance(QStringLiteral("test.widget"));
    QWidget *w = reg.instanceWidget(id);
    QVERIFY(w != nullptr);
    QCOMPARE(w->objectName(), id);
}

void TestComponentRegistry::testLoadEmptyDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ComponentRegistry reg(nullptr, nullptr, this);
    QCOMPARE(reg.loadComponentsFromDirectory(dir.path()), 0);
}

void TestComponentRegistry::testLoadNonexistentDirectory()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    QCOMPARE(reg.loadComponentsFromDirectory(QStringLiteral("/nonexistent/path/xyz")), 0);
}

void TestComponentRegistry::testSignals()
{
    ComponentRegistry reg(nullptr, nullptr, this);
    auto fac = std::make_unique<MockComponentFactory>(QStringLiteral("test.signals"), true, this);

    QSignalSpy regSpy(&reg, &ComponentRegistry::componentRegistered);
    QSignalSpy createSpy(&reg, &ComponentRegistry::instanceCreated);
    QSignalSpy destroySpy(&reg, &ComponentRegistry::instanceDestroyed);

    reg.registerFactory(fac.get());
    QCOMPARE(regSpy.count(), 1);
    QCOMPARE(regSpy.at(0).at(0).toString(), QStringLiteral("test.signals"));

    QString id = reg.createInstance(QStringLiteral("test.signals"));
    QCOMPARE(createSpy.count(), 1);
    QCOMPARE(createSpy.at(0).at(0).toString(), id);
    QCOMPARE(createSpy.at(0).at(1).toString(), QStringLiteral("test.signals"));

    reg.destroyInstance(id);
    QCOMPARE(destroySpy.count(), 1);
    QCOMPARE(destroySpy.at(0).at(0).toString(), id);
    QCOMPARE(destroySpy.at(0).at(1).toString(), QStringLiteral("test.signals"));
}

QTEST_MAIN(TestComponentRegistry)
#include "test_componentregistry.moc"
