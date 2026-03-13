#include <QTest>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTimer>
#include <QUuid>

#include "process/ipcprotocol.h"
#include "process/exobridgecore.h"

// ── TestExoBridge ────────────────────────────────────────────────────────────
//
// Tests for the ExoBridge IPC protocol, message framing, and service dispatch.
//
// Each test uses a unique pipe name to avoid conflicts with a running
// ExoBridge daemon or leftover pipe instances from previous tests.
//
// NOTE: QLocalSocket::waitForReadyRead() is unreliable on Windows when server
// and client run in the same process. We use a polling helper with
// QTest::qWait() instead, which processes the Qt event loop properly.

class TestExoBridge : public QObject
{
    Q_OBJECT

    // Unique pipe name per test invocation to avoid conflicts with daemon
    static QString uniquePipeName()
    {
        return QStringLiteral("exobridge-test-")
               + QUuid::createUuid().toString(QUuid::Id128).left(8);
    }

    // Helper: poll for incoming data by processing events
    static bool waitForData(QLocalSocket &sock, int msec = 3000)
    {
        int elapsed = 0;
        while (sock.bytesAvailable() == 0 && elapsed < msec) {
            QTest::qWait(10);
            elapsed += 10;
        }
        return sock.bytesAvailable() > 0;
    }

    // Helper: send a request and wait for response
    static Ipc::Message sendAndReceive(QLocalSocket &sock, const Ipc::Message &req,
                                       int msec = 3000)
    {
        sock.write(Ipc::frame(req.serialize()));
        sock.flush();
        if (!waitForData(sock, msec))
            return {};
        QByteArray buf = sock.readAll();
        QByteArray payload;
        if (!Ipc::tryUnframe(buf, payload))
            return {};
        return Ipc::Message::deserialize(payload);
    }

private slots:

    // ── IPC Protocol tests ───────────────────────────────────────────
    void testFrameUnframe()
    {
        const QByteArray payload = R"({"test":"value"})";
        const QByteArray framed = Ipc::frame(payload);

        // Frame should be 4 bytes header + payload
        QCOMPARE(framed.size(), 4 + payload.size());

        // Verify big-endian length
        const auto *ptr = reinterpret_cast<const unsigned char *>(framed.constData());
        quint32 len = (static_cast<quint32>(ptr[0]) << 24)
                    | (static_cast<quint32>(ptr[1]) << 16)
                    | (static_cast<quint32>(ptr[2]) << 8)
                    | static_cast<quint32>(ptr[3]);
        QCOMPARE(len, static_cast<quint32>(payload.size()));

        // Unframe should recover the payload
        QByteArray buffer = framed;
        QByteArray out;
        QVERIFY(Ipc::tryUnframe(buffer, out));
        QCOMPARE(out, payload);
        QVERIFY(buffer.isEmpty());
    }

    void testUnframeIncomplete()
    {
        const QByteArray payload = R"({"hello":"world"})";
        const QByteArray framed = Ipc::frame(payload);

        // Only provide first 6 bytes (header + 2 bytes of payload)
        QByteArray partial = framed.left(6);
        QByteArray out;
        QVERIFY(!Ipc::tryUnframe(partial, out));
        QCOMPARE(partial.size(), 6);  // buffer unchanged
    }

    void testUnframeTooSmall()
    {
        QByteArray tiny = QByteArray(2, '\0');
        QByteArray out;
        QVERIFY(!Ipc::tryUnframe(tiny, out));
    }

    void testFrameUnframeMultiple()
    {
        const QByteArray p1 = R"({"msg":1})";
        const QByteArray p2 = R"({"msg":2})";

        QByteArray buffer = Ipc::frame(p1) + Ipc::frame(p2);

        QByteArray out1, out2;
        QVERIFY(Ipc::tryUnframe(buffer, out1));
        QCOMPARE(out1, p1);
        QVERIFY(Ipc::tryUnframe(buffer, out2));
        QCOMPARE(out2, p2);
        QVERIFY(buffer.isEmpty());
    }

    // ── Message serialization tests ──────────────────────────────────

    void testRequestSerialization()
    {
        QJsonObject params;
        params[QLatin1String("key")] = QStringLiteral("value");
        auto msg = Ipc::Message::request(42, QStringLiteral("test/method"), params);

        const QByteArray data = msg.serialize();
        auto decoded = Ipc::Message::deserialize(data);

        QCOMPARE(decoded.kind, Ipc::MessageKind::Request);
        QCOMPARE(decoded.id, 42);
        QCOMPARE(decoded.method, QStringLiteral("test/method"));
        QCOMPARE(decoded.params[QLatin1String("key")].toString(),
                 QStringLiteral("value"));
    }

