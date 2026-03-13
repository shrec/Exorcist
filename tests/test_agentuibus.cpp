#include <QTest>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalSpy>

#include "agent/ui/agentuievent.h"
#include "agent/ui/iagentuirenderer.h"
#include "agent/ui/agentuibus.h"

// ── Mock renderer ────────────────────────────────────────────────────────────

class MockRenderer : public IAgentUIRenderer
{
public:
    QList<AgentUIEvent> received;
    bool active = true;
    int clearCount = 0;

    void handleEvent(const AgentUIEvent &event) override { received.append(event); }
    void clearDashboard() override { clearCount++; received.clear(); }
    bool isActive() const override { return active; }
};

// ── Test class ───────────────────────────────────────────────────────────────

class TestAgentUIBus : public QObject
{
    Q_OBJECT

private slots:
    void testEventCreation();
    void testBusDispatch();
    void testBusMultipleRenderers();
    void testMissionLifecycle();
    void testEventHistory();
    void testClearMission();
    void testTimestamp();
    void testStepConvenience();
    void testMetricConvenience();
    void testLogConvenience();
    void testArtifactConvenience();
    void testPanelConvenience();
    void testCustomEventConvenience();
    void testSignalEmitted();
};

// ── Event creation tests ─────────────────────────────────────────────────────

void TestAgentUIBus::testEventCreation()
{
    auto e = AgentUIEvent::missionCreated(QStringLiteral("m1"),
                                          QStringLiteral("Test Mission"),
                                          QStringLiteral("A test"));

    QCOMPARE(e.type, AgentUIEventType::MissionCreated);
    QCOMPARE(e.missionId, QStringLiteral("m1"));
    QCOMPARE(e.payload[QStringLiteral("title")].toString(), QStringLiteral("Test Mission"));
    QCOMPARE(e.payload[QStringLiteral("description")].toString(), QStringLiteral("A test"));
    QCOMPARE(e.payload[QStringLiteral("status")].toString(), QStringLiteral("created"));
}

// ── Bus dispatch tests ───────────────────────────────────────────────────────

void TestAgentUIBus::testBusDispatch()
{
    AgentUIBus bus;
    MockRenderer renderer;
    bus.addRenderer(&renderer);

    auto e = AgentUIEvent::missionCreated(QStringLiteral("m1"), QStringLiteral("Test"));
    bus.post(e);

    QCOMPARE(renderer.received.size(), 1);
    QCOMPARE(renderer.received[0].type, AgentUIEventType::MissionCreated);
    QCOMPARE(renderer.received[0].missionId, QStringLiteral("m1"));
}

void TestAgentUIBus::testBusMultipleRenderers()
{
    AgentUIBus bus;
    MockRenderer r1, r2;
    bus.addRenderer(&r1);
    bus.addRenderer(&r2);

    bus.post(AgentUIEvent::logAdded(QStringLiteral("m1"), AgentLogLevel::Info,
                                    QStringLiteral("hello")));

    QCOMPARE(r1.received.size(), 1);
    QCOMPARE(r2.received.size(), 1);

    // Remove one renderer
    bus.removeRenderer(&r1);
    bus.post(AgentUIEvent::logAdded(QStringLiteral("m1"), AgentLogLevel::Debug,
                                    QStringLiteral("world")));

    QCOMPARE(r1.received.size(), 1); // unchanged
    QCOMPARE(r2.received.size(), 2);
}

// ── Mission lifecycle ────────────────────────────────────────────────────────

void TestAgentUIBus::testMissionLifecycle()
{
    AgentUIBus bus;
    MockRenderer renderer;
    bus.addRenderer(&renderer);

    QVERIFY(bus.currentMissionId().isEmpty());

    bus.post(AgentUIEvent::missionCreated(QStringLiteral("m1"), QStringLiteral("Build")));
    QCOMPARE(bus.currentMissionId(), QStringLiteral("m1"));

    bus.post(AgentUIEvent::missionUpdated(QStringLiteral("m1"), AgentMissionStatus::Running));
    bus.post(AgentUIEvent::stepAdded(QStringLiteral("m1"), QStringLiteral("s1"),
                                     QStringLiteral("Read files")));
    bus.post(AgentUIEvent::stepUpdated(QStringLiteral("m1"), QStringLiteral("s1"),
                                       AgentStepStatus::Completed));
    bus.post(AgentUIEvent::missionCompleted(QStringLiteral("m1"), true, QStringLiteral("Done")));

    QCOMPARE(renderer.received.size(), 5);
}

