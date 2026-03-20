#include <QTest>

#include "bootstrap/templateservicesbootstrap.h"
#include "project/iprojecttemplateprovider.h"
#include "project/projecttemplateregistry.h"
#include "serviceregistry.h"

class TestTemplateServicesBootstrap : public QObject
{
    Q_OBJECT

private slots:
    void registersProjectTemplateRegistry();
    void registersBuiltinTemplates();
    void initializeIsIdempotent();
    void accessorMatchesRegisteredService();
    void registryExposesLanguages();
};

void TestTemplateServicesBootstrap::registersProjectTemplateRegistry()
{
    ServiceRegistry services;
    TemplateServicesBootstrap bootstrap;

    bootstrap.initialize(&services);

    QVERIFY(services.hasService(QStringLiteral("projectTemplateRegistry")));
}

void TestTemplateServicesBootstrap::registersBuiltinTemplates()
{
    ServiceRegistry services;
    TemplateServicesBootstrap bootstrap;

    bootstrap.initialize(&services);

    auto *registry = services.service<ProjectTemplateRegistry>(
        QStringLiteral("projectTemplateRegistry"));
    QVERIFY(registry != nullptr);
    QVERIFY(!registry->allTemplates().isEmpty());
}

void TestTemplateServicesBootstrap::initializeIsIdempotent()
{
    ServiceRegistry services;
    TemplateServicesBootstrap bootstrap;

    bootstrap.initialize(&services);
    auto *first = services.service<ProjectTemplateRegistry>(
        QStringLiteral("projectTemplateRegistry"));
    bootstrap.initialize(&services);
    auto *second = services.service<ProjectTemplateRegistry>(
        QStringLiteral("projectTemplateRegistry"));

    QCOMPARE(first, second);
}

void TestTemplateServicesBootstrap::accessorMatchesRegisteredService()
{
    ServiceRegistry services;
    TemplateServicesBootstrap bootstrap;

    bootstrap.initialize(&services);

    QCOMPARE(bootstrap.projectTemplateRegistry(),
             services.service<ProjectTemplateRegistry>(
                 QStringLiteral("projectTemplateRegistry")));
}

void TestTemplateServicesBootstrap::registryExposesLanguages()
{
    ServiceRegistry services;
    TemplateServicesBootstrap bootstrap;

    bootstrap.initialize(&services);

    auto *registry = services.service<ProjectTemplateRegistry>(
        QStringLiteral("projectTemplateRegistry"));
    QVERIFY(registry != nullptr);
    QVERIFY(registry->languages().contains(QStringLiteral("C++")));
    QVERIFY(registry->languages().contains(QStringLiteral("Python")));
}

QTEST_MAIN(TestTemplateServicesBootstrap)
#include "test_templateservicesbootstrap.moc"