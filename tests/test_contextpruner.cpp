#include <QtTest>
#include <QSignalSpy>

#include "agent/contextpruner.h"

class TestContextPruner : public QObject
{
    Q_OBJECT

private:
    static ContextPruner::ContextItem makeItem(const QString &key, int tokens,
                                                int priority = 5, bool pinned = false)
    {
        ContextPruner::ContextItem item;
        item.key = key;
        item.content = QStringLiteral("content for ") + key;
        item.tokens = tokens;
        item.priority = priority;
        item.pinned = pinned;
        return item;
    }

private slots:
    // ── Construction ──────────────────────────────────────────────────────

    void defaultState()
    {
        ContextPruner pruner;
        QCOMPARE(pruner.maxTokens(), 120000);
        QVERIFY(pruner.items().isEmpty());
        QCOMPARE(pruner.totalTokens(), 0);
    }

    void setMaxTokens()
    {
        ContextPruner pruner;
        pruner.setMaxTokens(50000);
        QCOMPARE(pruner.maxTokens(), 50000);
    }

    // ── addItem ───────────────────────────────────────────────────────────

    void addItem_basic()
    {
        ContextPruner pruner;
        QSignalSpy spy(&pruner, &ContextPruner::itemsChanged);
        pruner.addItem(makeItem(QStringLiteral("file:main.cpp"), 1000));

        QCOMPARE(pruner.items().size(), 1);
        QCOMPARE(pruner.totalTokens(), 1000);
        QCOMPARE(spy.count(), 1);
    }

