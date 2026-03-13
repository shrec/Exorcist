#include <QTest>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>

#include "agent/diagnosticsnotifier.h"

// ── Test ──────────────────────────────────────────────────────────────────────

class TestDiagnosticsNotifier : public QObject
{
    Q_OBJECT

private:
    static QJsonArray makeDiags(int errors, int warnings)
    {
        QJsonArray arr;
        for (int i = 0; i < errors; ++i) {
            QJsonObject d;
            d[QLatin1String("severity")] = 1;  // Error
            d[QLatin1String("message")]  = QStringLiteral("error %1").arg(i);
            d[QLatin1String("range")]    = QJsonObject{
                {QLatin1String("start"), QJsonObject{
                    {QLatin1String("line"), i},
                    {QLatin1String("character"), 0}
                }},
                {QLatin1String("end"), QJsonObject{
                    {QLatin1String("line"), i},
                    {QLatin1String("character"), 10}
                }}
            };
            arr.append(d);
        }
        for (int i = 0; i < warnings; ++i) {
            QJsonObject d;
            d[QLatin1String("severity")] = 2;  // Warning
            d[QLatin1String("message")]  = QStringLiteral("warning %1").arg(i);
            d[QLatin1String("range")]    = QJsonObject{
                {QLatin1String("start"), QJsonObject{
                    {QLatin1String("line"), errors + i},
                    {QLatin1String("character"), 0}
                }},
                {QLatin1String("end"), QJsonObject{
                    {QLatin1String("line"), errors + i},
                    {QLatin1String("character"), 10}
                }}
            };
            arr.append(d);
        }
        return arr;
    }

private slots:

    // ── Basic state ──────────────────────────────────────────────────────

    void initialState()
    {
        DiagnosticsNotifier n;
        QVERIFY(!n.hasPendingNotification());
        QVERIFY(!n.isEnabled());
        QVERIFY(n.consumeNotification().isEmpty());
    }

    void enableDisable()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);
        QVERIFY(n.isEnabled());
        n.setEnabled(false);
        QVERIFY(!n.isEnabled());
    }

    // ── Disabled → no notifications ──────────────────────────────────────

    void disabledIgnoresDiagnostics()
    {
        DiagnosticsNotifier n;
        // Don't enable — should ignore diagnostics
        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(3, 0));

        // Force timer (won't fire since disabled doesn't start it)
        QVERIFY(!n.hasPendingNotification());
    }

    // ── Enabled + diagnostics → notification after flush ─────────────────

    void publishCausesNotificationAfterFlush()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);

        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(2, 1));

        // Before debounce fires, no notification
        QVERIFY(!n.hasPendingNotification());

        // Flush is a private slot called by the timer.
        // Use QMetaObject::invokeMethod to call it.
        QMetaObject::invokeMethod(&n, "flush");

        QVERIFY(n.hasPendingNotification());
        const QString text = n.consumeNotification();
        QVERIFY(text.contains(QLatin1String("[Diagnostics Update]")));
        QVERIFY(text.contains(QLatin1String("2 error")));
        QVERIFY(text.contains(QLatin1String("1 warning")));

        // After consume, nothing pending
        QVERIFY(!n.hasPendingNotification());
    }

    // ── Same counts → no change ──────────────────────────────────────────

    void noDiffNoNotification()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);

        // Publish once and flush
        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(2, 1));
        QMetaObject::invokeMethod(&n, "flush");
        n.consumeNotification(); // clear

        // Same counts again
        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(2, 1));

        // No change detected → no pending changes → flush should not produce
        QMetaObject::invokeMethod(&n, "flush");
        QVERIFY(!n.hasPendingNotification());
    }

    // ── Changed counts → notification ────────────────────────────────────

    void changedCountsProduceNotification()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);

        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(2, 0));
        QMetaObject::invokeMethod(&n, "flush");
        n.consumeNotification();

        // Error count changes: 2 → 5
        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(5, 0));
        QMetaObject::invokeMethod(&n, "flush");

        QVERIFY(n.hasPendingNotification());
        const QString text = n.consumeNotification();
        QVERIFY(text.contains(QLatin1String("5 error")));
    }

    // ── Resolved errors ──────────────────────────────────────────────────

    void resolvedErrorsShowInNotification()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);

        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(3, 0));
        QMetaObject::invokeMethod(&n, "flush");
        n.consumeNotification();

        // All errors resolved
        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(0, 0));
        QMetaObject::invokeMethod(&n, "flush");

        QVERIFY(n.hasPendingNotification());
        const QString text = n.consumeNotification();
        QVERIFY(text.contains(QLatin1String("all issues resolved")));
    }

    // ── Disable clears pending ───────────────────────────────────────────

    void disableClearsPending()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);

        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(2, 0));
        QMetaObject::invokeMethod(&n, "flush");
        QVERIFY(n.hasPendingNotification());

        n.setEnabled(false);
        QVERIFY(!n.hasPendingNotification());
    }

    // ── Multiple files ───────────────────────────────────────────────────

    void multipleFilesAggregated()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);

        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/main.cpp"), makeDiags(1, 0));
        n.onDiagnosticsPublished(
            QStringLiteral("file:///src/util.cpp"), makeDiags(2, 1));
        QMetaObject::invokeMethod(&n, "flush");

        QVERIFY(n.hasPendingNotification());
        const QString text = n.consumeNotification();
        QVERIFY(text.contains(QLatin1String("3 error")));   // 1+2
        QVERIFY(text.contains(QLatin1String("1 warning"))); // 0+1
        QVERIFY(text.contains(QLatin1String("main.cpp")));
        QVERIFY(text.contains(QLatin1String("util.cpp")));
    }

    // ── Invalid URI ignored ──────────────────────────────────────────────

    void invalidUriIgnored()
    {
        DiagnosticsNotifier n;
        n.setEnabled(true);

        n.onDiagnosticsPublished(
            QStringLiteral("not-a-valid-uri"), makeDiags(1, 0));
        QMetaObject::invokeMethod(&n, "flush");

        QVERIFY(!n.hasPendingNotification());
    }
};

QTEST_MAIN(TestDiagnosticsNotifier)
#include "test_diagnosticsnotifier.moc"
