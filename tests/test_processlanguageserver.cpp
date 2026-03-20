#include <QSignalSpy>
#include <QTest>
#include <QUuid>

#include "lsp/processlanguageserver.h"
#include "sdk/ilspservice.h"

class TestProcessLanguageServer : public QObject
{
    Q_OBJECT

private slots:
    void exposesClientAndService();
    void failedStartReportsStatus();
    void serviceDelegatesStart();
};

void TestProcessLanguageServer::exposesClientAndService()
{
    ProcessLanguageServer server({
        QStringLiteral("nonexistent-lsp"),
        QStringLiteral("Test language server"),
        {},
    });

    QVERIFY(server.client() != nullptr);
    QVERIFY(server.service() != nullptr);
}

void TestProcessLanguageServer::failedStartReportsStatus()
{
    const QString program = QStringLiteral("exorcist-missing-lsp-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ProcessLanguageServer server({
        program,
        QStringLiteral("Missing LSP"),
        {},
    });

    QSignalSpy statusSpy(server.service(), SIGNAL(statusMessage(QString,int)));
    QSignalSpy stoppedSpy(&server, SIGNAL(serverStopped()));
    QSignalSpy readySpy(&server, SIGNAL(serverReady()));

    server.start(QString());

    QVERIFY(QTest::qWaitFor([&]() { return statusSpy.count() > 0; }, 3000));
    QCOMPARE(readySpy.count(), 0);
    QVERIFY(stoppedSpy.count() > 0);

    const QList<QVariant> first = statusSpy.first();
    QVERIFY(first.at(0).toString().contains(QStringLiteral("Missing LSP")));
}

void TestProcessLanguageServer::serviceDelegatesStart()
{
    const QString program = QStringLiteral("exorcist-missing-lsp-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    ProcessLanguageServer server({
        program,
        QStringLiteral("Delegated LSP"),
        {},
    });

    auto *service = server.service();
    QVERIFY(service != nullptr);

    QSignalSpy statusSpy(service, SIGNAL(statusMessage(QString,int)));
    QSignalSpy stoppedSpy(&server, SIGNAL(serverStopped()));

    service->startServer(QStringLiteral("delegated-root"));

    QVERIFY(QTest::qWaitFor([&]() { return statusSpy.count() > 0; }, 3000));
    QVERIFY(stoppedSpy.count() > 0);
    QCOMPARE(server.workspaceRoot(), QStringLiteral("delegated-root"));
}

QTEST_MAIN(TestProcessLanguageServer)
#include "test_processlanguageserver.moc"