// ── Event history ────────────────────────────────────────────────────────────

void TestAgentUIBus::testEventHistory()
{
    AgentUIBus bus;

    bus.post(AgentUIEvent::missionCreated(QStringLiteral("m1"), QStringLiteral("A")));
    bus.post(AgentUIEvent::logAdded(QStringLiteral("m1"), AgentLogLevel::Info,
                                    QStringLiteral("log1")));
    bus.post(AgentUIEvent::logAdded(QStringLiteral("m2"), AgentLogLevel::Info,
                                    QStringLiteral("log2")));

    auto history = bus.eventsForMission(QStringLiteral("m1"));
    QCOMPARE(history.size(), 2);

    auto history2 = bus.eventsForMission(QStringLiteral("m2"));
    QCOMPARE(history2.size(), 1);

    auto historyNone = bus.eventsForMission(QStringLiteral("nonexistent"));
    QCOMPARE(historyNone.size(), 0);
}

// ── Clear mission ────────────────────────────────────────────────────────────

void TestAgentUIBus::testClearMission()
{
    AgentUIBus bus;
    MockRenderer renderer;
    bus.addRenderer(&renderer);

    bus.post(AgentUIEvent::missionCreated(QStringLiteral("m1"), QStringLiteral("A")));
    QCOMPARE(bus.currentMissionId(), QStringLiteral("m1"));

    bus.clearMission();

    QVERIFY(bus.currentMissionId().isEmpty());
    QCOMPARE(renderer.clearCount, 1);
    QCOMPARE(bus.eventsForMission(QStringLiteral("m1")).size(), 0);
}

// ── Timestamp ────────────────────────────────────────────────────────────────

void TestAgentUIBus::testTimestamp()
{
    AgentUIBus bus;
    MockRenderer renderer;
    bus.addRenderer(&renderer);

    auto e = AgentUIEvent::logAdded(QStringLiteral("m1"), AgentLogLevel::Info,
                                    QStringLiteral("test"));
    QCOMPARE(e.timestamp, 0);

    bus.post(e);

    QVERIFY(renderer.received[0].timestamp > 0);
}

// ── Convenience constructors ─────────────────────────────────────────────────

void TestAgentUIBus::testStepConvenience()
{
    auto e = AgentUIEvent::stepAdded(QStringLiteral("m1"), QStringLiteral("s1"),
                                     QStringLiteral("Read"), 0);
    QCOMPARE(e.type, AgentUIEventType::StepAdded);
    QCOMPARE(e.payload[QStringLiteral("stepId")].toString(), QStringLiteral("s1"));
    QCOMPARE(e.payload[QStringLiteral("label")].toString(), QStringLiteral("Read"));
    QCOMPARE(e.payload[QStringLiteral("order")].toInt(), 0);
    QCOMPARE(e.payload[QStringLiteral("status")].toString(), QStringLiteral("pending"));

    auto u = AgentUIEvent::stepUpdated(QStringLiteral("m1"), QStringLiteral("s1"),
                                        AgentStepStatus::Running, QStringLiteral("50%"));
    QCOMPARE(u.type, AgentUIEventType::StepUpdated);
    QCOMPARE(u.payload[QStringLiteral("status")].toString(), QStringLiteral("running"));
    QCOMPARE(u.payload[QStringLiteral("detail")].toString(), QStringLiteral("50%"));
}

