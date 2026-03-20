#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// ── Test Runner Service ──────────────────────────────────────────────────────
//
// Stable SDK interface for discovering and running tests.
// Plugins and agent tools should use this instead of directly accessing
// TestDiscoveryService or any concrete test runner implementation.
//
// Registered in ServiceRegistry as "testRunner".

struct TestItem
{
    QString name;
    int     index = -1;
    QString command;
    QStringList args;
    QString workingDirectory;
    enum Status { Unknown, Running, Passed, Failed, Skipped };
    Status  status = Unknown;
    QString output;
    double  duration = 0.0;
};

class ITestRunner : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Discover tests in the workspace.
    virtual void discoverTests() = 0;

    /// Run all discovered tests.
    virtual void runAllTests() = 0;

    /// Run a specific test by index.
    virtual void runTest(int index) = 0;

    /// Get the list of discovered tests.
    virtual QList<TestItem> tests() const = 0;

    /// Whether test discovery has been performed.
    virtual bool hasTests() const = 0;

signals:
    /// Emitted when test discovery completes.
    void discoveryFinished();

    /// Emitted when a test starts running.
    void testStarted(int index);

    /// Emitted when a test finishes.
    void testFinished(int index);

    /// Emitted when all tests have finished.
    void allTestsFinished();
};
