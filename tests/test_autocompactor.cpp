#include <QtTest>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>

#include "agent/autocompactor.h"

class TestAutoCompactor : public QObject
{
    Q_OBJECT

private:
    static QJsonObject makeMsg(const QString &role, const QString &content)
    {
        QJsonObject msg;
        msg[QStringLiteral("role")] = role;
        msg[QStringLiteral("content")] = content;
        return msg;
    }

    static QJsonArray makeHistory(int userAssistantPairs, int charsPerMessage = 100)
    {
        QJsonArray history;
        // System message
        history.append(makeMsg(QStringLiteral("system"), QStringLiteral("You are a helpful assistant.")));

        for (int i = 0; i < userAssistantPairs; ++i) {
            history.append(makeMsg(QStringLiteral("user"),
                                   QString(charsPerMessage, QChar('u'))));
            history.append(makeMsg(QStringLiteral("assistant"),
                                   QString(charsPerMessage, QChar('a'))));
        }
        return history;
    }

private slots:
    // ── Construction ──────────────────────────────────────────────────────

    void defaultState()
    {
        AutoCompactor compactor;
        // Default threshold 0.75, max 120000
        QJsonArray empty;
        QVERIFY(!compactor.shouldCompact(empty, 0));
    }

    // ── shouldCompact ─────────────────────────────────────────────────────

    void shouldCompact_belowThreshold()
    {
        AutoCompactor compactor;
        compactor.setMaxContextTokens(10000);
        compactor.setThresholdRatio(0.75);

        QJsonArray history = makeHistory(5);
        QVERIFY(!compactor.shouldCompact(history, 5000));  // 50% < 75%
    }

    void shouldCompact_aboveThreshold()
    {
        AutoCompactor compactor;
        compactor.setMaxContextTokens(10000);
        compactor.setThresholdRatio(0.75);

        QJsonArray history = makeHistory(10);
        QVERIFY(compactor.shouldCompact(history, 8000));  // 80% > 75%
    }

    void shouldCompact_exactlyAtThreshold()
    {
        AutoCompactor compactor;
        compactor.setMaxContextTokens(10000);
        compactor.setThresholdRatio(0.75);

        // 7500 is exactly 75% — should NOT trigger (> not >=)
        QVERIFY(!compactor.shouldCompact(QJsonArray(), 7500));
    }

    void shouldCompact_justAboveThreshold()
    {
        AutoCompactor compactor;
        compactor.setMaxContextTokens(10000);
        compactor.setThresholdRatio(0.75);

        QVERIFY(compactor.shouldCompact(QJsonArray(), 7501));
    }

    // ── estimateTokens ────────────────────────────────────────────────────

    void estimateTokens_empty()
    {
        QCOMPARE(AutoCompactor::estimateTokens(QJsonArray()), 0);
    }

    void estimateTokens_singleMessage()
    {
        QJsonArray msgs;
        msgs.append(makeMsg(QStringLiteral("user"), QStringLiteral("Hello world!")));
        // "Hello world!" = 12 chars → 12/4 = 3 tokens
        QCOMPARE(AutoCompactor::estimateTokens(msgs), 3);
    }

    void estimateTokens_multipleMessages()
    {
        QJsonArray msgs;
        msgs.append(makeMsg(QStringLiteral("user"), QString(100, 'x')));
        msgs.append(makeMsg(QStringLiteral("assistant"), QString(200, 'y')));
        // 300 chars / 4 = 75
        QCOMPARE(AutoCompactor::estimateTokens(msgs), 75);
    }

    // ── compact — too small ──────────────────────────────────────────────

    void compact_tooSmall_returnsUnchanged()
    {
        AutoCompactor compactor;
        QJsonArray history;
        history.append(makeMsg(QStringLiteral("system"), QStringLiteral("sys")));
        history.append(makeMsg(QStringLiteral("user"), QStringLiteral("hi")));
        history.append(makeMsg(QStringLiteral("assistant"), QStringLiteral("hello")));
        history.append(makeMsg(QStringLiteral("user"), QStringLiteral("bye")));

        QJsonArray result = compactor.compact(history);
        QCOMPARE(result.size(), history.size());
    }

    // ── compact — normal operation ───────────────────────────────────────

    void compact_summarizesMiddle()
    {
        AutoCompactor compactor;
        QJsonArray history = makeHistory(10); // 1 sys + 20 user/assistant = 21

        QJsonArray result = compactor.compact(history);
        // Should be smaller: sys + summary + ack + last 4 = 7
        QVERIFY(result.size() < history.size());
        QCOMPARE(result.size(), 7);  // sys(1) + summary(1) + ack(1) + recent(4)

        // First message preserved (system)
        QCOMPARE(result[0].toObject()[QStringLiteral("role")].toString(),
                 QStringLiteral("system"));

        // Summary message
        QCOMPARE(result[1].toObject()[QStringLiteral("role")].toString(),
                 QStringLiteral("user"));
        QVERIFY(result[1].toObject()[QStringLiteral("content")].toString()
                .contains(QStringLiteral("Conversation Summary")));

        // Ack message
        QCOMPARE(result[2].toObject()[QStringLiteral("role")].toString(),
                 QStringLiteral("assistant"));
    }

