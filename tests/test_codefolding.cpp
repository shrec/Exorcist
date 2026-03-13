#include <QTest>
#include <QSignalSpy>
#include <QTextBlock>
#include <QTextDocument>

#include "editor/codefoldingengine.h"

class TestCodeFolding : public QObject
{
    Q_OBJECT

private slots:
    void testNoBracesNoRegions();
    void testSingleBraceRegion();
    void testNestedBraces();
    void testMultipleSiblingRegions();
    void testBracesInStringsIgnored();
    void testSingleLineCommentBracesIgnored();
    void testFoldAndUnfold();
    void testToggleFold();
    void testUnfoldAll();
    void testNestedFoldHidesCorrectly();
    void testIndentBasedRegions();
    void testIsFoldableAndIsFolded();
    void testFoldSignalEmitted();
    void testEmptyDocument();
};

void TestCodeFolding::testNoBracesNoRegions()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral("int x = 5;\nreturn x;\n"));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QCOMPARE(engine.regions().size(), 0);
}

void TestCodeFolding::testSingleBraceRegion()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"
        "    int x = 1;\n"
        "}\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QCOMPARE(engine.regions().size(), 1);
    QCOMPARE(engine.regions()[0].startBlock, 0);
    QCOMPARE(engine.regions()[0].endBlock, 2);
}

void TestCodeFolding::testNestedBraces()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"       // block 0
        "    if (true) {\n"    // block 1
        "        bar();\n"     // block 2
        "    }\n"              // block 3
        "}\n"                  // block 4
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QCOMPARE(engine.regions().size(), 2);
    // Regions sorted by start block
    QCOMPARE(engine.regions()[0].startBlock, 0);
    QCOMPARE(engine.regions()[0].endBlock, 4);
    QCOMPARE(engine.regions()[1].startBlock, 1);
    QCOMPARE(engine.regions()[1].endBlock, 3);
}

void TestCodeFolding::testMultipleSiblingRegions()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"    // 0
        "    x();\n"        // 1
        "}\n"               // 2
        "void bar() {\n"    // 3
        "    y();\n"        // 4
        "}\n"               // 5
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QCOMPARE(engine.regions().size(), 2);
    QCOMPARE(engine.regions()[0].startBlock, 0);
    QCOMPARE(engine.regions()[0].endBlock, 2);
    QCOMPARE(engine.regions()[1].startBlock, 3);
    QCOMPARE(engine.regions()[1].endBlock, 5);
}

void TestCodeFolding::testBracesInStringsIgnored()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"
        "    auto s = \"{ not a fold }\";\n"
        "}\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    // Only one region: the function, no region from string braces
    QCOMPARE(engine.regions().size(), 1);
    QCOMPARE(engine.regions()[0].startBlock, 0);
    QCOMPARE(engine.regions()[0].endBlock, 2);
}

void TestCodeFolding::testSingleLineCommentBracesIgnored()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"
        "    // { not a fold }\n"
        "}\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QCOMPARE(engine.regions().size(), 1);
    QCOMPARE(engine.regions()[0].startBlock, 0);
    QCOMPARE(engine.regions()[0].endBlock, 2);
}

void TestCodeFolding::testFoldAndUnfold()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"
        "    int x = 1;\n"
        "    int y = 2;\n"
        "}\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QVERIFY(engine.isFoldable(0));
    QVERIFY(!engine.isFolded(0));

    engine.fold(0);
    QVERIFY(engine.isFolded(0));

    // Inner blocks should be hidden
    QVERIFY(!doc.findBlockByNumber(1).isVisible());
    QVERIFY(!doc.findBlockByNumber(2).isVisible());
    QVERIFY(!doc.findBlockByNumber(3).isVisible());

    engine.unfold(0);
    QVERIFY(!engine.isFolded(0));

    // Blocks should be visible again
    QVERIFY(doc.findBlockByNumber(1).isVisible());
    QVERIFY(doc.findBlockByNumber(2).isVisible());
    QVERIFY(doc.findBlockByNumber(3).isVisible());
}

