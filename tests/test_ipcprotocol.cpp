#include <QTest>
#include <QJsonArray>

#include "process/ipcprotocol.h"

class TestIpcProtocol : public QObject
{
    Q_OBJECT

private slots:
    // ── Message creation ──────────────────────────────────────────────────
    void testRequestCreation();
    void testNotificationCreation();
    void testResponseCreation();
    void testErrorResponseCreation();

    // ── Serialization round-trip ──────────────────────────────────────────
    void testSerializeRequest();
    void testSerializeNotification();
    void testSerializeResponse();
    void testSerializeErrorResponse();
    void testRoundTripWithParams();

    // ── Framing ───────────────────────────────────────────────────────────
    void testFrameUnframe();
    void testFrameMultipleMessages();
    void testUnframePartialBuffer();
    void testUnframeEmptyBuffer();
    void testUnframeIncompleteHeader();

    // ── Edge cases ────────────────────────────────────────────────────────
    void testDeserializeInvalidJson();
    void testDeserializeEmptyData();
    void testBridgePipeNameNotEmpty();
};

// ── Message creation ──────────────────────────────────────────────────────

void TestIpcProtocol::testRequestCreation()
{
    auto msg = Ipc::Message::request(1, QStringLiteral("test/method"),
                                     {{QStringLiteral("key"), QStringLiteral("value")}});
    QCOMPARE(msg.kind, Ipc::MessageKind::Request);
    QCOMPARE(msg.id, 1);
    QCOMPARE(msg.method, QStringLiteral("test/method"));
    QCOMPARE(msg.params[QStringLiteral("key")].toString(), QStringLiteral("value"));
    QVERIFY(!msg.isError);
}

void TestIpcProtocol::testNotificationCreation()
{
    auto msg = Ipc::Message::notification(QStringLiteral("notify/event"),
                                          {{QStringLiteral("data"), 42}});
    QCOMPARE(msg.kind, Ipc::MessageKind::Notification);
    QCOMPARE(msg.id, -1);
    QCOMPARE(msg.method, QStringLiteral("notify/event"));
    QCOMPARE(msg.params[QStringLiteral("data")].toInt(), 42);
}

void TestIpcProtocol::testResponseCreation()
{
    auto msg = Ipc::Message::response(5, {{QStringLiteral("status"), QStringLiteral("ok")}});
    QCOMPARE(msg.kind, Ipc::MessageKind::Response);
    QCOMPARE(msg.id, 5);
    QVERIFY(!msg.isError);
    QCOMPARE(msg.result[QStringLiteral("status")].toString(), QStringLiteral("ok"));
}

void TestIpcProtocol::testErrorResponseCreation()
{
    auto msg = Ipc::Message::errorResponse(3, Ipc::ErrorCode::MethodNotFound,
                                           QStringLiteral("not found"));
    QCOMPARE(msg.kind, Ipc::MessageKind::Response);
    QCOMPARE(msg.id, 3);
    QVERIFY(msg.isError);
    QCOMPARE(msg.error[QStringLiteral("code")].toInt(), Ipc::ErrorCode::MethodNotFound);
    QCOMPARE(msg.error[QStringLiteral("message")].toString(), QStringLiteral("not found"));
}

// ── Serialization round-trip ──────────────────────────────────────────────

void TestIpcProtocol::testSerializeRequest()
{
    auto orig = Ipc::Message::request(10, QStringLiteral("service/call"),
                                      {{QStringLiteral("svc"), QStringLiteral("foo")}});
    QByteArray data = orig.serialize();
    QVERIFY(!data.isEmpty());

    auto restored = Ipc::Message::deserialize(data);
    QCOMPARE(restored.kind, Ipc::MessageKind::Request);
    QCOMPARE(restored.id, 10);
    QCOMPARE(restored.method, QStringLiteral("service/call"));
    QCOMPARE(restored.params[QStringLiteral("svc")].toString(), QStringLiteral("foo"));
}

void TestIpcProtocol::testSerializeNotification()
{
    auto orig = Ipc::Message::notification(QStringLiteral("server/shuttingDown"));
    auto restored = Ipc::Message::deserialize(orig.serialize());
    QCOMPARE(restored.kind, Ipc::MessageKind::Notification);
    QCOMPARE(restored.method, QStringLiteral("server/shuttingDown"));
    QCOMPARE(restored.id, -1);
}

void TestIpcProtocol::testSerializeResponse()
{
    auto orig = Ipc::Message::response(7, {{QStringLiteral("count"), 3}});
    auto restored = Ipc::Message::deserialize(orig.serialize());
    QCOMPARE(restored.kind, Ipc::MessageKind::Response);
    QCOMPARE(restored.id, 7);
    QVERIFY(!restored.isError);
    QCOMPARE(restored.result[QStringLiteral("count")].toInt(), 3);
}

