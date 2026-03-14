#include <QTest>
#include <QJsonDocument>

#include "plugin/pluginmanifest.h"

class TestPluginManifest : public QObject
{
    Q_OBJECT

private slots:
    void testFromJsonBasicFields();
    void testToJsonRoundTrip();
    void testActivatesOnStartup();
    void testActivatesOnStartupFalse();
    void testHasContributions();
    void testHasNoContributions();
    void testCommandContribution();
    void testLanguageContribution();
    void testFromJsonMissingFields();
    void testEmptyJson();
    void testDependency();
    void testIsLanguagePluginExplicit();
    void testIsLanguagePluginAutoInferred();
    void testIsGeneralPlugin();
    void testAutoInferFromActivationEvents();
};

void TestPluginManifest::testFromJsonBasicFields()
{
    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("my.plugin");
    json[QStringLiteral("name")] = QStringLiteral("My Plugin");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");
    json[QStringLiteral("description")] = QStringLiteral("A test plugin");
    json[QStringLiteral("author")] = QStringLiteral("Test Author");
    json[QStringLiteral("license")] = QStringLiteral("MIT");

    auto manifest = PluginManifest::fromJson(json);
    QCOMPARE(manifest.id, QStringLiteral("my.plugin"));
    QCOMPARE(manifest.name, QStringLiteral("My Plugin"));
    QCOMPARE(manifest.version, QStringLiteral("1.0.0"));
    QCOMPARE(manifest.description, QStringLiteral("A test plugin"));
    QCOMPARE(manifest.author, QStringLiteral("Test Author"));
    QCOMPARE(manifest.license, QStringLiteral("MIT"));
}

void TestPluginManifest::testToJsonRoundTrip()
{
    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("roundtrip.test");
    json[QStringLiteral("name")] = QStringLiteral("Roundtrip");
    json[QStringLiteral("version")] = QStringLiteral("2.0.0");
    json[QStringLiteral("description")] = QStringLiteral("Test roundtrip");
    json[QStringLiteral("author")] = QStringLiteral("Author");
    json[QStringLiteral("license")] = QStringLiteral("BSD");
    json[QStringLiteral("activationEvents")] = QJsonArray{QStringLiteral("*")};

    auto manifest = PluginManifest::fromJson(json);
    QJsonObject restored = manifest.toJson();

    QCOMPARE(restored[QStringLiteral("id")].toString(), QStringLiteral("roundtrip.test"));
    QCOMPARE(restored[QStringLiteral("name")].toString(), QStringLiteral("Roundtrip"));
    QCOMPARE(restored[QStringLiteral("version")].toString(), QStringLiteral("2.0.0"));
}

void TestPluginManifest::testActivatesOnStartup()
{
    PluginManifest m;
    m.activationEvents = {QStringLiteral("*")};
    QVERIFY(m.activatesOnStartup());
}

void TestPluginManifest::testActivatesOnStartupFalse()
{
    PluginManifest m;
    m.activationEvents = {QStringLiteral("onLanguage:python")};
    QVERIFY(!m.activatesOnStartup());
}

void TestPluginManifest::testHasContributions()
{
    PluginManifest m;
    QVERIFY(!m.hasContributions());

    CommandContribution cmd;
    cmd.id = QStringLiteral("test.cmd");
    cmd.title = QStringLiteral("Test");
    m.commands.append(cmd);
    QVERIFY(m.hasContributions());
}

void TestPluginManifest::testHasNoContributions()
{
    PluginManifest m;
    QVERIFY(!m.hasContributions());
}

void TestPluginManifest::testCommandContribution()
{
    QJsonObject cmdJson;
    cmdJson[QStringLiteral("id")] = QStringLiteral("plugin.doThing");
    cmdJson[QStringLiteral("title")] = QStringLiteral("Do Thing");
    cmdJson[QStringLiteral("category")] = QStringLiteral("Plugin");
    cmdJson[QStringLiteral("keybinding")] = QStringLiteral("Ctrl+T");

    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("cmd.test");
    json[QStringLiteral("name")] = QStringLiteral("Cmd Test");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");

    QJsonObject contrib;
    contrib[QStringLiteral("commands")] = QJsonArray{cmdJson};
    json[QStringLiteral("contributions")] = contrib;

    auto manifest = PluginManifest::fromJson(json);
    QCOMPARE(manifest.commands.size(), 1);
    QCOMPARE(manifest.commands[0].id, QStringLiteral("plugin.doThing"));
    QCOMPARE(manifest.commands[0].title, QStringLiteral("Do Thing"));
    QCOMPARE(manifest.commands[0].keybinding, QStringLiteral("Ctrl+T"));
}

