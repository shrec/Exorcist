#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>

#include "lsp/socketlsptransport.h"
#include "lsp/lspmessage.h"
#include "remote/sshsession.h"
#include "remote/sshprofile.h"

class TestRemoteLsp : public QObject
{
    Q_OBJECT

private slots:
    // ── SocketLspTransport ───────────────────────────────────────────────
    void transport_constructsWithoutCrash();
    void transport_notConnectedInitially();
    void transport_connectsToServer();
    void transport_sendsLspMessage();
    void transport_receivesLspMessage();
    void transport_handlesMultipleMessages();
    void transport_errorOnSendWhileDisconnected();
    void transport_handlesPartialMessage();
    void transport_stopDisconnects();

    // ── SshSession port forwarding API ───────────────────────────────────
    void ssh_portForwardNeedsConnection();
    void ssh_stopPortForwardNoOp();

private:
    // Helper: start a local TCP server for testing
    struct TestServer {
        std::unique_ptr<QTcpServer> server;
        int port = 0;

        bool start()
        {
            server = std::make_unique<QTcpServer>();
            if (!server->listen(QHostAddress::LocalHost, 0))
                return false;
            port = server->serverPort();
            return true;
        }
    };

    static QByteArray makeLspFrame(const QJsonObject &payload)
    {
        const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
        return "Content-Length: " + QByteArray::number(json.size()) + "\r\n\r\n" + json;
    }
};

// ── SocketLspTransport tests ─────────────────────────────────────────────────

void TestRemoteLsp::transport_constructsWithoutCrash()
{
    SocketLspTransport transport;
    QVERIFY(!transport.isConnected());
}

void TestRemoteLsp::transport_notConnectedInitially()
{
    SocketLspTransport transport;
    QVERIFY(!transport.isConnected());
}

void TestRemoteLsp::transport_connectsToServer()
{
    TestServer srv;
    QVERIFY(srv.start());

    SocketLspTransport transport;
    transport.connectToServer(QStringLiteral("127.0.0.1"), srv.port);

    // Wait for connection
    QVERIFY(QTest::qWaitFor([&]() { return transport.isConnected(); }, 3000));
    QVERIFY(transport.isConnected());
}

void TestRemoteLsp::transport_sendsLspMessage()
{
    TestServer srv;
    QVERIFY(srv.start());

    SocketLspTransport transport;
    transport.connectToServer(QStringLiteral("127.0.0.1"), srv.port);
    QVERIFY(QTest::qWaitFor([&]() { return transport.isConnected(); }, 3000));

    // Accept connection on server side
    QVERIFY(QTest::qWaitFor([&]() { return srv.server->hasPendingConnections(); }, 1000));
    auto *serverSocket = srv.server->nextPendingConnection();
    QVERIFY(serverSocket);

    // Send a message
    LspMessage msg;
    msg.payload = QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                              {QStringLiteral("method"), QStringLiteral("initialize")},
                              {QStringLiteral("id"), 1}};
    transport.send(msg);

    // Read on server side
    QVERIFY(QTest::qWaitFor([&]() { return serverSocket->bytesAvailable() > 0; }, 2000));
    const QByteArray received = serverSocket->readAll();
    QVERIFY(received.contains("Content-Length:"));
    QVERIFY(received.contains("initialize"));
}

void TestRemoteLsp::transport_receivesLspMessage()
{
    TestServer srv;
    QVERIFY(srv.start());

    SocketLspTransport transport;
    QSignalSpy spy(&transport, &SocketLspTransport::messageReceived);
    transport.connectToServer(QStringLiteral("127.0.0.1"), srv.port);
    QVERIFY(QTest::qWaitFor([&]() { return transport.isConnected(); }, 3000));

    QVERIFY(QTest::qWaitFor([&]() { return srv.server->hasPendingConnections(); }, 1000));
    auto *serverSocket = srv.server->nextPendingConnection();
    QVERIFY(serverSocket);

    // Server sends an LSP message
    QJsonObject response{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                         {QStringLiteral("id"), 1},
                         {QStringLiteral("result"), QJsonObject{{QStringLiteral("capabilities"), QJsonObject()}}}};
    serverSocket->write(makeLspFrame(response));
    serverSocket->flush();

    // Transport should emit messageReceived
    QVERIFY(QTest::qWaitFor([&]() { return spy.count() > 0; }, 3000));
    QCOMPARE(spy.count(), 1);

    const auto receivedMsg = spy.first().first().value<LspMessage>();
    QCOMPARE(receivedMsg.payload.value(QStringLiteral("id")).toInt(), 1);
}