    void testResponseSerialization()
    {
        QJsonObject result;
        result[QLatin1String("status")] = QStringLiteral("ok");
        auto msg = Ipc::Message::response(7, result);

        const QByteArray data = msg.serialize();
        auto decoded = Ipc::Message::deserialize(data);

        QCOMPARE(decoded.kind, Ipc::MessageKind::Response);
        QCOMPARE(decoded.id, 7);
        QVERIFY(!decoded.isError);
        QCOMPARE(decoded.result[QLatin1String("status")].toString(),
                 QStringLiteral("ok"));
    }

    void testErrorResponseSerialization()
    {
        auto msg = Ipc::Message::errorResponse(
            5, Ipc::ErrorCode::MethodNotFound, QStringLiteral("Not found"));

        const QByteArray data = msg.serialize();
        auto decoded = Ipc::Message::deserialize(data);

        QCOMPARE(decoded.kind, Ipc::MessageKind::Response);
        QCOMPARE(decoded.id, 5);
        QVERIFY(decoded.isError);
        QCOMPARE(decoded.error[QLatin1String("code")].toInt(),
                 Ipc::ErrorCode::MethodNotFound);
        QCOMPARE(decoded.error[QLatin1String("message")].toString(),
                 QStringLiteral("Not found"));
    }

    void testNotificationSerialization()
    {
        QJsonObject params;
        params[QLatin1String("event")] = QStringLiteral("test");
        auto msg = Ipc::Message::notification(QStringLiteral("notify/test"), params);

        const QByteArray data = msg.serialize();
        auto decoded = Ipc::Message::deserialize(data);

        QCOMPARE(decoded.kind, Ipc::MessageKind::Notification);
        QCOMPARE(decoded.method, QStringLiteral("notify/test"));
    }

    // ── ExoBridgeCore tests ──────────────────────────────────────────

    void testBridgePipeName()
    {
        const QString name = Ipc::bridgePipeName();
        QVERIFY(name.startsWith(QStringLiteral("exobridge-")));
        QVERIFY(name.length() > 10);
    }

