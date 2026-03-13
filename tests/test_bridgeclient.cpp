#include <QTest>
#include <QSignalSpy>
#include <QUuid>
#include <QLocalServer>

#include "process/bridgeclient.h"
#include "process/exobridgecore.h"

// ── TestBridgeClient ─────────────────────────────────────────────────────────
//
// Tests for BridgeClient crash detection and graceful shutdown signalling.
// Each test uses a unique pipe name to avoid daemon/test conflicts.

class TestBridgeClient : public QObject
{
    Q_OBJECT

    static QString uniquePipeName()
    {
        return QStringLiteral("exobridge-test-")
               + QUuid::createUuid().toString(QUuid::Id128).left(8);
    }

private slots:

    // ── Crash detection tests ────────────────────────────────────────

    void testCrashDetectedOnUnexpectedDisconnect()
    {
        // When the server disappears without sending ServerShutdown,
        // bridgeCrashed() should fire.
        const auto pipe = uniquePipeName();

        // Use a raw QLocalServer — no protocol, just accept and kill
        auto server = std::make_unique<QLocalServer>();
        QVERIFY(server->listen(pipe));

        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy connectedSpy(&client, &BridgeClient::connected);
        QSignalSpy crashedSpy(&client, &BridgeClient::bridgeCrashed);
        QSignalSpy disconnectedSpy(&client, &BridgeClient::disconnected);

        client.connectToServer(pipe);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);
        QVERIFY(client.isConnected());

        // Kill the server abruptly — no graceful shutdown notification
        server.reset();

