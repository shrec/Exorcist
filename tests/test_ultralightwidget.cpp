#include <QTest>

#ifdef EXORCIST_HAS_ULTRALIGHT

#include <QTimer>

#include "agent/chat/ultralight/ultralightwidget.h"

class TestUltralightWidget : public QObject
{
    Q_OBJECT

private slots:
    void hiddenLoadDoesNotStartTick();
    void showingWidgetStartsTick();
};

void TestUltralightWidget::hiddenLoadDoesNotStartTick()
{
    exorcist::UltralightWidget widget;

    auto *tickTimer = widget.findChild<QTimer *>();
    QVERIFY(tickTimer != nullptr);
    QVERIFY(!widget.isVisible());

    widget.loadURL(QStringLiteral("about:blank"));

    QVERIFY(!tickTimer->isActive());
}

void TestUltralightWidget::showingWidgetStartsTick()
{
    exorcist::UltralightWidget widget;

    auto *tickTimer = widget.findChild<QTimer *>();
    QVERIFY(tickTimer != nullptr);

    widget.resize(320, 240);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    widget.loadURL(QStringLiteral("about:blank"));

    QTRY_VERIFY(tickTimer->isActive());

    widget.hide();
    QTRY_VERIFY(!tickTimer->isActive());
}

#else

class TestUltralightWidget : public QObject
{
    Q_OBJECT

private slots:
    void ultralightDisabled();
};

void TestUltralightWidget::ultralightDisabled()
{
    QSKIP("Ultralight support is disabled in this build.");
}

#endif

QTEST_MAIN(TestUltralightWidget)
#include "test_ultralightwidget.moc"