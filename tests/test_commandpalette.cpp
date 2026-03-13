#include <QtTest>
#include <QLineEdit>
#include <QListWidget>

#include "commandpalette.h"

class TestCommandPalette : public QObject
{
    Q_OBJECT

private:
    // Helper: get the internal QListWidget from CommandPalette
    static QListWidget *listWidget(CommandPalette &cp)
    {
        return cp.findChild<QListWidget *>();
    }

    static QLineEdit *lineEdit(CommandPalette &cp)
    {
        return cp.findChild<QLineEdit *>();
    }

private slots:
    // ── Construction ──────────────────────────────────────────────────────

    void construct_fileMode()
    {
        CommandPalette cp(CommandPalette::FileMode);
        QCOMPARE(cp.selectedCommandIndex(), -1);
        QVERIFY(cp.selectedFile().isEmpty());
    }

    void construct_commandMode()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        QCOMPARE(cp.selectedCommandIndex(), -1);
    }

    // ── setFiles populates list ───────────────────────────────────────────

    void setFiles_populatesList()
    {
        CommandPalette cp(CommandPalette::FileMode);
        cp.setFiles({QStringLiteral("src/main.cpp"),
                     QStringLiteral("src/editor/editorview.cpp"),
                     QStringLiteral("CMakeLists.txt")});

        auto *list = listWidget(cp);
        QVERIFY(list);
        QCOMPARE(list->count(), 3);
    }

    void setFiles_empty()
    {
        CommandPalette cp(CommandPalette::FileMode);
        cp.setFiles({});

        auto *list = listWidget(cp);
        QVERIFY(list);
        QCOMPARE(list->count(), 0);
    }

    // ── setCommands populates list ────────────────────────────────────────

    void setCommands_populatesList()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        cp.setCommands({QStringLiteral("Open File"),
                        QStringLiteral("Save File"),
                        QStringLiteral("Close Tab")});

        auto *list = listWidget(cp);
        QVERIFY(list);
        QCOMPARE(list->count(), 3);
    }

    // ── Filtering ─────────────────────────────────────────────────────────

    void filter_matchesSubstring()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        cp.setCommands({QStringLiteral("Open File"),
                        QStringLiteral("Save File"),
                        QStringLiteral("Close Tab"),
                        QStringLiteral("Toggle Terminal")});

        auto *edit = lineEdit(cp);
        QVERIFY(edit);
        edit->setText(QStringLiteral("file"));

        auto *list = listWidget(cp);
        QCOMPARE(list->count(), 2); // Open File, Save File
    }

    void filter_caseInsensitive()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        cp.setCommands({QStringLiteral("Open File"),
                        QStringLiteral("Build Project")});

        auto *edit = lineEdit(cp);
        edit->setText(QStringLiteral("OPEN"));

        auto *list = listWidget(cp);
        QCOMPARE(list->count(), 1);
    }

    void filter_emptyShowsAll()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        cp.setCommands({QStringLiteral("A"), QStringLiteral("B"),
                        QStringLiteral("C")});

        auto *edit = lineEdit(cp);
        edit->setText(QStringLiteral("x")); // filter down
        QCOMPARE(listWidget(cp)->count(), 0);

        edit->setText(QString()); // clear filter
        QCOMPARE(listWidget(cp)->count(), 3);
    }

    void filter_noMatch()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        cp.setCommands({QStringLiteral("Open"), QStringLiteral("Save")});

        auto *edit = lineEdit(cp);
        edit->setText(QStringLiteral("zzznomatch"));

        QCOMPARE(listWidget(cp)->count(), 0);
    }

    // ── File mode display ─────────────────────────────────────────────────

    void fileMode_displaysFilename()
    {
        CommandPalette cp(CommandPalette::FileMode);
        cp.setFiles({QStringLiteral("/home/user/project/src/main.cpp")});

        auto *list = listWidget(cp);
        QCOMPARE(list->count(), 1);
        // Display should contain the filename
        QVERIFY(list->item(0)->text().contains(QStringLiteral("main.cpp")));
    }

    void fileMode_filterByPath()
    {
        CommandPalette cp(CommandPalette::FileMode);
        cp.setFiles({QStringLiteral("src/editor/view.cpp"),
                     QStringLiteral("src/terminal/screen.cpp"),
                     QStringLiteral("tests/test_view.cpp")});

        auto *edit = lineEdit(cp);
        edit->setText(QStringLiteral("editor"));

        QCOMPARE(listWidget(cp)->count(), 1);
    }

    // ── selectedFile before accept ────────────────────────────────────────

    void selectedFile_beforeAccept_empty()
    {
        CommandPalette cp(CommandPalette::FileMode);
        cp.setFiles({QStringLiteral("src/main.cpp")});
        QVERIFY(cp.selectedFile().isEmpty());
    }

    void selectedCommandIndex_beforeAccept_negative()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        cp.setCommands({QStringLiteral("cmd1")});
        QCOMPARE(cp.selectedCommandIndex(), -1);
    }

    // ── Large list ────────────────────────────────────────────────────────

    void filter_largeList()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        QStringList cmds;
        for (int i = 0; i < 1000; ++i)
            cmds << QStringLiteral("Command_%1").arg(i);
        cp.setCommands(cmds);

        QCOMPARE(listWidget(cp)->count(), 1000);

        auto *edit = lineEdit(cp);
        edit->setText(QStringLiteral("_99"));

        // Should match Command_99, Command_990-999 = 11 items
        auto *list = listWidget(cp);
        QVERIFY(list->count() > 0);
        QVERIFY(list->count() <= 11);
    }

    // ── First item selected after filter ──────────────────────────────────

    void filter_selectsFirstItem()
    {
        CommandPalette cp(CommandPalette::CommandMode);
        cp.setCommands({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                        QStringLiteral("Gamma")});

        auto *list = listWidget(cp);
        QCOMPARE(list->currentRow(), 0);

        auto *edit = lineEdit(cp);
        edit->setText(QStringLiteral("beta"));
        QCOMPARE(list->currentRow(), 0); // first match selected
        QCOMPARE(list->count(), 1);
    }
};

QTEST_MAIN(TestCommandPalette)
#include "test_commandpalette.moc"
