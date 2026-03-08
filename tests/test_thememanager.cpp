#include <QTest>
#include <QTemporaryFile>
#include <QTextStream>
#include "thememanager.h"

class TestThemeManager : public QObject
{
    Q_OBJECT

private slots:
    void defaultThemeIsDark()
    {
        ThemeManager tm;
        QCOMPARE(tm.currentTheme(), ThemeManager::Dark);
    }

    void toggleSwitchesTheme()
    {
        ThemeManager tm;
        tm.toggle();
        QCOMPARE(tm.currentTheme(), ThemeManager::Light);
        tm.toggle();
        QCOMPARE(tm.currentTheme(), ThemeManager::Dark);
    }

    void loadCustomThemeFromJson()
    {
        ThemeManager tm;

        QTemporaryFile f;
        f.setAutoRemove(true);
        QVERIFY(f.open());
        {
            QTextStream out(&f);
            out << R"({
                "window": "#2d2d30",
                "text": "#ffffff",
                "highlight": "#ff0000"
            })";
        }
        f.close();

        QString err;
        QVERIFY(tm.loadCustomTheme(f.fileName(), &err));
        QVERIFY(err.isEmpty());

        tm.setTheme(ThemeManager::Custom);
        QCOMPARE(tm.currentTheme(), ThemeManager::Custom);
    }

    void customThemeInvalidJson()
    {
        ThemeManager tm;

        QTemporaryFile f;
        f.setAutoRemove(true);
        QVERIFY(f.open());
        {
            QTextStream out(&f);
            out << "not json {{{";
        }
        f.close();

        QString err;
        QVERIFY(!tm.loadCustomTheme(f.fileName(), &err));
        QVERIFY(!err.isEmpty());
    }

    void customThemeNonexistentFile()
    {
        ThemeManager tm;
        QString err;
        QVERIFY(!tm.loadCustomTheme(QStringLiteral("/nonexistent/path.json"), &err));
        QVERIFY(!err.isEmpty());
    }

    void setCustomWithoutLoadReturnsEarly()
    {
        ThemeManager tm;
        tm.setTheme(ThemeManager::Custom); // no custom loaded
        QCOMPARE(tm.currentTheme(), ThemeManager::Dark); // unchanged
    }

    void themeChangedSignal()
    {
        ThemeManager tm;
        int count = 0;
        connect(&tm, &ThemeManager::themeChanged, [&count](ThemeManager::Theme) { ++count; });
        tm.setTheme(ThemeManager::Light);
        QCOMPARE(count, 1);
        tm.setTheme(ThemeManager::Light); // same, no signal
        QCOMPARE(count, 1);
    }
};

QTEST_MAIN(TestThemeManager)
#include "test_thememanager.moc"