void TestIpcProtocol::testSerializeErrorResponse()
{
    auto orig = Ipc::Message::errorResponse(2, Ipc::ErrorCode::InternalError,
                                            QStringLiteral("boom"));
    auto restored = Ipc::Message::deserialize(orig.serialize());
    QCOMPARE(restored.kind, Ipc::MessageKind::Response);
    QCOMPARE(restored.id, 2);
    QVERIFY(restored.isError);
    QCOMPARE(restored.error[QStringLiteral("code")].toInt(), Ipc::ErrorCode::InternalError);
    QCOMPARE(restored.error[QStringLiteral("message")].toString(), QStringLiteral("boom"));
}

void TestIpcProtocol::testRoundTripWithParams()
{
    QJsonObject nested;
    nested[QStringLiteral("inner")] = QStringLiteral("value");
    QJsonObject params;
    params[QStringLiteral("nested")] = nested;
    params[QStringLiteral("number")] = 42;
    params[QStringLiteral("flag")] = true;

    auto orig = Ipc::Message::request(99, QStringLiteral("complex/call"), params);
    auto restored = Ipc::Message::deserialize(orig.serialize());
    QCOMPARE(restored.params[QStringLiteral("number")].toInt(), 42);
    QCOMPARE(restored.params[QStringLiteral("flag")].toBool(), true);
    QCOMPARE(restored.params[QStringLiteral("nested")].toObject()[QStringLiteral("inner")].toString(),
             QStringLiteral("value"));
}

// ── Framing ───────────────────────────────────────────────────────────────

void TestIpcProtocol::testFrameUnframe()
{
    QByteArray payload = QByteArrayLiteral("{\"test\":1}");
    QByteArray framed = Ipc::frame(payload);

    // Frame should be 4 bytes header + payload
    QCOMPARE(framed.size(), 4 + payload.size());

    QByteArray buffer = framed;
    QByteArray out;
    QVERIFY(Ipc::tryUnframe(buffer, out));
    QCOMPARE(out, payload);
    QVERIFY(buffer.isEmpty()); // buffer consumed
}

void TestIpcProtocol::testFrameMultipleMessages()
{
    QByteArray p1 = QByteArrayLiteral("{\"a\":1}");
    QByteArray p2 = QByteArrayLiteral("{\"b\":2}");

    QByteArray buffer = Ipc::frame(p1) + Ipc::frame(p2);

    QByteArray out;
    QVERIFY(Ipc::tryUnframe(buffer, out));
    QCOMPARE(out, p1);

    // Second message still in buffer
    QVERIFY(!buffer.isEmpty());
    QVERIFY(Ipc::tryUnframe(buffer, out));
    QCOMPARE(out, p2);
    QVERIFY(buffer.isEmpty());
}

void TestIpcProtocol::testUnframePartialBuffer()
{
    QByteArray payload = QByteArrayLiteral("{\"x\":123}");
    QByteArray framed = Ipc::frame(payload);

    // Only give half the data — should fail gracefully
    QByteArray partial = framed.left(framed.size() / 2);
    QByteArray out;
    QVERIFY(!Ipc::tryUnframe(partial, out));
    // partial should be unchanged
    QCOMPARE(partial.size(), framed.size() / 2);
}

void TestIpcProtocol::testUnframeEmptyBuffer()
{
    QByteArray buffer;
    QByteArray out;
    QVERIFY(!Ipc::tryUnframe(buffer, out));
}

void TestIpcProtocol::testUnframeIncompleteHeader()
{
    QByteArray buffer(3, '\0'); // only 3 bytes, need 4 for header
    QByteArray out;
    QVERIFY(!Ipc::tryUnframe(buffer, out));
}

// ── Edge cases ────────────────────────────────────────────────────────────

void TestIpcProtocol::testDeserializeInvalidJson()
{
    auto msg = Ipc::Message::deserialize(QByteArrayLiteral("not valid json"));
    // Should return a default/empty message without crashing
    QVERIFY(msg.method.isEmpty());
}

void TestIpcProtocol::testDeserializeEmptyData()
{
    auto msg = Ipc::Message::deserialize(QByteArray());
    QVERIFY(msg.method.isEmpty());
}

void TestIpcProtocol::testBridgePipeNameNotEmpty()
{
    QString name = Ipc::bridgePipeName();
    QVERIFY(!name.isEmpty());
    QVERIFY(name.contains(QStringLiteral("exobridge")));
}

QTEST_MAIN(TestIpcProtocol)
#include "test_ipcprotocol.moc"
