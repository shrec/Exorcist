#include "testdiscoveryservice.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>

TestDiscoveryService::TestDiscoveryService(QObject *parent)
    : QObject(parent)
{
}

void TestDiscoveryService::setBuildDirectory(const QString &dir)
{
    m_buildDir = dir;
    m_tests.clear();
}

void TestDiscoveryService::discoverTests()
{
    if (m_buildDir.isEmpty() || m_busy)
        return;

    // Check that the build directory has CTest files
    if (!QFileInfo::exists(m_buildDir + QStringLiteral("/CTestTestfile.cmake")))
        return;

    m_busy = true;

    QProcess proc;
    proc.setWorkingDirectory(m_buildDir);
    proc.start(QStringLiteral("ctest"),
               {QStringLiteral("--test-dir"), m_buildDir,
                QStringLiteral("--show-only=json-v1")});

    if (!proc.waitForFinished(10000)) {
        m_busy = false;
        return;
    }

    parseDiscoveryOutput(proc.readAllStandardOutput());
    m_busy = false;
    emit discoveryFinished();
}

void TestDiscoveryService::parseDiscoveryOutput(const QByteArray &json)
{
    m_tests.clear();

    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return;

    const QJsonArray testsArr = doc.object().value(QStringLiteral("tests")).toArray();

    for (int arrIdx = 0; arrIdx < testsArr.size(); ++arrIdx) {
        const QJsonObject obj = testsArr[arrIdx].toObject();
        TestItem item;
        item.name  = obj.value(QStringLiteral("name")).toString();
        item.index = arrIdx + 1; // 1-based CTest index

        // Command is an array: [executable, arg1, arg2, ...]
        const QJsonArray cmdArr = obj.value(QStringLiteral("command")).toArray();
        if (!cmdArr.isEmpty()) {
            item.command = cmdArr.first().toString();
            for (int i = 1; i < cmdArr.size(); ++i)
                item.args.append(cmdArr[i].toString());
        }

        m_tests.append(item);
    }
}

void TestDiscoveryService::runAllTests()
{
    if (m_busy || m_tests.isEmpty())
        return;

    m_runQueue.clear();
    for (int i = 0; i < m_tests.size(); ++i)
        m_runQueue.append(i);

    // Reset all statuses
    for (auto &t : m_tests) {
        t.status   = TestItem::Unknown;
        t.output.clear();
        t.duration = 0.0;
    }

    m_busy = true;
    runNextTest();
}

void TestDiscoveryService::runTest(int index)
{
    if (m_busy || index < 0 || index >= m_tests.size())
        return;

    m_runQueue.clear();
    m_runQueue.append(index);

    m_tests[index].status = TestItem::Unknown;
    m_tests[index].output.clear();
    m_tests[index].duration = 0.0;

    m_busy = true;
    runNextTest();
}

void TestDiscoveryService::runNextTest()
{
    if (m_runQueue.isEmpty()) {
        m_busy = false;
        emit allTestsFinished();
        return;
    }

    const int idx = m_runQueue.takeFirst();
    m_runIndex = idx;

    auto &item = m_tests[idx];
    item.status = TestItem::Running;
    emit testStarted(idx);

    QProcess proc;
    proc.setWorkingDirectory(m_buildDir);
    proc.start(item.command, item.args);

    if (!proc.waitForFinished(120000)) {
        item.status = TestItem::Failed;
        item.output = tr("Test timed out after 120 seconds");
        emit testFinished(idx);
        runNextTest();
        return;
    }

    item.output = QString::fromUtf8(proc.readAllStandardOutput())
                + QString::fromUtf8(proc.readAllStandardError());
    item.status = (proc.exitCode() == 0) ? TestItem::Passed : TestItem::Failed;

    // Try to parse duration from Qt Test output (e.g. "Totals: ... 123ms")
    const QRegularExpression timeRe(QStringLiteral(R"((\d+(?:\.\d+)?)\s*(?:ms|sec))"));
    auto match = timeRe.match(item.output);
    if (match.hasMatch()) {
        double val = match.captured(1).toDouble();
        if (match.captured(0).contains(QStringLiteral("ms")))
            val /= 1000.0;
        item.duration = val;
    }

    emit testFinished(idx);
    runNextTest();
}
