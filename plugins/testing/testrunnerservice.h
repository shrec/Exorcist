#pragma once

#include "sdk/itestrunner.h"

class TestDiscoveryService;

// ── TestRunnerService ────────────────────────────────────────────────────────
//
// Adapts TestDiscoveryService to the ITestRunner SDK interface.
// Registered in ServiceRegistry as "testRunner" so agent tools and plugins
// can discover and run tests without coupling to TestDiscoveryService.

class TestRunnerService : public ITestRunner
{
    Q_OBJECT

public:
    explicit TestRunnerService(TestDiscoveryService *discovery, QObject *parent = nullptr);

    void discoverTests() override;
    void runAllTests() override;
    void runTest(int index) override;
    QList<TestItem> tests() const override;
    bool hasTests() const override;

private:
    TestDiscoveryService *m_discovery;
};