void TestRemoteLsp::transport_handlesMultipleMessages()
{
    TestServer srv;
    QVERIFY(srv.start());

    SocketLspTransport transport;
    QSignalSpy spy(&transport, &SocketLspTransport::messageReceived);
    transport.connectToServer(QStringLiteral("127.0.0.1"), srv.port);
    QVERIFY(QTest::qWaitFor([&]() { return transport.isConnected(); }, 3000));

    QVERIFY(QTest::qWaitFor([&]() { return srv.server->hasPendingConnections(); }, 1000));
    auto *serverSocket = srv.server->nextPendingConnection();

    // Send 3 messages back-to-back
    for (int i = 1; i <= 3; ++i) {
        QJsonObject msg{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                        {QStringLiteral("id"), i}};
        serverSocket->write(makeLspFrame(msg));
    }
    serverSocket->flush();

    QVERIFY(QTest::qWaitFor([&]() { return spy.count() >= 3; }, 3000));
    QCOMPARE(spy.count(), 3);
}

void TestRemoteLsp::transport_errorOnSendWhileDisconnected()
{
    SocketLspTransport transport;
    QSignalSpy spy(&transport, &SocketLspTransport::transportError);

    LspMessage msg;
    msg.payload = QJsonObject{{QStringLiteral("test"), true}};
    transport.send(msg);

    QCOMPARE(spy.count(), 1);
}

void TestRemoteLsp::transport_handlesPartialMessage()
{
    TestServer srv;
    QVERIFY(srv.start());

    SocketLspTransport transport;
    QSignalSpy spy(&transport, &SocketLspTransport::messageReceived);
    transport.connectToServer(QStringLiteral("127.0.0.1"), srv.port);
    QVERIFY(QTest::qWaitFor([&]() { return transport.isConnected(); }, 3000));

    QVERIFY(QTest::qWaitFor([&]() { return srv.server->hasPendingConnections(); }, 1000));
    auto *serverSocket = srv.server->nextPendingConnection();

    QJsonObject msg{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                    {QStringLiteral("id"), 42}};
    QByteArray frame = makeLspFrame(msg);

    // Send first half
    int half = frame.size() / 2;
    serverSocket->write(frame.left(half));
    serverSocket->flush();

    QTest::qWait(200);
    QCOMPARE(spy.count(), 0); // Not enough data yet

    // Send second half
    serverSocket->write(frame.mid(half));
    serverSocket->flush();

    QVERIFY(QTest::qWaitFor([&]() { return spy.count() > 0; }, 3000));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().value<LspMessage>().payload.value(QStringLiteral("id")).toInt(), 42);
}

void TestRemoteLsp::transport_stopDisconnects()
{
    TestServer srv;
    QVERIFY(srv.start());

    SocketLspTransport transport;
    transport.connectToServer(QStringLiteral("127.0.0.1"), srv.port);
    QVERIFY(QTest::qWaitFor([&]() { return transport.isConnected(); }, 3000));

    transport.stop();
    QTest::qWait(100);
    QVERIFY(!transport.isConnected());
}

// ── SshSession port forwarding ───────────────────────────────────────────────

void TestRemoteLsp::ssh_portForwardNeedsConnection()
{
    SshProfile profile;
    profile.host = QStringLiteral("nonexistent.invalid");
    auto session = std::make_unique<SshSession>(profile);
    // Not connected — should return -1
    int id = session->startLocalPortForward(12345, QStringLiteral("127.0.0.1"), 12345);
    QCOMPARE(id, -1);
}

void TestRemoteLsp::ssh_stopPortForwardNoOp()
{
    SshProfile profile;
    auto session = std::make_unique<SshSession>(profile);
    // Stopping a non-existent forward should not crash
    session->stopPortForward(999);
}

QTEST_MAIN(TestRemoteLsp)
#include "test_remotelsp.moc"
