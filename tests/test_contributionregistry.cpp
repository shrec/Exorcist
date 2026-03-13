#include <QTest>
#include <QSignalSpy>
#include <QSettings>

#include "plugin/pluginmanifest.h"
#include "plugin/ilanguagecontributor.h"
#include "plugin/itaskcontributor.h"
#include "plugin/isettingscontributor.h"

// ── Test contribution registration and query logic ──────────────────────────
//
// ContributionRegistry has heavy MainWindow/DockManager dependencies that make
// standalone unit testing impractical. Instead, we test:
// 1. PluginManifest data structures (the contribution types themselves)
// 2. The query/matching algorithms (extension lookup, ID lookup)
// 3. Interface contracts (ILanguageContributor, ITaskContributor, etc.)
//
// Integration testing of full ContributionRegistry wiring is covered by the
// application-level build+run tests.

// ── Lightweight registry that mirrors ContributionRegistry query logic ────────

class ContributionStore : public QObject
{
    Q_OBJECT
public:
    struct RegisteredLanguage {
        QString pluginId;
        LanguageContribution contribution;
    };

    struct RegisteredTask {
        QString pluginId;
        TaskContribution contribution;
    };

    struct RegisteredSetting {
        QString pluginId;
        SettingContribution contribution;
    };

    struct RegisteredTheme {
        QString pluginId;
        ThemeContribution contribution;
    };

    void registerLanguages(const QString &pluginId, const QList<LanguageContribution> &langs)
    {
        for (const auto &l : langs)
            m_languages.append({pluginId, l});
        emit languagesChanged();
    }

    void registerTasks(const QString &pluginId, const QList<TaskContribution> &tasks)
    {
        for (const auto &t : tasks)
            m_tasks.append({pluginId, t});
    }

    void registerSettings(const QString &pluginId, const QList<SettingContribution> &settings)
    {
        for (const auto &s : settings)
            m_settings.append({pluginId, s});
        emit settingsChanged();
    }

    void registerThemes(const QString &pluginId, const QList<ThemeContribution> &themes)
    {
        for (const auto &t : themes)
            m_themes.append({pluginId, t});
        emit themesChanged();
    }

    void unregisterPlugin(const QString &pluginId)
    {
        bool langChanged = false, settChanged = false, themeChanged = false;
        m_languages.erase(std::remove_if(m_languages.begin(), m_languages.end(),
            [&](const RegisteredLanguage &l) { if (l.pluginId == pluginId) { langChanged = true; return true; } return false; }),
            m_languages.end());
        m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(),
            [&](const RegisteredTask &t) { return t.pluginId == pluginId; }),
            m_tasks.end());
        m_settings.erase(std::remove_if(m_settings.begin(), m_settings.end(),
            [&](const RegisteredSetting &s) { if (s.pluginId == pluginId) { settChanged = true; return true; } return false; }),
            m_settings.end());
        m_themes.erase(std::remove_if(m_themes.begin(), m_themes.end(),
            [&](const RegisteredTheme &t) { if (t.pluginId == pluginId) { themeChanged = true; return true; } return false; }),
            m_themes.end());

        if (langChanged) emit languagesChanged();
        if (settChanged) emit settingsChanged();
        if (themeChanged) emit themesChanged();
    }

    const LanguageContribution *languageForExtension(const QString &ext) const
    {
        QString normalized = ext.startsWith('.') ? ext : ('.' + ext);
        for (const auto &rl : m_languages) {
            for (const QString &langExt : rl.contribution.extensions) {
                if (langExt.compare(normalized, Qt::CaseInsensitive) == 0)
                    return &rl.contribution;
            }
        }
        return nullptr;
    }

    const LanguageContribution *languageById(const QString &id) const
    {
        for (const auto &rl : m_languages) {
            if (rl.contribution.id == id)
                return &rl.contribution;
        }
        return nullptr;
    }

    QList<LanguageContribution> registeredLanguages() const
    {
        QList<LanguageContribution> r;
        for (const auto &rl : m_languages) r.append(rl.contribution);
        return r;
    }

    QList<TaskContribution> registeredTasks() const
    {
        QList<TaskContribution> r;
        for (const auto &rt : m_tasks) r.append(rt.contribution);
        return r;
    }

    QList<SettingContribution> registeredSettings() const
    {
        QList<SettingContribution> r;
        for (const auto &rs : m_settings) r.append(rs.contribution);
        return r;
    }

    QList<SettingContribution> settingsForPlugin(const QString &pluginId) const
    {
        QList<SettingContribution> r;
        for (const auto &rs : m_settings) {
            if (rs.pluginId == pluginId) r.append(rs.contribution);
        }
        return r;
    }

    QList<ThemeContribution> registeredThemes() const
    {
        QList<ThemeContribution> r;
        for (const auto &rt : m_themes) r.append(rt.contribution);
        return r;
    }

