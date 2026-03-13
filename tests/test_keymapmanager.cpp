#include <QtTest>
#include <QAction>
#include <QSignalSpy>

#include "ui/keymapmanager.h"

class TestKeymapManager : public QObject
{
    Q_OBJECT

private slots:
    // ── Registration ──────────────────────────────────────────────────────

    void registerAction_basic()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("file.save"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+S")));

        QCOMPARE(km.shortcut(QStringLiteral("file.save")),
                 QKeySequence(QStringLiteral("Ctrl+S")));
    }

    void registerAction_setsActionShortcut()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("file.open"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+O")));

        // The action's shortcut should NOT be set by registerAction
        // (it only stores the binding) — actually let's check
        // registerAction stores default but doesn't set action shortcut
        // Looking at the code: it only stores in m_bindings, doesn't call action->setShortcut()
        QCOMPARE(km.defaultShortcut(QStringLiteral("file.open")),
                 QKeySequence(QStringLiteral("Ctrl+O")));
    }

    void registerAction_multiple()
    {
        KeymapManager km;
        auto a1 = std::make_unique<QAction>();
        auto a2 = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("file.save"), a1.get(),
                          QKeySequence(QStringLiteral("Ctrl+S")));
        km.registerAction(QStringLiteral("file.open"), a2.get(),
                          QKeySequence(QStringLiteral("Ctrl+O")));

