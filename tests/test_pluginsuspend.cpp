#include <QTest>
#include "plugin/pluginmanifest.h"

// ── Profile management logic tests ──────────────────────────────────────────
//
// PluginManager has heavy dependencies (LuaJIT, CABI bridge, ContributionRegistry)
// that make standalone compilation impractical. Instead we test:
// 1. PluginManifest classification (isLanguagePlugin/isGeneralPlugin)
// 2. Profile gating logic (mirrors PluginManager::isPluginAllowedByProfile)
// 3. Active profile set management (workspace vs editor-level)
// 4. Suspend/resume state transitions
//
// Integration testing of actual plugin suspend/resume is covered by the
// application-level build+run tests.

// ── Lightweight mirror of PluginManager profile logic ────────────────────────

class ProfileManager
{
public:
    void setActiveLanguageProfiles(const QSet<QString> &ids)
    {
        m_activeProfiles = ids;
        m_workspaceProfiles = ids;
    }

    QSet<QString> activeLanguageProfiles() const { return m_activeProfiles; }

    void addWorkspaceLanguage(const QString &id)
    {
        m_workspaceProfiles.insert(id);
        m_activeProfiles.insert(id);
    }

    bool isPluginAllowedByProfile(const PluginManifest &manifest) const
    {
        if (manifest.isGeneralPlugin())
            return true;
        for (const QString &lid : manifest.languageIds) {
            if (m_activeProfiles.contains(lid))
                return true;
        }
        return false;
    }

    void switchActiveLanguage(const QString &newLang)
    {
        if (newLang == m_activeEditorLanguage)
            return;
        const QString prev = m_activeEditorLanguage;
        m_activeEditorLanguage = newLang;

        // Remove previous if not workspace-level
        if (!prev.isEmpty() && !m_workspaceProfiles.contains(prev))
            m_activeProfiles.remove(prev);

        if (!newLang.isEmpty())
            m_activeProfiles.insert(newLang);
    }

    QString activeEditorLanguage() const { return m_activeEditorLanguage; }

    // Track suspend/resume state for testing
    struct PluginState {
        PluginManifest manifest;
        bool suspended = false;
    };

    void addPlugin(const PluginManifest &m) { m_plugins.append({m, false}); }

    int suspendByLanguageProfile(const QString &langId)
    {
        if (langId.isEmpty()) return 0;
        int count = 0;
        for (auto &ps : m_plugins) {
            if (ps.suspended || !ps.manifest.isLanguagePlugin())
                continue;
            if (!ps.manifest.languageIds.contains(langId))
                continue;
            // Don't suspend if served by another active profile
            bool other = false;
            for (const QString &lid : ps.manifest.languageIds) {
                if (lid != langId && m_activeProfiles.contains(lid)) {
                    other = true;
                    break;
                }
            }
            if (other) continue;
            ps.suspended = true;
            ++count;
        }
        return count;
    }

    int resumeByLanguageProfile(const QString &langId)
    {
        if (langId.isEmpty()) return 0;
        int count = 0;
        for (auto &ps : m_plugins) {
            if (!ps.suspended || !ps.manifest.isLanguagePlugin())
                continue;
            if (!ps.manifest.languageIds.contains(langId))
                continue;
            ps.suspended = false;
            ++count;
        }
        return count;
    }

    bool isPluginSuspended(int idx) const { return m_plugins[idx].suspended; }
    int pluginCount() const { return m_plugins.size(); }

private:
    QSet<QString> m_activeProfiles;
    QSet<QString> m_workspaceProfiles;
    QString m_activeEditorLanguage;
    QVector<PluginState> m_plugins;
};