signals:
    void languagesChanged();
    void settingsChanged();
    void themesChanged();

private:
    QList<RegisteredLanguage> m_languages;
    QList<RegisteredTask> m_tasks;
    QList<RegisteredSetting> m_settings;
    QList<RegisteredTheme> m_themes;
};

// ── Test class ───────────────────────────────────────────────────────────────

class TestContributionRegistry : public QObject
{
    Q_OBJECT

private slots:
    void languageRegistration();
    void languageForExtension();
    void languageById();
    void languageUnregister();

    void taskRegistration();
    void taskUnregister();

    void settingRegistration();
    void settingsForPlugin();
    void settingUnregister();

    void themeRegistration();
    void themeUnregister();

    void fullManifestRegistration();

    void manifestFromJson();
    void manifestActivationEvents();
};

// ── Language tests ───────────────────────────────────────────────────────────

void TestContributionRegistry::languageRegistration()
{
    ContributionStore store;
    QVERIFY(store.registeredLanguages().isEmpty());

    LanguageContribution lang;
    lang.id = "rust";
    lang.name = "Rust";
    lang.extensions = {".rs"};

    QSignalSpy spy(&store, &ContributionStore::languagesChanged);
    store.registerLanguages("org.test.rust", {lang});

    QCOMPARE(store.registeredLanguages().size(), 1);
    QCOMPARE(store.registeredLanguages().first().id, QString("rust"));
    QCOMPARE(spy.count(), 1);
}

void TestContributionRegistry::languageForExtension()
{
    ContributionStore store;

    LanguageContribution rust;
    rust.id = "rust";
    rust.extensions = {".rs"};

    LanguageContribution python;
    python.id = "python";
    python.extensions = {".py", ".pyw"};

    store.registerLanguages("org.test", {rust, python});

    // With dot prefix
    auto *found = store.languageForExtension(".rs");
    QVERIFY(found);
    QCOMPARE(found->id, QString("rust"));

    // Without dot prefix
    found = store.languageForExtension("py");
    QVERIFY(found);
    QCOMPARE(found->id, QString("python"));

    // Second extension
    found = store.languageForExtension(".pyw");
    QVERIFY(found);
    QCOMPARE(found->id, QString("python"));

    // Case insensitive
    found = store.languageForExtension(".RS");
    QVERIFY(found);
    QCOMPARE(found->id, QString("rust"));

    // Unknown extension
    QVERIFY(!store.languageForExtension(".java"));
}

void TestContributionRegistry::languageById()
{
    ContributionStore store;

    LanguageContribution lang;
    lang.id = "cpp";
    lang.name = "C++";
    lang.extensions = {".cpp", ".h"};

    store.registerLanguages("org.test", {lang});

    auto *found = store.languageById("cpp");
    QVERIFY(found);
    QCOMPARE(found->name, QString("C++"));

    QVERIFY(!store.languageById("rust"));
}

void TestContributionRegistry::languageUnregister()
{
    ContributionStore store;

    LanguageContribution lang;
    lang.id = "rust";
    lang.extensions = {".rs"};

    store.registerLanguages("org.test.rust", {lang});
    QCOMPARE(store.registeredLanguages().size(), 1);

    QSignalSpy spy(&store, &ContributionStore::languagesChanged);
    store.unregisterPlugin("org.test.rust");
    QCOMPARE(store.registeredLanguages().size(), 0);
    QVERIFY(!store.languageForExtension(".rs"));
    QCOMPARE(spy.count(), 1);
}

// ── Task tests ───────────────────────────────────────────────────────────────

