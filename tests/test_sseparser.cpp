#include <QTest>
#include <QSignalSpy>
#include "sseparser.h"

class TestSseParser : public QObject
{
    Q_OBJECT

private slots:
    void singleEvent()
    {
        SseParser parser;
        QSignalSpy spy(&parser, &SseParser::eventReceived);

        parser.feed("data: {\"text\":\"hello\"}\n\n");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("{\"text\":\"hello\"}"));
    }

    void eventWithType()
    {
        SseParser parser;
        QSignalSpy spy(&parser, &SseParser::eventReceived);

        parser.feed("event: delta\ndata: chunk\n\n");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("delta"));
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("chunk"));
    }

    void multipleEvents()
    {
        SseParser parser;
        QSignalSpy spy(&parser, &SseParser::eventReceived);

        parser.feed("data: first\n\ndata: second\n\n");

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("first"));
        QCOMPARE(spy.at(1).at(1).toString(), QStringLiteral("second"));
    }

    void chunkedDelivery()
    {
        SseParser parser;
        QSignalSpy spy(&parser, &SseParser::eventReceived);

        parser.feed("data: hel");
        QCOMPARE(spy.count(), 0);

        parser.feed("lo\n\n");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("hello"));
    }

    void doneSignal()
    {
        SseParser parser;
        QSignalSpy doneSpy(&parser, &SseParser::done);

        parser.feed("data: [DONE]\n\n");

        QCOMPARE(doneSpy.count(), 1);
    }

    void multiLineData()
    {
        SseParser parser;
        QSignalSpy spy(&parser, &SseParser::eventReceived);

        parser.feed("data: line1\ndata: line2\n\n");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("line1\nline2"));
    }

    void resetClearsState()
    {
        SseParser parser;
        QSignalSpy spy(&parser, &SseParser::eventReceived);

        parser.feed("data: partial");
        parser.reset();
        parser.feed("data: fresh\n\n");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("fresh"));
    }

    void carriageReturnHandling()
    {
        SseParser parser;
        QSignalSpy spy(&parser, &SseParser::eventReceived);

        parser.feed("data: hello\r\n\r\n");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("hello"));
    }
};

QTEST_MAIN(TestSseParser)
#include "test_sseparser.moc"