class TestPluginSuspend : public QObject
{
    Q_OBJECT

private slots:
    void profileGating_generalAlwaysAllowed()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.category = QStringLiteral("ai");
        QVERIFY(m.isGeneralPlugin());
        QVERIFY(mgr.isPluginAllowedByProfile(m));
    }

    void profileGating_languageNeedsActiveProfile()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.category = QStringLiteral("language");
        m.languageIds = {QStringLiteral("cpp"), QStringLiteral("c")};

        QVERIFY(m.isLanguagePlugin());
        QVERIFY(!mgr.isPluginAllowedByProfile(m));

        mgr.setActiveLanguageProfiles({QStringLiteral("cpp")});
        QVERIFY(mgr.isPluginAllowedByProfile(m));
    }

    void profileGating_partialMatch()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.languageIds = {QStringLiteral("rust")};

        mgr.setActiveLanguageProfiles({QStringLiteral("cpp"), QStringLiteral("python")});
        QVERIFY(!mgr.isPluginAllowedByProfile(m));

        mgr.setActiveLanguageProfiles({QStringLiteral("rust")});
        QVERIFY(mgr.isPluginAllowedByProfile(m));
    }

    void workspaceLanguage_notSuspendedOnSwitch()
    {
        ProfileManager mgr;
        mgr.addWorkspaceLanguage(QStringLiteral("cpp"));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("cpp")));

        mgr.switchActiveLanguage(QStringLiteral("python"));
        QCOMPARE(mgr.activeEditorLanguage(), QStringLiteral("python"));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("cpp")));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("python")));
    }

    void switchActiveLanguage_removesNonWorkspaceLanguage()
    {
        ProfileManager mgr;
        mgr.switchActiveLanguage(QStringLiteral("python"));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("python")));

        mgr.switchActiveLanguage(QStringLiteral("rust"));
        QVERIFY(!mgr.activeLanguageProfiles().contains(QStringLiteral("python")));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("rust")));
    }

    void switchActiveLanguage_sameLanguageNoop()
    {
        ProfileManager mgr;
        mgr.switchActiveLanguage(QStringLiteral("cpp"));
        QCOMPARE(mgr.activeEditorLanguage(), QStringLiteral("cpp"));
        mgr.switchActiveLanguage(QStringLiteral("cpp"));
        QCOMPARE(mgr.activeEditorLanguage(), QStringLiteral("cpp"));
    }

    void suspendByProfile_empty_returnsZero()
    {
        ProfileManager mgr;
        QCOMPARE(mgr.suspendByLanguageProfile(QString()), 0);
    }

    void resumeByProfile_empty_returnsZero()
    {
        ProfileManager mgr;
        QCOMPARE(mgr.resumeByLanguageProfile(QString()), 0);
    }

    void setActiveProfiles_setsWorkspaceProfiles()
    {
        ProfileManager mgr;
        mgr.setActiveLanguageProfiles({QStringLiteral("cpp"), QStringLiteral("python")});
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("cpp")));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("python")));

        mgr.switchActiveLanguage(QStringLiteral("rust"));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("cpp")));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("python")));
        QVERIFY(mgr.activeLanguageProfiles().contains(QStringLiteral("rust")));
    }

    // ── Plugin suspend/resume state tests ─────────────────────────────

    void suspendPlugin_languagePlugin()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.languageIds = {QStringLiteral("rust")};
        m.category = QStringLiteral("language");
        mgr.addPlugin(m);

        mgr.addWorkspaceLanguage(QStringLiteral("rust"));
        QVERIFY(!mgr.isPluginSuspended(0));

        // Rust is workspace-level, but we can still explicitly suspend
        QCOMPARE(mgr.suspendByLanguageProfile(QStringLiteral("rust")), 1);
        QVERIFY(mgr.isPluginSuspended(0));

        // Resume
        QCOMPARE(mgr.resumeByLanguageProfile(QStringLiteral("rust")), 1);
        QVERIFY(!mgr.isPluginSuspended(0));
    }

    void suspendPlugin_generalNeverSuspended()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.category = QStringLiteral("ai");
        // No languageIds → general plugin
        mgr.addPlugin(m);

        QCOMPARE(mgr.suspendByLanguageProfile(QStringLiteral("cpp")), 0);
        QVERIFY(!mgr.isPluginSuspended(0));
    }

    void suspendPlugin_multiLangNotSuspendedIfOtherActive()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.languageIds = {QStringLiteral("cpp"), QStringLiteral("c")};
        m.category = QStringLiteral("language");
        mgr.addPlugin(m);

        // Both cpp and c are active
        mgr.setActiveLanguageProfiles({QStringLiteral("cpp"), QStringLiteral("c")});
        QVERIFY(!mgr.isPluginSuspended(0));

        // Suspend cpp — but c is still active → plugin stays active
        QCOMPARE(mgr.suspendByLanguageProfile(QStringLiteral("cpp")), 0);
        QVERIFY(!mgr.isPluginSuspended(0));
    }

    void suspendPlugin_multiLangSuspendedIfNoneActive()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.languageIds = {QStringLiteral("cpp"), QStringLiteral("c")};
        m.category = QStringLiteral("language");
        mgr.addPlugin(m);

        // Only cpp active, suspend cpp → no other profile active → suspend
        mgr.setActiveLanguageProfiles({QStringLiteral("cpp")});
        QCOMPARE(mgr.suspendByLanguageProfile(QStringLiteral("cpp")), 1);
        QVERIFY(mgr.isPluginSuspended(0));
    }

    void suspendResume_alreadySuspendedSkipped()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.languageIds = {QStringLiteral("python")};
        m.category = QStringLiteral("language");
        mgr.addPlugin(m);

        QCOMPARE(mgr.suspendByLanguageProfile(QStringLiteral("python")), 1);
        // Already suspended — should return 0
        QCOMPARE(mgr.suspendByLanguageProfile(QStringLiteral("python")), 0);
    }

    void resumePlugin_notSuspendedSkipped()
    {
        ProfileManager mgr;
        PluginManifest m;
        m.languageIds = {QStringLiteral("go")};
        m.category = QStringLiteral("language");
        mgr.addPlugin(m);

        // Not suspended — resume should return 0
        QCOMPARE(mgr.resumeByLanguageProfile(QStringLiteral("go")), 0);
    }
};

QTEST_MAIN(TestPluginSuspend)
#include "test_pluginsuspend.moc"