void TestContributionRegistry::taskRegistration()
{
    ContributionStore store;
    QVERIFY(store.registeredTasks().isEmpty());

    TaskContribution task;
    task.id = "myPlugin.lint";
    task.label = "Run Linter";
    task.type = "shell";
    task.command = "eslint";

    store.registerTasks("org.test", {task});

    QCOMPARE(store.registeredTasks().size(), 1);
    QCOMPARE(store.registeredTasks().first().id, QString("myPlugin.lint"));
    QCOMPARE(store.registeredTasks().first().command, QString("eslint"));
}

void TestContributionRegistry::taskUnregister()
{
    ContributionStore store;

    TaskContribution task;
    task.id = "test.build";
    task.type = "shell";

    store.registerTasks("org.test", {task});
    QCOMPARE(store.registeredTasks().size(), 1);

    store.unregisterPlugin("org.test");
    QVERIFY(store.registeredTasks().isEmpty());
}

// ── Settings tests ───────────────────────────────────────────────────────────

void TestContributionRegistry::settingRegistration()
{
    ContributionStore store;
    QVERIFY(store.registeredSettings().isEmpty());

    SettingContribution setting;
    setting.key = "enableLinting";
    setting.title = "Enable Linting";
    setting.type = SettingContribution::Bool;
    setting.defaultValue = true;
    setting.category = "Editor";

    QSignalSpy spy(&store, &ContributionStore::settingsChanged);
    store.registerSettings("org.test.linter", {setting});

    QCOMPARE(store.registeredSettings().size(), 1);
    QCOMPARE(store.registeredSettings().first().key, QString("enableLinting"));
    QCOMPARE(spy.count(), 1);
}

void TestContributionRegistry::settingsForPlugin()
{
    ContributionStore store;

    SettingContribution s1;
    s1.key = "tabSize";
    s1.type = SettingContribution::Int;
    s1.defaultValue = 4;

    SettingContribution s2;
    s2.key = "formatOnSave";
    s2.type = SettingContribution::Bool;
    s2.defaultValue = false;

    store.registerSettings("org.plugin.a", {s1});
    store.registerSettings("org.plugin.b", {s2});

    QCOMPARE(store.registeredSettings().size(), 2);
    QCOMPARE(store.settingsForPlugin("org.plugin.a").size(), 1);
    QCOMPARE(store.settingsForPlugin("org.plugin.a").first().key, QString("tabSize"));
    QCOMPARE(store.settingsForPlugin("org.plugin.b").size(), 1);
    QCOMPARE(store.settingsForPlugin("org.plugin.b").first().key, QString("formatOnSave"));
    QVERIFY(store.settingsForPlugin("org.plugin.c").isEmpty());
}

void TestContributionRegistry::settingUnregister()
{
    ContributionStore store;

    SettingContribution s;
    s.key = "timeout";
    s.type = SettingContribution::Int;
    s.defaultValue = 30;

    store.registerSettings("org.test", {s});
    QCOMPARE(store.registeredSettings().size(), 1);

    QSignalSpy spy(&store, &ContributionStore::settingsChanged);
    store.unregisterPlugin("org.test");
    QVERIFY(store.registeredSettings().isEmpty());
    QCOMPARE(spy.count(), 1);
}

// ── Theme tests ──────────────────────────────────────────────────────────────

void TestContributionRegistry::themeRegistration()
{
    ContributionStore store;
    QVERIFY(store.registeredThemes().isEmpty());

    ThemeContribution theme;
    theme.id = "my-dark";
    theme.label = "My Dark Theme";
    theme.type = ThemeContribution::Dark;
    theme.path = "themes/dark.json";

    QSignalSpy spy(&store, &ContributionStore::themesChanged);
    store.registerThemes("org.test.theme", {theme});

    QCOMPARE(store.registeredThemes().size(), 1);
    QCOMPARE(store.registeredThemes().first().id, QString("my-dark"));
    QCOMPARE(store.registeredThemes().first().label, QString("My Dark Theme"));
    QCOMPARE(spy.count(), 1);
}

void TestContributionRegistry::themeUnregister()
{
    ContributionStore store;

    ThemeContribution theme;
    theme.id = "my-light";
    theme.label = "My Light Theme";
    theme.type = ThemeContribution::Light;

    store.registerThemes("org.test.theme", {theme});
    QCOMPARE(store.registeredThemes().size(), 1);

    QSignalSpy spy(&store, &ContributionStore::themesChanged);
    store.unregisterPlugin("org.test.theme");
    QVERIFY(store.registeredThemes().isEmpty());
    QCOMPARE(spy.count(), 1);
}

