#pragma once

#include "sdk/itestrunner.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QProcess>

// ── TestDiscoveryService ──────────────────────────────────────────────────────

/// Discovers and runs CTest tests from a build directory.
class TestDiscoveryService : public QObject
{
    Q_OBJECT

public:
    explicit TestDiscoveryService(QObject *parent = nullptr);

    /// Set the build directory (must contain CTestTestfile.cmake).
    void setBuildDirectory(const QString &dir);
    QString buildDirectory() const { return m_buildDir; }

    /// Discover tests by running `ctest --test-dir <dir> --show-only=json-v1`.
    void discoverTests();

    /// Run all discovered tests.
    void runAllTests();

    /// Run a specific test by index.
    void runTest(int index);

    /// Current list of discovered tests.
    const QList<TestItem> &tests() const { return m_tests; }

    /// Whether discovery or test execution is running.
    bool isBusy() const { return m_busy; }

signals:
    void discoveryFinished();
    void testStarted(int index);
    void testFinished(int index);
    void allTestsFinished();

private:
    void parseDiscoveryOutput(const QByteArray &json);
    void runNextTest();

    QString m_buildDir;
    QList<TestItem> m_tests;
    bool m_busy = false;
    int  m_runIndex = -1;  // current test being run in queue
    QList<int> m_runQueue; // indices to run
};