void TestCodeFolding::testToggleFold()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"
        "    bar();\n"
        "}\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    // Toggle from expanded → collapsed
    QVERIFY(engine.toggleFold(0));
    QVERIFY(engine.isFolded(0));

    // Toggle from collapsed → expanded
    QVERIFY(engine.toggleFold(0));
    QVERIFY(!engine.isFolded(0));

    // Toggle on non-foldable line returns false
    QVERIFY(!engine.toggleFold(1));
}

void TestCodeFolding::testUnfoldAll()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"       // 0
        "    if (true) {\n"    // 1
        "        bar();\n"     // 2
        "    }\n"              // 3
        "}\n"                  // 4
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    engine.fold(0);
    engine.fold(1);
    QVERIFY(engine.isFolded(0));
    QVERIFY(engine.isFolded(1));

    engine.unfoldAll();
    QVERIFY(!engine.isFolded(0));
    QVERIFY(!engine.isFolded(1));

    // All blocks should be visible
    for (int i = 0; i < 5; ++i)
        QVERIFY(doc.findBlockByNumber(i).isVisible());
}

void TestCodeFolding::testNestedFoldHidesCorrectly()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"       // 0
        "    if (true) {\n"    // 1
        "        bar();\n"     // 2
        "    }\n"              // 3
        "    baz();\n"         // 4
        "}\n"                  // 5
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    // Fold inner region first
    engine.fold(1);
    QVERIFY(!doc.findBlockByNumber(2).isVisible());
    QVERIFY(!doc.findBlockByNumber(3).isVisible());
    QVERIFY(doc.findBlockByNumber(4).isVisible()); // baz() still visible

    // Now fold outer region — everything 1..5 hidden
    engine.fold(0);
    QVERIFY(!doc.findBlockByNumber(1).isVisible());
    QVERIFY(!doc.findBlockByNumber(4).isVisible());

    // Unfold outer — inner should still be folded
    engine.unfold(0);
    QVERIFY(doc.findBlockByNumber(1).isVisible());
    QVERIFY(doc.findBlockByNumber(4).isVisible());
    // Inner fold still active — blocks 2,3 should remain hidden
    QVERIFY(!doc.findBlockByNumber(2).isVisible());
    QVERIFY(!doc.findBlockByNumber(3).isVisible());
}

void TestCodeFolding::testIndentBasedRegions()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "def foo():\n"
        "    x = 1\n"
        "    y = 2\n"
        "z = 3\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("python"));
    engine.update();

    QVERIFY(engine.regions().size() >= 1);
    // def foo() starts at block 0, scope extends to block 2
    QCOMPARE(engine.regions()[0].startBlock, 0);
    QCOMPARE(engine.regions()[0].endBlock, 2);
}

void TestCodeFolding::testIsFoldableAndIsFolded()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"
        "    bar();\n"
        "}\n"
        "int x = 5;\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QVERIFY(engine.isFoldable(0));
    QVERIFY(!engine.isFoldable(1));
    QVERIFY(!engine.isFoldable(2));
    QVERIFY(!engine.isFoldable(3));

    QVERIFY(!engine.isFolded(0));
    engine.fold(0);
    QVERIFY(engine.isFolded(0));
    QVERIFY(!engine.isFolded(1)); // not a fold start
}

void TestCodeFolding::testFoldSignalEmitted()
{
    QTextDocument doc;
    doc.setPlainText(QStringLiteral(
        "void foo() {\n"
        "    bar();\n"
        "}\n"
    ));

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QSignalSpy spy(&engine, &CodeFoldingEngine::foldStateChanged);
    engine.fold(0);
    QCOMPARE(spy.count(), 1);

    engine.unfold(0);
    QCOMPARE(spy.count(), 2);

    engine.fold(0);
    engine.unfoldAll();
    QCOMPARE(spy.count(), 4);
}

void TestCodeFolding::testEmptyDocument()
{
    QTextDocument doc;
    doc.setPlainText(QString());

    CodeFoldingEngine engine;
    engine.setDocument(&doc);
    engine.setLanguageId(QStringLiteral("cpp"));
    engine.update();

    QCOMPARE(engine.regions().size(), 0);
    QVERIFY(!engine.isFoldable(0));
    QVERIFY(!engine.toggleFold(0));
}

QTEST_MAIN(TestCodeFolding)
#include "test_codefolding.moc"