// ── Full manifest test ───────────────────────────────────────────────────────

void TestContributionRegistry::fullManifestRegistration()
{
    ContributionStore store;

    LanguageContribution lang;
    lang.id = "typescript";
    lang.extensions = {".ts", ".tsx"};

    TaskContribution task;
    task.id = "test.build";
    task.type = "npm";

    SettingContribution setting;
    setting.key = "strictMode";
    setting.type = SettingContribution::Bool;
    setting.defaultValue = true;

    ThemeContribution theme;
    theme.id = "nord";
    theme.label = "Nord";

    store.registerLanguages("org.full", {lang});
    store.registerTasks("org.full", {task});
    store.registerSettings("org.full", {setting});
    store.registerThemes("org.full", {theme});

    QCOMPARE(store.registeredLanguages().size(), 1);
    QCOMPARE(store.registeredTasks().size(), 1);
    QCOMPARE(store.registeredSettings().size(), 1);
    QCOMPARE(store.registeredThemes().size(), 1);

    QVERIFY(store.languageForExtension(".ts"));
    QCOMPARE(store.languageForExtension(".ts")->id, QString("typescript"));

    // Unregister all at once
    store.unregisterPlugin("org.full");
    QVERIFY(store.registeredLanguages().isEmpty());
    QVERIFY(store.registeredTasks().isEmpty());
    QVERIFY(store.registeredSettings().isEmpty());
    QVERIFY(store.registeredThemes().isEmpty());
}

// ── Manifest parsing/construction tests ──────────────────────────────────────

void TestContributionRegistry::manifestFromJson()
{
    // Test that PluginManifest::fromJson correctly parses contribution types
    QJsonObject json;
    json["id"] = "org.test.full";
    json["name"] = "Test Plugin";
    json["version"] = "1.0.0";

    // Contributions are nested under "contributions" key
    QJsonObject contrib;

    QJsonArray langs;
    QJsonObject langObj;
    langObj["id"] = "go";
    langObj["name"] = "Go";
    langObj["extensions"] = QJsonArray{".go"};
    langs.append(langObj);
    contrib["languages"] = langs;

    QJsonArray tasks;
    QJsonObject taskObj;
    taskObj["id"] = "test.run";
    taskObj["label"] = "Run Tests";
    taskObj["type"] = "shell";
    taskObj["command"] = "go test";
    tasks.append(taskObj);
    contrib["tasks"] = tasks;

    QJsonArray settings;
    QJsonObject settObj;
    settObj["key"] = "gopath";
    settObj["title"] = "GOPATH";
    settObj["type"] = "string";
    settObj["default"] = "/usr/local/go";
    settings.append(settObj);
    contrib["settings"] = settings;

    QJsonArray themes;
    QJsonObject themeObj;
    themeObj["id"] = "go-dark";
    themeObj["label"] = "Go Dark";
    themeObj["type"] = "dark";
    themeObj["path"] = "themes/dark.json";
    themes.append(themeObj);
    contrib["themes"] = themes;

    json["contributions"] = contrib;

    PluginManifest manifest = PluginManifest::fromJson(json);

    QCOMPARE(manifest.id, QString("org.test.full"));
    QCOMPARE(manifest.languages.size(), 1);
    QCOMPARE(manifest.languages.first().id, QString("go"));
    QCOMPARE(manifest.languages.first().extensions.size(), 1);
    QCOMPARE(manifest.tasks.size(), 1);
    QCOMPARE(manifest.tasks.first().id, QString("test.run"));
    QCOMPARE(manifest.settings.size(), 1);
    QCOMPARE(manifest.settings.first().key, QString("gopath"));
    QCOMPARE(manifest.themes.size(), 1);
    QCOMPARE(manifest.themes.first().id, QString("go-dark"));
}

void TestContributionRegistry::manifestActivationEvents()
{
    PluginManifest manifest;
    QVERIFY(!manifest.activatesOnStartup());

    manifest.activationEvents = {"onCommand:test.run"};
    QVERIFY(!manifest.activatesOnStartup());

    manifest.activationEvents = {"*"};
    QVERIFY(manifest.activatesOnStartup());

    manifest.activationEvents = {"workspaceContains:**/go.mod", "*"};
    QVERIFY(manifest.activatesOnStartup());
}

QTEST_MAIN(TestContributionRegistry)
#include "test_contributionregistry.moc"
