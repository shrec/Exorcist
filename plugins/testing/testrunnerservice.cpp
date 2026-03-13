#include "testrunnerservice.h"
#include "testdiscoveryservice.h"

TestRunnerService::TestRunnerService(TestDiscoveryService *discovery, QObject *parent)
    : ITestRunner(parent)
    , m_discovery(discovery)
{
    connect(m_discovery, &TestDiscoveryService::discoveryFinished,
            this,        &ITestRunner::discoveryFinished);
    connect(m_discovery, &TestDiscoveryService::testStarted,
            this,        &ITestRunner::testStarted);
    connect(m_discovery, &TestDiscoveryService::testFinished,
            this,        &ITestRunner::testFinished);
    connect(m_discovery, &TestDiscoveryService::allTestsFinished,
            this,        &ITestRunner::allTestsFinished);
}

void TestRunnerService::discoverTests()
{
    m_discovery->discoverTests();
}

void TestRunnerService::runAllTests()
{
    m_discovery->runAllTests();
}

void TestRunnerService::runTest(int index)
{
    m_discovery->runTest(index);
}

QList<TestItem> TestRunnerService::tests() const
{
    return m_discovery->tests();
}

bool TestRunnerService::hasTests() const
{
    return !m_discovery->tests().isEmpty();
}