    void addItem_multiple()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500));
        pruner.addItem(makeItem(QStringLiteral("file:b.cpp"), 300));
        pruner.addItem(makeItem(QStringLiteral("terminal"), 200));

        QCOMPARE(pruner.items().size(), 3);
        QCOMPARE(pruner.totalTokens(), 1000);
    }

    void addItem_replacesDuplicate()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500));
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 800));

        QCOMPARE(pruner.items().size(), 1);
        QCOMPARE(pruner.totalTokens(), 800);
    }

    void addItem_emitsSignalOnReplace()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500));

        QSignalSpy spy(&pruner, &ContextPruner::itemsChanged);
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 800));
        QCOMPARE(spy.count(), 1);
    }

    // ── removeItem ────────────────────────────────────────────────────────

    void removeItem_existing()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500));
        pruner.addItem(makeItem(QStringLiteral("file:b.cpp"), 300));

        QSignalSpy spy(&pruner, &ContextPruner::itemsChanged);
        pruner.removeItem(QStringLiteral("file:a.cpp"));

        QCOMPARE(pruner.items().size(), 1);
        QCOMPARE(pruner.totalTokens(), 300);
        QCOMPARE(spy.count(), 1);
    }

    void removeItem_nonexistent()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500));

        QSignalSpy spy(&pruner, &ContextPruner::itemsChanged);
        pruner.removeItem(QStringLiteral("nonexistent"));

        QCOMPARE(pruner.items().size(), 1);
        QCOMPARE(spy.count(), 0);
    }

    // ── pinItem ───────────────────────────────────────────────────────────

    void pinItem_setsFlag()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500));
        pruner.pinItem(QStringLiteral("file:a.cpp"), true);

        QVERIFY(pruner.items()[0].pinned);
    }

    void pinItem_unpins()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500, 5, true));
        pruner.pinItem(QStringLiteral("file:a.cpp"), false);

        QVERIFY(!pruner.items()[0].pinned);
    }

    void pinItem_nonexistent_noOp()
    {
        ContextPruner pruner;
        QSignalSpy spy(&pruner, &ContextPruner::itemsChanged);
        pruner.pinItem(QStringLiteral("nonexistent"), true);
        QCOMPARE(spy.count(), 0);
    }

    void pinItem_emitsSignal()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500));

        QSignalSpy spy(&pruner, &ContextPruner::itemsChanged);
        pruner.pinItem(QStringLiteral("file:a.cpp"), true);
        QCOMPARE(spy.count(), 1);
    }

    // ── setPriority ───────────────────────────────────────────────────────

    void setPriority_changesValue()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("file:a.cpp"), 500, 5));
        pruner.setPriority(QStringLiteral("file:a.cpp"), 1);

        QCOMPARE(pruner.items()[0].priority, 1);
    }

    void setPriority_nonexistent_noOp()
    {
        ContextPruner pruner;
        QSignalSpy spy(&pruner, &ContextPruner::itemsChanged);
        pruner.setPriority(QStringLiteral("nonexistent"), 1);
        QCOMPARE(spy.count(), 0);
    }

    // ── prune — under budget ─────────────────────────────────────────────

    void prune_underBudget_returnsAll()
    {
        ContextPruner pruner;
        pruner.setMaxTokens(10000);
        pruner.addItem(makeItem(QStringLiteral("a"), 1000));
        pruner.addItem(makeItem(QStringLiteral("b"), 2000));
        pruner.addItem(makeItem(QStringLiteral("c"), 3000));

        auto result = pruner.prune();
        QCOMPARE(result.size(), 3);
    }

    void prune_exactlyAtBudget_returnsAll()
    {
        ContextPruner pruner;
        pruner.setMaxTokens(6000);
        pruner.addItem(makeItem(QStringLiteral("a"), 2000));
        pruner.addItem(makeItem(QStringLiteral("b"), 2000));
        pruner.addItem(makeItem(QStringLiteral("c"), 2000));

        auto result = pruner.prune();
        QCOMPARE(result.size(), 3);
    }

    // ── prune — over budget ──────────────────────────────────────────────

    void prune_overBudget_dropsLowestPriority()
    {
        ContextPruner pruner;
        pruner.setMaxTokens(5000);
        pruner.addItem(makeItem(QStringLiteral("important"), 3000, 1));  // priority 1 (highest)
        pruner.addItem(makeItem(QStringLiteral("medium"), 2000, 5));     // priority 5
        pruner.addItem(makeItem(QStringLiteral("low"), 1000, 9));        // priority 9 (lowest)

        auto result = pruner.prune();
        // 3000 + 2000 = 5000 fits, adding 1000 would be 6000 > 5000
        QCOMPARE(result.size(), 2);

        QStringList keys;
        for (const auto &item : result)
            keys << item.key;
        QVERIFY(keys.contains(QStringLiteral("important")));
        QVERIFY(keys.contains(QStringLiteral("medium")));
    }

    // ── prune — pinned items ─────────────────────────────────────────────

    void prune_pinnedItemsPreferred()
    {
        ContextPruner pruner;
        pruner.setMaxTokens(4000);
        pruner.addItem(makeItem(QStringLiteral("pinned"), 3000, 10, true));   // low priority but pinned
        pruner.addItem(makeItem(QStringLiteral("unpinned"), 2000, 1, false)); // high priority but unpinned

        auto result = pruner.prune();
        // 3000 fits first (pinned), then 2000 would exceed → only pinned survives
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].key, QStringLiteral("pinned"));
    }

    // ── prune — same priority, smaller items win ─────────────────────────

    void prune_samePriority_smallerFirst()
    {
        ContextPruner pruner;
        pruner.setMaxTokens(3000);
        pruner.addItem(makeItem(QStringLiteral("big"), 2500, 5));
        pruner.addItem(makeItem(QStringLiteral("small1"), 1000, 5));
        pruner.addItem(makeItem(QStringLiteral("small2"), 1500, 5));

        auto result = pruner.prune();
        // Sorted by tokens (ascending): small1(1000), small2(1500), big(2500)
        // 1000 + 1500 = 2500 ≤ 3000, next 2500 would be 5000 > 3000
        QCOMPARE(result.size(), 2);

        QStringList keys;
        for (const auto &item : result)
            keys << item.key;
        QVERIFY(keys.contains(QStringLiteral("small1")));
        QVERIFY(keys.contains(QStringLiteral("small2")));
    }

    // ── prune — empty ────────────────────────────────────────────────────

    void prune_empty()
    {
        ContextPruner pruner;
        pruner.setMaxTokens(1000);
        auto result = pruner.prune();
        QVERIFY(result.isEmpty());
    }

    // ── prune — totalTokens ──────────────────────────────────────────────

    void totalTokens_accurate()
    {
        ContextPruner pruner;
        pruner.addItem(makeItem(QStringLiteral("a"), 100));
        pruner.addItem(makeItem(QStringLiteral("b"), 200));
        pruner.addItem(makeItem(QStringLiteral("c"), 300));
        QCOMPARE(pruner.totalTokens(), 600);

        pruner.removeItem(QStringLiteral("b"));
        QCOMPARE(pruner.totalTokens(), 400);
    }
};

QTEST_MAIN(TestContextPruner)
#include "test_contextpruner.moc"