        QTRY_COMPARE_WITH_TIMEOUT(crashedSpy.count(), 1, 3000);
        QTRY_COMPARE_WITH_TIMEOUT(disconnectedSpy.count(), 1, 3000);
        QVERIFY(!client.isConnected());
    }

    void testNoCrashOnGracefulShutdown()
    {
        // When the server sends ServerShutdown notification before closing,
        // bridgeCrashed() should NOT fire.
        const auto pipe = uniquePipeName();
        auto server = std::make_unique<QLocalServer>();
        QVERIFY(server->listen(pipe));

        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy connectedSpy(&client, &BridgeClient::connected);
        QSignalSpy crashedSpy(&client, &BridgeClient::bridgeCrashed);
        QSignalSpy disconnectedSpy(&client, &BridgeClient::disconnected);
        QSignalSpy shuttingDownSpy(&client, &BridgeClient::serverShuttingDown);

        // Accept connection on server side
        QLocalSocket *serverSock = nullptr;
        connect(server.get(), &QLocalServer::newConnection, this, [&]() {
            serverSock = server->nextPendingConnection();
        });

        client.connectToServer(pipe);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(serverSock != nullptr, 3000);

        // Manually send ServerShutdown notification via the server socket
        auto shutdownMsg = Ipc::Message::notification(
            QLatin1String(Ipc::Method::ServerShutdown));
        serverSock->write(Ipc::frame(shutdownMsg.serialize()));
        serverSock->flush();

        // Wait for client to process the notification
        QTRY_COMPARE_WITH_TIMEOUT(shuttingDownSpy.count(), 1, 3000);

        // Now kill the server — should NOT be detected as crash
        server.reset();
        QTRY_COMPARE_WITH_TIMEOUT(disconnectedSpy.count(), 1, 3000);
        QCOMPARE(crashedSpy.count(), 0);
    }

    void testGracefulFlagResetsOnReconnect()
    {
        // After a graceful shutdown and reconnect, a new unexpected
        // disconnect should be detected as a crash again.
        const auto pipe1 = uniquePipeName();
        auto server1 = std::make_unique<QLocalServer>();
        QVERIFY(server1->listen(pipe1));

        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy connectedSpy(&client, &BridgeClient::connected);
        QSignalSpy crashedSpy(&client, &BridgeClient::bridgeCrashed);
        QSignalSpy shuttingDownSpy(&client, &BridgeClient::serverShuttingDown);

        // Accept on server side
        QLocalSocket *serverSock1 = nullptr;
        connect(server1.get(), &QLocalServer::newConnection, this, [&]() {
            serverSock1 = server1->nextPendingConnection();
        });

        // First connection — graceful shutdown
        client.connectToServer(pipe1);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(serverSock1 != nullptr, 3000);

        // Send shutdown notification
        auto shutdownMsg = Ipc::Message::notification(
            QLatin1String(Ipc::Method::ServerShutdown));
        serverSock1->write(Ipc::frame(shutdownMsg.serialize()));
        serverSock1->flush();
        QTRY_COMPARE_WITH_TIMEOUT(shuttingDownSpy.count(), 1, 3000);

        server1.reset();
        QTRY_VERIFY_WITH_TIMEOUT(!client.isConnected(), 3000);
        QCOMPARE(crashedSpy.count(), 0);

        // Second connection — crash (no shutdown notification)
        const auto pipe2 = uniquePipeName();
        auto server2 = std::make_unique<QLocalServer>();
        QVERIFY(server2->listen(pipe2));

        client.connectToServer(pipe2);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 2, 3000);

        server2.reset();  // Kill abruptly
        QTRY_COMPARE_WITH_TIMEOUT(crashedSpy.count(), 1, 3000);
    }

    void testNoCrashWhenNeverConnected()
    {
        // Disconnect without ever reaching Connected state should not crash.
        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy crashedSpy(&client, &BridgeClient::bridgeCrashed);

        // Connect to a non-existent server
        client.connectToServer(uniquePipeName());

        QTest::qWait(500);
        QCOMPARE(crashedSpy.count(), 0);
    }

    void testCrashSignalEmittedBeforeDisconnected()
    {
        // bridgeCrashed() must be emitted before disconnected() so that
        // handlers can distinguish crash from normal disconnect.
        const auto pipe = uniquePipeName();
        auto server = std::make_unique<QLocalServer>();
        QVERIFY(server->listen(pipe));

        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy connectedSpy(&client, &BridgeClient::connected);

        QStringList signalOrder;
        connect(&client, &BridgeClient::bridgeCrashed, this, [&]() {
            signalOrder.append(QStringLiteral("crashed"));
        });
        connect(&client, &BridgeClient::disconnected, this, [&]() {
            signalOrder.append(QStringLiteral("disconnected"));
        });

        client.connectToServer(pipe);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);

        server.reset();
        QTRY_COMPARE_WITH_TIMEOUT(signalOrder.size(), 2, 3000);

        QCOMPARE(signalOrder.at(0), QStringLiteral("crashed"));
        QCOMPARE(signalOrder.at(1), QStringLiteral("disconnected"));
    }

    // ── Basic connectivity tests ─────────────────────────────────────

    void testConnectAndCallService()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();

        bool handlerCalled = false;
        core.installServiceHandler(
            QStringLiteral("testSvc"),
            [&](const QString &method, const QJsonObject &args,
                std::function<void(bool, QJsonObject)> respond) {
                handlerCalled = true;
                QJsonObject result;
                result[QLatin1String("echo")] = args.value(QLatin1String("msg"));
                respond(true, result);
            });

        QVERIFY(core.start(pipe));

        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy connectedSpy(&client, &BridgeClient::connected);
        client.connectToServer(pipe);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);

        bool gotResponse = false;
        QJsonObject callArgs;
        callArgs[QLatin1String("msg")] = QStringLiteral("hello");

        client.callService(
            QStringLiteral("testSvc"), QStringLiteral("echo"),
            callArgs,
            [&](bool ok, const QJsonObject &result) {
                QVERIFY(ok);
                QCOMPARE(result.value(QLatin1String("echo")).toString(),
                         QStringLiteral("hello"));
                gotResponse = true;
            });

        QTRY_VERIFY_WITH_TIMEOUT(gotResponse, 3000);
        QVERIFY(handlerCalled);

        client.disconnect();
        core.stop();
    }

    void testListServicesViaClient()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();
        core.installServiceHandler(
            QStringLiteral("alpha"),
            [](const QString &, const QJsonObject &,
               std::function<void(bool, QJsonObject)> respond) {
                respond(true, {});
            });

        QVERIFY(core.start(pipe));

        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy connectedSpy(&client, &BridgeClient::connected);
        client.connectToServer(pipe);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);

        bool gotList = false;
        client.listServices([&](bool ok, const QJsonObject &result) {
            QVERIFY(ok);
            const auto services = result.value(QLatin1String("services")).toArray();
            QStringList names;
            for (const auto &v : services)
                names << v.toString();
            QVERIFY(names.contains(QStringLiteral("alpha")));
            gotList = true;
        });

        QTRY_VERIFY_WITH_TIMEOUT(gotList, 3000);

        client.disconnect();
        core.stop();
    }

    void testServiceEvent()
    {
        const auto pipe = uniquePipeName();
        auto server = std::make_unique<QLocalServer>();
        QVERIFY(server->listen(pipe));

        BridgeClient client;
        client.setAutoReconnect(false);

        QSignalSpy connectedSpy(&client, &BridgeClient::connected);
        QSignalSpy eventSpy(&client, &BridgeClient::serviceEvent);

        QLocalSocket *serverSock = nullptr;
        connect(server.get(), &QLocalServer::newConnection, this, [&]() {
            serverSock = server->nextPendingConnection();
        });

        client.connectToServer(pipe);
        QTRY_COMPARE_WITH_TIMEOUT(connectedSpy.count(), 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(serverSock != nullptr, 3000);

        // Send a service event notification from the server
        QJsonObject params;
        params[QLatin1String("service")] = QStringLiteral("git");
        params[QLatin1String("event")]   = QStringLiteral("changed");
        auto msg = Ipc::Message::notification(
            QLatin1String(Ipc::Method::ServiceEvent), params);
        serverSock->write(Ipc::frame(msg.serialize()));
        serverSock->flush();

        QTRY_COMPARE_WITH_TIMEOUT(eventSpy.count(), 1, 3000);
        QCOMPARE(eventSpy.at(0).at(0).toString(), QStringLiteral("git"));
        QCOMPARE(eventSpy.at(0).at(1).toString(), QStringLiteral("changed"));

        client.disconnect();
        server.reset();
    }
};

QTEST_MAIN(TestBridgeClient)
#include "test_bridgeclient.moc"
