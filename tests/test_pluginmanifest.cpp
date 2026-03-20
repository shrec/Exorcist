#include <QJsonDocument>
#include <QFile>
#include <QTest>

#include "plugin/pluginmanifest.h"

#ifndef EXORCIST_SOURCE_DIR
#define EXORCIST_SOURCE_DIR ""
#endif

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
    void testMenuContributionLocations();
    void testLanguageContribution();
    void testFromJsonMissingFields();
    void testEmptyJson();
    void testDependency();
    void testIsLanguagePluginExplicit();
    void testIsLanguagePluginAutoInferred();
    void testIsGeneralPlugin();
    void testAutoInferFromActivationEvents();
    void testWaveOneBundledPluginManifests();
    void testEmbeddedBundledPluginManifests();
    void testEmbeddedLinuxRemotePluginManifest();
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

void TestPluginManifest::testMenuContributionLocations()
{
    QJsonObject buildMenuJson;
    buildMenuJson[QStringLiteral("commandId")] = QStringLiteral("build.run");
    buildMenuJson[QStringLiteral("location")] = QStringLiteral("build");

    QJsonObject windowMenuJson;
    windowMenuJson[QStringLiteral("commandId")] = QStringLiteral("window.toggleZen");
    windowMenuJson[QStringLiteral("location")] = QStringLiteral("window");

    QJsonObject json;
    json[QStringLiteral("id")] = QStringLiteral("menus.test");
    json[QStringLiteral("name")] = QStringLiteral("Menus Test");
    json[QStringLiteral("version")] = QStringLiteral("1.0.0");

    QJsonObject contrib;
    contrib[QStringLiteral("menus")] = QJsonArray{buildMenuJson, windowMenuJson};
    json[QStringLiteral("contributions")] = contrib;

    const auto manifest = PluginManifest::fromJson(json);
    QCOMPARE(manifest.menus.size(), 2);
    QCOMPARE(manifest.menus[0].location, MenuContribution::MainMenuBuild);
    QCOMPARE(manifest.menus[1].location, MenuContribution::MainMenuWindow);
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

void TestPluginManifest::testWaveOneBundledPluginManifests()
{
    struct ManifestExpectation {
        QString path;
        QString pluginId;
        QString category;
    };

    const QList<ManifestExpectation> manifests = {
        {QStringLiteral(EXORCIST_SOURCE_DIR "/plugins/python-language/plugin.json"),
         QStringLiteral("org.exorcist.python-language"),
         QStringLiteral("language")},
        {QStringLiteral(EXORCIST_SOURCE_DIR "/plugins/rust-language/plugin.json"),
         QStringLiteral("org.exorcist.rust-language"),
         QStringLiteral("language")},
        {QStringLiteral(EXORCIST_SOURCE_DIR "/plugins/qt-tools/plugin.json"),
         QStringLiteral("org.exorcist.qt-tools"),
         QStringLiteral("workbench")},
    };

    for (const ManifestExpectation &expectation : manifests) {
        QFile file(expectation.path);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(expectation.path));

        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        const PluginManifest manifest = PluginManifest::fromJson(json);
        QCOMPARE(manifest.id, expectation.pluginId);
        QCOMPARE(manifest.category, expectation.category);
        QVERIFY(!manifest.activationEvents.isEmpty());
    }
}

void TestPluginManifest::testEmbeddedBundledPluginManifests()
{
    struct ManifestExpectation {
        QString path;
        QString pluginId;
    };

    const QList<ManifestExpectation> manifests = {
        {QStringLiteral(EXORCIST_SOURCE_DIR "/plugins/embedded-tools/plugin.json"),
         QStringLiteral("org.exorcist.embedded-tools")},
        {QStringLiteral(EXORCIST_SOURCE_DIR "/plugins/serial-monitor/plugin.json"),
         QStringLiteral("org.exorcist.serial-monitor")},
    };

    for (const ManifestExpectation &expectation : manifests) {
        QFile file(expectation.path);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(expectation.path));

        const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
        const PluginManifest manifest = PluginManifest::fromJson(json);
        QCOMPARE(manifest.id, expectation.pluginId);
        QCOMPARE(manifest.category, QStringLiteral("workbench"));
        QVERIFY(manifest.activationEvents.contains(QStringLiteral("onProfile:embedded-mcu")));
        QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:platformio.ini")));
        QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:west.yml")));
        QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:sdkconfig")));
        QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:pyocd.yaml")));
        QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:openocd*.cfg")));
        QVERIFY(!manifest.commands.isEmpty());
        QVERIFY(!manifest.views.isEmpty());
    }
}

void TestPluginManifest::testEmbeddedLinuxRemotePluginManifest()
{
    QFile file(QStringLiteral(EXORCIST_SOURCE_DIR "/plugins/remote/plugin.json"));
    QVERIFY(file.open(QIODevice::ReadOnly));

    const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
    const PluginManifest manifest = PluginManifest::fromJson(json);
    QCOMPARE(manifest.id, QStringLiteral("org.exorcist.remote"));
    QVERIFY(manifest.activationEvents.contains(QStringLiteral("onProfile:embedded-linux")));
    QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:buildroot")));
    QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:yocto")));
    QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:bblayers.conf")));
    QVERIFY(manifest.activationEvents.contains(QStringLiteral("workspaceContains:*.bb")));
    QVERIFY(manifest.activationEvents.contains(QStringLiteral("onCommand:remote.showExplorer")));
    QVERIFY(!manifest.views.isEmpty());
    QVERIFY(!manifest.commands.isEmpty());
}

QTEST_MAIN(TestPluginManifest)
#include "test_pluginmanifest.moc"
