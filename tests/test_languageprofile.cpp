#include <QTest>
#include <QSettings>
#include <QSignalSpy>
#include "settings/languageprofile.h"

class TestLanguageProfile : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        // Clear any saved profiles before each test
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        s.remove(QStringLiteral("LanguageProfiles"));
    }

    void defaultState()
    {
        LanguageProfileManager mgr;
        QVERIFY(mgr.configuredLanguages().isEmpty());
    }

    void setAndGetProfile()
    {
        LanguageProfileManager mgr;
        LanguageProfileData d;
        d.tabSize = 2;
        d.useTabs = true;
        d.useTabsSet = true;
        d.trimWhitespace = true;
        d.trimSet = true;
        d.formatOnSave = true;
        d.formatOnSaveSet = true;
        d.eolMode = QStringLiteral("lf");

        mgr.setProfile(QStringLiteral("python"), d);
        QCOMPARE(mgr.configuredLanguages().size(), 1);
        QVERIFY(mgr.configuredLanguages().contains(QStringLiteral("python")));

        const auto got = mgr.profile(QStringLiteral("python"));
        QCOMPARE(got.tabSize, 2);
        QCOMPARE(got.useTabs, true);
        QCOMPARE(got.trimWhitespace, true);
        QCOMPARE(got.formatOnSave, true);
        QCOMPARE(got.eolMode, QStringLiteral("lf"));
    }

    void tabSizeFallback()
    {
        LanguageProfileManager mgr;
        // No profile → return global default
        QCOMPARE(mgr.tabSize(QStringLiteral("rust"), 4), 4);

        // Profile with tabSize -1 → return global default
        LanguageProfileData d;
        d.tabSize = -1;
        mgr.setProfile(QStringLiteral("rust"), d);
        QCOMPARE(mgr.tabSize(QStringLiteral("rust"), 4), 4);

        // Profile with specific value → override
        d.tabSize = 2;
        mgr.setProfile(QStringLiteral("rust"), d);
        QCOMPARE(mgr.tabSize(QStringLiteral("rust"), 4), 2);
    }

    void useTabsFallback()
    {
        LanguageProfileManager mgr;
        QCOMPARE(mgr.useTabs(QStringLiteral("go"), false), false);
        QCOMPARE(mgr.useTabs(QStringLiteral("go"), true), true);

        LanguageProfileData d;
        d.useTabs = true;
        d.useTabsSet = true;
        mgr.setProfile(QStringLiteral("go"), d);
        QCOMPARE(mgr.useTabs(QStringLiteral("go"), false), true);
    }

    void removeProfile()
    {
        LanguageProfileManager mgr;
        LanguageProfileData d;
        d.tabSize = 8;
        mgr.setProfile(QStringLiteral("cpp"), d);
        QCOMPARE(mgr.configuredLanguages().size(), 1);

        mgr.removeProfile(QStringLiteral("cpp"));
        QVERIFY(mgr.configuredLanguages().isEmpty());
        QCOMPARE(mgr.tabSize(QStringLiteral("cpp"), 4), 4);
    }

    void profileChangedSignal()
    {
        LanguageProfileManager mgr;
        QSignalSpy spy(&mgr, &LanguageProfileManager::profileChanged);

        LanguageProfileData d;
        d.tabSize = 2;
        mgr.setProfile(QStringLiteral("js"), d);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("js"));

        mgr.removeProfile(QStringLiteral("js"));
        QCOMPARE(spy.count(), 2);
    }

    void persistsAcrossInstances()
    {
        {
            LanguageProfileManager mgr;
            LanguageProfileData d;
            d.tabSize = 3;
            d.useTabs = true;
            d.useTabsSet = true;
            d.formatOnSave = true;
            d.formatOnSaveSet = true;
            d.eolMode = QStringLiteral("crlf");
            mgr.setProfile(QStringLiteral("typescript"), d);
        }
        // New instance should load persisted data
        LanguageProfileManager mgr2;
        QVERIFY(mgr2.configuredLanguages().contains(QStringLiteral("typescript")));
        QCOMPARE(mgr2.tabSize(QStringLiteral("typescript"), 4), 3);
        QCOMPARE(mgr2.useTabs(QStringLiteral("typescript"), false), true);
        QCOMPARE(mgr2.formatOnSave(QStringLiteral("typescript")), true);
        QCOMPARE(mgr2.eolMode(QStringLiteral("typescript")), QStringLiteral("crlf"));
    }

    void trimWhitespace_defaultFalse()
    {
        LanguageProfileManager mgr;
        QCOMPARE(mgr.trimTrailingWhitespace(QStringLiteral("unknown")), false);
    }

    void formatOnSave_defaultFalse()
    {
        LanguageProfileManager mgr;
        QCOMPARE(mgr.formatOnSave(QStringLiteral("unknown")), false);
    }

    void multipleLanguages()
    {
        LanguageProfileManager mgr;
        LanguageProfileData py;
        py.tabSize = 4;
        mgr.setProfile(QStringLiteral("python"), py);

        LanguageProfileData go;
        go.tabSize = 8;
        go.useTabs = true;
        go.useTabsSet = true;
        mgr.setProfile(QStringLiteral("go"), go);

        QCOMPARE(mgr.configuredLanguages().size(), 2);
        QCOMPARE(mgr.tabSize(QStringLiteral("python"), 2), 4);
        QCOMPARE(mgr.tabSize(QStringLiteral("go"), 2), 8);
        QCOMPARE(mgr.useTabs(QStringLiteral("go"), false), true);
        QCOMPARE(mgr.useTabs(QStringLiteral("python"), false), false);
    }

    void cleanup()
    {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        s.remove(QStringLiteral("LanguageProfiles"));
    }
};

QTEST_MAIN(TestLanguageProfile)
#include "test_languageprofile.moc"