void TestAgentUIBus::testMetricConvenience()
{
    auto e = AgentUIEvent::metricUpdated(QStringLiteral("m1"),
                                          QStringLiteral("tokens"),
                                          QJsonValue(12345),
                                          QStringLiteral("tok"));
    QCOMPARE(e.type, AgentUIEventType::MetricUpdated);
    QCOMPARE(e.payload[QStringLiteral("key")].toString(), QStringLiteral("tokens"));
    QCOMPARE(e.payload[QStringLiteral("value")].toInt(), 12345);
    QCOMPARE(e.payload[QStringLiteral("unit")].toString(), QStringLiteral("tok"));
}

void TestAgentUIBus::testLogConvenience()
{
    auto e = AgentUIEvent::logAdded(QStringLiteral("m1"), AgentLogLevel::Error,
                                     QStringLiteral("fail!"));
    QCOMPARE(e.type, AgentUIEventType::LogAdded);
    QCOMPARE(e.payload[QStringLiteral("level")].toString(), QStringLiteral("error"));
    QCOMPARE(e.payload[QStringLiteral("message")].toString(), QStringLiteral("fail!"));
}

void TestAgentUIBus::testArtifactConvenience()
{
    QJsonObject meta{{QStringLiteral("lines"), 42}};
    auto e = AgentUIEvent::artifactAdded(QStringLiteral("m1"),
                                          QStringLiteral("file"),
                                          QStringLiteral("/src/foo.cpp"),
                                          meta);
    QCOMPARE(e.type, AgentUIEventType::ArtifactAdded);
    QCOMPARE(e.payload[QStringLiteral("type")].toString(), QStringLiteral("file"));
    QCOMPARE(e.payload[QStringLiteral("path")].toString(), QStringLiteral("/src/foo.cpp"));
    QCOMPARE(e.payload[QStringLiteral("meta")].toObject()[QStringLiteral("lines")].toInt(), 42);
}

void TestAgentUIBus::testPanelConvenience()
{
    auto c = AgentUIEvent::panelCreated(QStringLiteral("m1"),
                                         QStringLiteral("p1"),
                                         QStringLiteral("Diff View"),
                                         QStringLiteral("diff"));
    QCOMPARE(c.type, AgentUIEventType::PanelCreated);
    QCOMPARE(c.payload[QStringLiteral("panelId")].toString(), QStringLiteral("p1"));
    QCOMPARE(c.payload[QStringLiteral("title")].toString(), QStringLiteral("Diff View"));

    auto u = AgentUIEvent::panelUpdated(QStringLiteral("m1"), QStringLiteral("p1"),
                                         QJsonObject{{QStringLiteral("html"), QStringLiteral("<p>hi</p>")}});
    QCOMPARE(u.type, AgentUIEventType::PanelUpdated);

    auto r = AgentUIEvent::panelRemoved(QStringLiteral("m1"), QStringLiteral("p1"));
    QCOMPARE(r.type, AgentUIEventType::PanelRemoved);
}

void TestAgentUIBus::testCustomEventConvenience()
{
    auto e = AgentUIEvent::custom(QStringLiteral("m1"), QStringLiteral("myEvent"),
                                   QJsonObject{{QStringLiteral("foo"), QStringLiteral("bar")}});
    QCOMPARE(e.type, AgentUIEventType::CustomEvent);
    QCOMPARE(e.payload[QStringLiteral("eventName")].toString(), QStringLiteral("myEvent"));
    QCOMPARE(e.payload[QStringLiteral("foo")].toString(), QStringLiteral("bar"));
}

// ── Signal emission ──────────────────────────────────────────────────────────

void TestAgentUIBus::testSignalEmitted()
{
    AgentUIBus bus;
    QSignalSpy spy(&bus, &AgentUIBus::eventDispatched);
    QVERIFY(spy.isValid());

    bus.post(AgentUIEvent::missionCreated(QStringLiteral("m1"), QStringLiteral("Test")));
    QCOMPARE(spy.count(), 1);

    bus.post(AgentUIEvent::logAdded(QStringLiteral("m1"), AgentLogLevel::Info,
                                    QStringLiteral("msg")));
    QCOMPARE(spy.count(), 2);
}

QTEST_MAIN(TestAgentUIBus)
#include "test_agentuibus.moc"