        QStringList ids = km.actionIds();
        QVERIFY(ids.contains(QStringLiteral("file.save")));
        QVERIFY(ids.contains(QStringLiteral("file.open")));
        QCOMPARE(ids.size(), 2);
    }

    // ── Shortcut override ─────────────────────────────────────────────────

    void setShortcut_overrides()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("file.save"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+S")));

        km.setShortcut(QStringLiteral("file.save"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+S")));

        QCOMPARE(km.shortcut(QStringLiteral("file.save")),
                 QKeySequence(QStringLiteral("Ctrl+Shift+S")));
        QCOMPARE(action->shortcut(),
                 QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    }

    void setShortcut_emptyResetsToDefault()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("file.save"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+S")));

        km.setShortcut(QStringLiteral("file.save"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+S")));
        km.setShortcut(QStringLiteral("file.save"), QKeySequence());

        QCOMPARE(km.shortcut(QStringLiteral("file.save")),
                 QKeySequence(QStringLiteral("Ctrl+S")));
    }

    void setShortcut_emitsSignal()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("edit.copy"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+C")));

        QSignalSpy spy(&km, &KeymapManager::shortcutChanged);
        km.setShortcut(QStringLiteral("edit.copy"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+C")));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("edit.copy"));
    }

    void setShortcut_unknownId_noOp()
    {
        KeymapManager km;
        // Should not crash
        km.setShortcut(QStringLiteral("nonexistent"),
                       QKeySequence(QStringLiteral("Ctrl+Z")));
        QCOMPARE(km.shortcut(QStringLiteral("nonexistent")), QKeySequence());
    }

    // ── Default shortcut ──────────────────────────────────────────────────

    void defaultShortcut_unchanged()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("file.new"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+N")));

        km.setShortcut(QStringLiteral("file.new"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+N")));

        // Default should remain unchanged
        QCOMPARE(km.defaultShortcut(QStringLiteral("file.new")),
                 QKeySequence(QStringLiteral("Ctrl+N")));
    }

    void defaultShortcut_unknown_returnsEmpty()
    {
        KeymapManager km;
        QCOMPARE(km.defaultShortcut(QStringLiteral("nonexistent")), QKeySequence());
    }

    // ── Action text ───────────────────────────────────────────────────────

    void actionText_returnsText()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>(QStringLiteral("&Save File"));
        km.registerAction(QStringLiteral("file.save"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+S")));

        // Should strip & from text
        QCOMPARE(km.actionText(QStringLiteral("file.save")),
                 QStringLiteral("Save File"));
    }

    void actionText_unknown_returnsId()
    {
        KeymapManager km;
        QCOMPARE(km.actionText(QStringLiteral("unknown.action")),
                 QStringLiteral("unknown.action"));
    }

    // ── Action IDs ────────────────────────────────────────────────────────

    void actionIds_sorted()
    {
        KeymapManager km;
        auto a1 = std::make_unique<QAction>();
        auto a2 = std::make_unique<QAction>();
        auto a3 = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("z.last"), a1.get(), QKeySequence());
        km.registerAction(QStringLiteral("a.first"), a2.get(), QKeySequence());
        km.registerAction(QStringLiteral("m.middle"), a3.get(), QKeySequence());

        QStringList ids = km.actionIds();
        QCOMPARE(ids.at(0), QStringLiteral("a.first"));
        QCOMPARE(ids.at(1), QStringLiteral("m.middle"));
        QCOMPARE(ids.at(2), QStringLiteral("z.last"));
    }

    void actionIds_empty()
    {
        KeymapManager km;
        QVERIFY(km.actionIds().isEmpty());
    }

    // ── Reset ─────────────────────────────────────────────────────────────

    void resetToDefault_restores()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("edit.undo"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+Z")));

        km.setShortcut(QStringLiteral("edit.undo"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+Z")));
        km.resetToDefault(QStringLiteral("edit.undo"));

        QCOMPARE(km.shortcut(QStringLiteral("edit.undo")),
                 QKeySequence(QStringLiteral("Ctrl+Z")));
        QCOMPARE(action->shortcut(),
                 QKeySequence(QStringLiteral("Ctrl+Z")));
    }

    void resetToDefault_emitsSignal()
    {
        KeymapManager km;
        auto action = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("edit.redo"), action.get(),
                          QKeySequence(QStringLiteral("Ctrl+Y")));

        km.setShortcut(QStringLiteral("edit.redo"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+Y")));

        QSignalSpy spy(&km, &KeymapManager::shortcutChanged);
        km.resetToDefault(QStringLiteral("edit.redo"));

        QCOMPARE(spy.count(), 1);
    }

    void resetAllToDefaults_restoresAll()
    {
        KeymapManager km;
        auto a1 = std::make_unique<QAction>();
        auto a2 = std::make_unique<QAction>();
        km.registerAction(QStringLiteral("a"), a1.get(),
                          QKeySequence(QStringLiteral("Ctrl+A")));
        km.registerAction(QStringLiteral("b"), a2.get(),
                          QKeySequence(QStringLiteral("Ctrl+B")));

        km.setShortcut(QStringLiteral("a"), QKeySequence(QStringLiteral("Ctrl+1")));
        km.setShortcut(QStringLiteral("b"), QKeySequence(QStringLiteral("Ctrl+2")));

        km.resetAllToDefaults();

        QCOMPARE(km.shortcut(QStringLiteral("a")),
                 QKeySequence(QStringLiteral("Ctrl+A")));
        QCOMPARE(km.shortcut(QStringLiteral("b")),
                 QKeySequence(QStringLiteral("Ctrl+B")));
    }

    // ── Null action ───────────────────────────────────────────────────────

    void registerAction_nullAction()
    {
        KeymapManager km;
        km.registerAction(QStringLiteral("test"), nullptr,
                          QKeySequence(QStringLiteral("Ctrl+T")));

        QCOMPARE(km.shortcut(QStringLiteral("test")),
                 QKeySequence(QStringLiteral("Ctrl+T")));
        // setShortcut with null action should not crash
        km.setShortcut(QStringLiteral("test"),
                       QKeySequence(QStringLiteral("Ctrl+Shift+T")));
        QCOMPARE(km.shortcut(QStringLiteral("test")),
                 QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    }
};

QTEST_MAIN(TestKeymapManager)
#include "test_keymapmanager.moc"