    void compact_preservesLast4Messages()
    {
        AutoCompactor compactor;
        QJsonArray history = makeHistory(10);
        int origSize = history.size();

        QJsonArray result = compactor.compact(history);

        // Last 4 messages should be identical
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(result[result.size() - 4 + i].toObject(),
                     history[origSize - 4 + i].toObject());
        }
    }

    void compact_preservesSystemMessage()
    {
        AutoCompactor compactor;
        QJsonArray history = makeHistory(10);
        history[0] = makeMsg(QStringLiteral("system"),
                             QStringLiteral("You are a custom system prompt."));

        QJsonArray result = compactor.compact(history);
        QCOMPARE(result[0].toObject()[QStringLiteral("content")].toString(),
                 QStringLiteral("You are a custom system prompt."));
    }

    // ── compact — reduces tokens ─────────────────────────────────────────

    void compact_reducesTokenCount()
    {
        AutoCompactor compactor;
        QJsonArray history = makeHistory(10, 500);  // 500 chars per message

        int before = AutoCompactor::estimateTokens(history);
        QJsonArray result = compactor.compact(history);
        int after = AutoCompactor::estimateTokens(result);

        QVERIFY(after < before);
    }

    // ── compact — signal emitted ─────────────────────────────────────────

    void compact_emitsSignal()
    {
        AutoCompactor compactor;
        QSignalSpy spy(&compactor, &AutoCompactor::compactionPerformed);

        QJsonArray history = makeHistory(10);
        compactor.compact(history);

        QCOMPARE(spy.count(), 1);
        int removedMessages = spy[0][0].toInt();
        int savedTokens = spy[0][1].toInt();
        QVERIFY(removedMessages > 0);
        QVERIFY(savedTokens >= 0);
    }

    // ── compact — summary includes topics ────────────────────────────────

    void compact_summaryIncludesMessageCounts()
    {
        AutoCompactor compactor;
        QJsonArray history;
        history.append(makeMsg(QStringLiteral("system"), QStringLiteral("sys")));
        // 5 user/assistant exchanges + some tool messages
        for (int i = 0; i < 5; ++i) {
            history.append(makeMsg(QStringLiteral("user"),
                                   QStringLiteral("Question %1").arg(i)));
            history.append(makeMsg(QStringLiteral("assistant"),
                                   QStringLiteral("Answer %1").arg(i)));
        }
        // 1 + 10 + add extra so compact runs (need > 4 after sys)
        // total = 11, keepRecent=4, summarizeEnd = 7, so 6 msgs summarized
        QVERIFY(history.size() > 5);

        QJsonArray result = compactor.compact(history);
        QString summary = result[1].toObject()[QStringLiteral("content")].toString();
        QVERIFY(summary.contains(QStringLiteral("Compacted")));
        QVERIFY(summary.contains(QStringLiteral("user")));
        QVERIFY(summary.contains(QStringLiteral("assistant")));
    }

    // ── Edge: exactly 5 messages ─────────────────────────────────────────

    void compact_fiveMessages_compactsOne()
    {
        AutoCompactor compactor;
        QJsonArray history;
        history.append(makeMsg(QStringLiteral("system"), QStringLiteral("sys")));
        history.append(makeMsg(QStringLiteral("user"), QStringLiteral("q1")));
        history.append(makeMsg(QStringLiteral("assistant"), QStringLiteral("a1")));
        history.append(makeMsg(QStringLiteral("user"), QStringLiteral("q2")));
        history.append(makeMsg(QStringLiteral("assistant"), QStringLiteral("a2")));

        // 5 messages: keepStart=1, summarizeEnd=5-4=1, so summarizeEnd <= keepStart
        // Should return unchanged
        QJsonArray result = compactor.compact(history);
        QCOMPARE(result.size(), history.size());
    }

    // ── setThresholdRatio / setMaxContextTokens ──────────────────────────

    void customThreshold()
    {
        AutoCompactor compactor;
        compactor.setMaxContextTokens(1000);
        compactor.setThresholdRatio(0.5);

        QVERIFY(!compactor.shouldCompact(QJsonArray(), 500));  // exactly 50%
        QVERIFY(compactor.shouldCompact(QJsonArray(), 501));
    }
};

QTEST_MAIN(TestAutoCompactor)
#include "test_autocompactor.moc"