void TestPluginManifest::testLanguageContribution()
{
    QJsonObject langJson;
    langJson[QStringLiteral("id")] = QStringLiteral("mylang");
    langJson[QStringLiteral("name")] = QStringLiteral("MyLang");
    langJson[QStringLiteral("extensions")] = QJsonArray{QStringLiteral(".ml"), QStringLiteral(".mli")};

    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("lang.test");
    json[QStringLiteral("name")] = QStringLiteral("Lang Test");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");

    QJsonObject contrib;
    contrib[QStringLiteral("languages")] = QJsonArray{langJson};
    json[QStringLiteral("contributions")] = contrib;

    auto manifest = PluginManifest::fromJson(json);
    QCOMPARE(manifest.languages.size(), 1);
    QCOMPARE(manifest.languages[0].id, QStringLiteral("mylang"));
    QCOMPARE(manifest.languages[0].extensions.size(), 2);
}

void TestPluginManifest::testFromJsonMissingFields()
{
    // Minimal JSON — should not crash, use defaults
    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("minimal");

    auto manifest = PluginManifest::fromJson(json);
    QCOMPARE(manifest.id, QStringLiteral("minimal"));
    QVERIFY(manifest.name.isEmpty());
    QVERIFY(manifest.commands.isEmpty());
    QVERIFY(!manifest.hasContributions());
}

void TestPluginManifest::testEmptyJson()
{
    auto manifest = PluginManifest::fromJson(QJsonObject());
    QVERIFY(manifest.id.isEmpty());
    QVERIFY(!manifest.hasContributions());
}

void TestPluginManifest::testDependency()
{
    QJsonObject depJson;
    depJson[QStringLiteral("id")] = QStringLiteral("dep.plugin");
    depJson[QStringLiteral("minVersion")] = QStringLiteral("1.0.0");
    depJson[QStringLiteral("optional")] = true;

    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("dep.test");
    json[QStringLiteral("name")] = QStringLiteral("Dep Test");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");
    json[QStringLiteral("dependencies")] = QJsonArray{depJson};

    auto manifest = PluginManifest::fromJson(json);
    QCOMPARE(manifest.dependencies.size(), 1);
    QCOMPARE(manifest.dependencies[0].pluginId, QStringLiteral("dep.plugin"));
    QCOMPARE(manifest.dependencies[0].minVersion, QStringLiteral("1.0.0"));
    QVERIFY(manifest.dependencies[0].optional);
}

void TestPluginManifest::testIsLanguagePluginExplicit()
{
    PluginManifest m;
    QVERIFY(m.isGeneralPlugin());
    QVERIFY(!m.isLanguagePlugin());

    m.category = QStringLiteral("language");
    QVERIFY(m.isLanguagePlugin());
    QVERIFY(!m.isGeneralPlugin());
}

void TestPluginManifest::testIsLanguagePluginAutoInferred()
{
    // Build JSON with a language contribution — auto-infer languageIds
    QJsonObject langJson;
    langJson[QStringLiteral("id")] = QStringLiteral("rust");
    langJson[QStringLiteral("name")] = QStringLiteral("Rust");
    langJson[QStringLiteral("extensions")] = QJsonArray{QStringLiteral(".rs")};

    QJsonObject contrib;
    contrib[QStringLiteral("languages")] = QJsonArray{langJson};

    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("lang.rust");
    json[QStringLiteral("name")] = QStringLiteral("Rust Support");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");
    json[QStringLiteral("contributions")] = contrib;

    auto manifest = PluginManifest::fromJson(json);
    QVERIFY(manifest.isLanguagePlugin());
    QVERIFY(manifest.languageIds.contains(QStringLiteral("rust")));
    QCOMPARE(manifest.category, QStringLiteral("language"));
}

void TestPluginManifest::testIsGeneralPlugin()
{
    // AI-style plugin with "*" activation, no language contributions
    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("ai.copilot");
    json[QStringLiteral("name")] = QStringLiteral("Copilot");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");
    json[QStringLiteral("category")] = QStringLiteral("ai");
    json[QStringLiteral("activationEvents")] = QJsonArray{QStringLiteral("*")};

    auto manifest = PluginManifest::fromJson(json);
    QVERIFY(manifest.isGeneralPlugin());
    QVERIFY(!manifest.isLanguagePlugin());
    QVERIFY(manifest.languageIds.isEmpty());
}

void TestPluginManifest::testAutoInferFromActivationEvents()
{
    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("lang.python");
    json[QStringLiteral("name")] = QStringLiteral("Python");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");
    json[QStringLiteral("activationEvents")] = QJsonArray{
        QStringLiteral("onLanguage:python"),
        QStringLiteral("onLanguage:jupyter")
    };

    auto manifest = PluginManifest::fromJson(json);
    QVERIFY(manifest.isLanguagePlugin());
    QVERIFY(manifest.languageIds.contains(QStringLiteral("python")));
    QVERIFY(manifest.languageIds.contains(QStringLiteral("jupyter")));
}

QTEST_MAIN(TestPluginManifest)
#include "test_pluginmanifest.moc"