    void testExoBridgeCoreStartStop()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();
        QVERIFY(core.start(pipe));
        QCOMPARE(core.clientCount(), 0);
        core.stop();
    }

    void testExoBridgeCoreHandshake()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();
        QVERIFY(core.start(pipe));

        QLocalSocket client;
        client.connectToServer(pipe);
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE_WITH_TIMEOUT(core.clientCount(), 1, 3000);

        // Send handshake
        QJsonObject params;
        params[QLatin1String("instanceId")] = QStringLiteral("test-id-1");
        params[QLatin1String("pid")] = static_cast<qint64>(12345);
        auto req = Ipc::Message::request(1, QLatin1String(Ipc::Method::Handshake), params);
        auto resp = sendAndReceive(client, req);

        QCOMPARE(resp.kind, Ipc::MessageKind::Response);
        QCOMPARE(resp.id, 1);
        QVERIFY(!resp.isError);
        QCOMPARE(resp.result[QLatin1String("serverVersion")].toString(),
                 QStringLiteral("1.0"));
        QCOMPARE(core.clientCount(), 1);

        client.disconnectFromServer();
        QTest::qWait(100);
        core.stop();
    }

    void testServiceHandlerDispatch()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();

        bool handlerCalled = false;
        QString receivedMethod;
        QJsonObject receivedArgs;

        core.installServiceHandler(
            QStringLiteral("testService"),
            [&](const QString &method, const QJsonObject &args,
                std::function<void(bool, QJsonObject)> respond) {
                handlerCalled = true;
                receivedMethod = method;
                receivedArgs = args;

                QJsonObject result;
                result[QLatin1String("answer")] = 42;
                respond(true, result);
            });

        QVERIFY(core.start(pipe));

        QLocalSocket client;
        client.connectToServer(pipe);
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE_WITH_TIMEOUT(core.clientCount(), 1, 3000);

        QJsonObject callParams;
        callParams[QLatin1String("service")] = QStringLiteral("testService");
        callParams[QLatin1String("method")] = QStringLiteral("doSomething");
        QJsonObject callArgs;
        callArgs[QLatin1String("input")] = QStringLiteral("hello");
        callParams[QLatin1String("args")] = callArgs;

        auto req = Ipc::Message::request(
            2, QLatin1String(Ipc::Method::CallService), callParams);
        auto resp = sendAndReceive(client, req);

        QTRY_VERIFY_WITH_TIMEOUT(handlerCalled, 3000);
        QCOMPARE(receivedMethod, QStringLiteral("doSomething"));
        QCOMPARE(receivedArgs[QLatin1String("input")].toString(),
                 QStringLiteral("hello"));
        QCOMPARE(resp.kind, Ipc::MessageKind::Response);
        QCOMPARE(resp.id, 2);
        QVERIFY(!resp.isError);
        QCOMPARE(resp.result[QLatin1String("answer")].toInt(), 42);

        client.disconnectFromServer();
        QTest::qWait(100);
        core.stop();
    }

    void testServiceNotFound()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();
        QVERIFY(core.start(pipe));

        QLocalSocket client;
        client.connectToServer(pipe);
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE_WITH_TIMEOUT(core.clientCount(), 1, 3000);

        QJsonObject callParams;
        callParams[QLatin1String("service")] = QStringLiteral("nonexistent");
        callParams[QLatin1String("method")] = QStringLiteral("test");
        callParams[QLatin1String("args")] = QJsonObject{};

        auto req = Ipc::Message::request(
            3, QLatin1String(Ipc::Method::CallService), callParams);
        auto resp = sendAndReceive(client, req);

        QCOMPARE(resp.kind, Ipc::MessageKind::Response);
        QVERIFY(resp.isError);
        QCOMPARE(resp.error[QLatin1String("code")].toInt(),
                 Ipc::ErrorCode::ServiceNotFound);

        client.disconnectFromServer();
        QTest::qWait(100);
        core.stop();
    }

    void testListServices()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();
        core.installServiceHandler(
            QStringLiteral("svc1"),
            [](const QString &, const QJsonObject &,
               std::function<void(bool, QJsonObject)> respond) {
                respond(true, {});
            });
        core.installServiceHandler(
            QStringLiteral("svc2"),
            [](const QString &, const QJsonObject &,
               std::function<void(bool, QJsonObject)> respond) {
                respond(true, {});
            });

        QVERIFY(core.start(pipe));

        QLocalSocket client;
        client.connectToServer(pipe);
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE_WITH_TIMEOUT(core.clientCount(), 1, 3000);

        auto req = Ipc::Message::request(
            4, QLatin1String(Ipc::Method::ListServices), {});
        auto resp = sendAndReceive(client, req);

        QVERIFY(!resp.isError);
        const auto services = resp.result[QLatin1String("services")].toArray();
        QVERIFY(services.size() >= 2);

        QStringList names;
        for (const auto &v : services)
            names << v.toString();
        QVERIFY(names.contains(QStringLiteral("svc1")));
        QVERIFY(names.contains(QStringLiteral("svc2")));

        client.disconnectFromServer();
        QTest::qWait(100);
        core.stop();
    }

    void testIdleTimeout()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();
        core.setPersistent(false);
        core.setIdleTimeout(1);  // 1 second

        QSignalSpy shutdownSpy(&core, &ExoBridgeCore::shutdownRequested);
        QVERIFY(core.start(pipe));

        // Connect and verify with a handshake round-trip
        QLocalSocket client;
        client.connectToServer(pipe);
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE_WITH_TIMEOUT(core.clientCount(), 1, 3000);

        QJsonObject params;
        params[QLatin1String("instanceId")] = QStringLiteral("idle-test");
        params[QLatin1String("pid")] = static_cast<qint64>(99999);
        auto req = Ipc::Message::request(
            1, QLatin1String(Ipc::Method::Handshake), params);
        auto resp = sendAndReceive(client, req);
        QVERIFY(!resp.isError);
        QCOMPARE(core.clientCount(), 1);

        // Disconnect — idle timer should fire after 1 second
        client.disconnectFromServer();
        QTest::qWait(100); // let server process disconnect

        QTRY_COMPARE_WITH_TIMEOUT(shutdownSpy.count(), 1, 5000);

        core.stop();
    }

    void testServiceHandlerError()
    {
        ExoBridgeCore core;
        const auto pipe = uniquePipeName();
        core.installServiceHandler(
            QStringLiteral("failService"),
            [](const QString &, const QJsonObject &,
               std::function<void(bool, QJsonObject)> respond) {
                QJsonObject err;
                err[QLatin1String("message")] = QStringLiteral("intentional failure");
                respond(false, err);
            });

        QVERIFY(core.start(pipe));

        QLocalSocket client;
        client.connectToServer(pipe);
        QVERIFY(client.waitForConnected(3000));
        QTRY_COMPARE_WITH_TIMEOUT(core.clientCount(), 1, 3000);

        QJsonObject callParams;
        callParams[QLatin1String("service")] = QStringLiteral("failService");
        callParams[QLatin1String("method")] = QStringLiteral("fail");
        callParams[QLatin1String("args")] = QJsonObject{};

        auto req = Ipc::Message::request(
            5, QLatin1String(Ipc::Method::CallService), callParams);
        auto resp = sendAndReceive(client, req);

        QVERIFY(resp.isError);

        client.disconnectFromServer();
        QTest::qWait(100);
        core.stop();
    }
};

QTEST_MAIN(TestExoBridge)
#include "test_exobridge.moc"
