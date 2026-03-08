#include <QTest>
#include "piecetablebuffer.h"

class TestPieceTable : public QObject
{
    Q_OBJECT

private slots:
    void emptyBuffer()
    {
        PieceTableBuffer buf;
        QCOMPARE(buf.length(), 0);
        QCOMPARE(buf.text(), QString());
    }

    void initialContent()
    {
        PieceTableBuffer buf(QStringLiteral("Hello"));
        QCOMPARE(buf.length(), 5);
        QCOMPARE(buf.text(), QStringLiteral("Hello"));
    }

    void insertAtBeginning()
    {
        PieceTableBuffer buf(QStringLiteral("World"));
        buf.insert(0, QStringLiteral("Hello "));
        QCOMPARE(buf.text(), QStringLiteral("Hello World"));
    }

    void insertAtEnd()
    {
        PieceTableBuffer buf(QStringLiteral("Hello"));
        buf.insert(5, QStringLiteral(" World"));
        QCOMPARE(buf.text(), QStringLiteral("Hello World"));
    }

    void insertInMiddle()
    {
        PieceTableBuffer buf(QStringLiteral("Hllo"));
        buf.insert(1, QStringLiteral("e"));
        QCOMPARE(buf.text(), QStringLiteral("Hello"));
    }

    void removeFromBeginning()
    {
        PieceTableBuffer buf(QStringLiteral("Hello World"));
        buf.remove(0, 6);
        QCOMPARE(buf.text(), QStringLiteral("World"));
    }

    void removeFromEnd()
    {
        PieceTableBuffer buf(QStringLiteral("Hello World"));
        buf.remove(5, 6);
        QCOMPARE(buf.text(), QStringLiteral("Hello"));
    }

    void removeFromMiddle()
    {
        PieceTableBuffer buf(QStringLiteral("Hello World"));
        buf.remove(5, 1);
        QCOMPARE(buf.text(), QStringLiteral("HelloWorld"));
    }

    void sliceSubstring()
    {
        PieceTableBuffer buf(QStringLiteral("Hello World"));
        QCOMPARE(buf.slice(6, 5), QStringLiteral("World"));
        QCOMPARE(buf.slice(0, 5), QStringLiteral("Hello"));
    }

    void multipleEdits()
    {
        PieceTableBuffer buf(QStringLiteral("abcdef"));
        buf.insert(3, QStringLiteral("XY"));      // abc XY def
        QCOMPARE(buf.text(), QStringLiteral("abcXYdef"));
        buf.remove(1, 2);                          // a XY def
        QCOMPARE(buf.text(), QStringLiteral("aXYdef"));
        buf.insert(0, QStringLiteral("Z"));        // Z a XYdef
        QCOMPARE(buf.text(), QStringLiteral("ZaXYdef"));
        QCOMPARE(buf.length(), 7);
    }

    void insertEmpty()
    {
        PieceTableBuffer buf(QStringLiteral("test"));
        buf.insert(2, QString());
        QCOMPARE(buf.text(), QStringLiteral("test"));
    }

    void removeZeroLength()
    {
        PieceTableBuffer buf(QStringLiteral("test"));
        buf.remove(1, 0);
        QCOMPARE(buf.text(), QStringLiteral("test"));
    }

    void removeBeyondEnd()
    {
        PieceTableBuffer buf(QStringLiteral("test"));
        buf.remove(2, 100);
        QCOMPARE(buf.text(), QStringLiteral("te"));
    }
};

QTEST_MAIN(TestPieceTable)
#include "test_piecetable.moc"
