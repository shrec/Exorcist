#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#include "profile/profilemanifest.h"
#include "profile/profilemanager.h"

// ── Test Class ──────────────────────────────────────────────────────────────

class TestProfileManager : public QObject
{
    Q_OBJECT

private slots:
    // ProfileManifest tests
    void testManifestFromValidJson();
    void testManifestFromMinimalJson();
    void testManifestFromEmptyJson();
    void testManifestMissingId();
    void testManifestMissingName();
    void testManifestDetectionRules();
    void testManifestFileContentRule();
    void testManifestDockDefaults();
    void testManifestSettings();

    // ProfileManager tests
    void testRegisterProfile();
    void testRegisterInvalidProfile();
    void testAvailableProfiles();
    void testProfileName();
    void testProfileDescription();
    void testActivateProfile();
    void testActivateNonexistent();
    void testDeactivateProfile();
    void testActiveProfileSignal();
    void testAutoDetect();
    void testLoadProfilesFromDirectory();
    void testLoadProfilesEmptyDir();
    void testLoadProfilesNonexistentDir();
    void testDetectProfileFileExists();
    void testDetectProfileDirectoryExists();
    void testDetectProfileFileContent();
    void testDetectProfileThreshold();
    void testDetectProfileBestScore();
    void testDetectProfileEmptyPath();
    void testManifestLookup();

private:
    ProfileManifest makeTestProfile(const QString &id,
                                     const QString &name);
};

// ── ProfileManifest tests ───────────────────────────────────────────────────

void TestProfileManager::testManifestFromValidJson()
{
    QJsonObject obj;
    obj[QStringLiteral("id")]          = QStringLiteral("test-profile");
    obj[QStringLiteral("name")]        = QStringLiteral("Test Profile");
    obj[QStringLiteral("description")] = QStringLiteral("A test profile");
    obj[QStringLiteral("icon")]        = QStringLiteral(":/test.svg");
    obj[QStringLiteral("category")]    = QStringLiteral("test");

    QJsonArray req;
    req.append(QStringLiteral("plugin-a"));
    req.append(QStringLiteral("plugin-b"));
    obj[QStringLiteral("requiredPlugins")] = req;

    auto m = ProfileManifest::fromJson(obj);
    QVERIFY(m.isValid());
    QCOMPARE(m.id, QStringLiteral("test-profile"));
    QCOMPARE(m.name, QStringLiteral("Test Profile"));
    QCOMPARE(m.description, QStringLiteral("A test profile"));
    QCOMPARE(m.icon, QStringLiteral(":/test.svg"));
    QCOMPARE(m.category, QStringLiteral("test"));
    QCOMPARE(m.requiredPlugins.size(), 2);
    QCOMPARE(m.requiredPlugins.at(0), QStringLiteral("plugin-a"));
}

void TestProfileManager::testManifestFromMinimalJson()
{
    QJsonObject obj;
    obj[QStringLiteral("id")]   = QStringLiteral("minimal");
    obj[QStringLiteral("name")] = QStringLiteral("Minimal");

    auto m = ProfileManifest::fromJson(obj);
    QVERIFY(m.isValid());
    QCOMPARE(m.id, QStringLiteral("minimal"));
    QVERIFY(m.requiredPlugins.isEmpty());
    QVERIFY(m.detectionRules.isEmpty());
    QCOMPARE(m.detectionThreshold, 1);
}

void TestProfileManager::testManifestFromEmptyJson()
{
    auto m = ProfileManifest::fromJson(QJsonObject());
    QVERIFY(!m.isValid());
}

void TestProfileManager::testManifestMissingId()
{
    QJsonObject obj;
    obj[QStringLiteral("name")] = QStringLiteral("No ID");
    auto m = ProfileManifest::fromJson(obj);
    QVERIFY(!m.isValid());
}

void TestProfileManager::testManifestMissingName()
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = QStringLiteral("no-name");
    auto m = ProfileManifest::fromJson(obj);
    QVERIFY(!m.isValid());
}

void TestProfileManager::testManifestDetectionRules()
{
    QJsonObject obj;
    obj[QStringLiteral("id")]   = QStringLiteral("detect");
    obj[QStringLiteral("name")] = QStringLiteral("Detect");
    obj[QStringLiteral("detectionThreshold")] = 3;

    QJsonArray rules;
    {
        QJsonObject r;
        r[QStringLiteral("type")]    = QStringLiteral("fileExists");
        r[QStringLiteral("pattern")] = QStringLiteral("CMakeLists.txt");
        r[QStringLiteral("weight")]  = 2;
        rules.append(r);
    }
    {
        QJsonObject r;
        r[QStringLiteral("type")]    = QStringLiteral("directoryExists");
        r[QStringLiteral("pattern")] = QStringLiteral("build");
        r[QStringLiteral("weight")]  = 1;
        rules.append(r);
    }
    obj[QStringLiteral("detection")] = rules;

    auto m = ProfileManifest::fromJson(obj);
    QVERIFY(m.isValid());
    QCOMPARE(m.detectionRules.size(), 2);
    QCOMPARE(m.detectionRules.at(0).type, ProfileDetectionRule::FileExists);
    QCOMPARE(m.detectionRules.at(0).pattern, QStringLiteral("CMakeLists.txt"));
    QCOMPARE(m.detectionRules.at(0).weight, 2);
    QCOMPARE(m.detectionRules.at(1).type, ProfileDetectionRule::DirectoryExists);
    QCOMPARE(m.detectionThreshold, 3);
}

void TestProfileManager::testManifestFileContentRule()
{
    QJsonObject obj;
    obj[QStringLiteral("id")]   = QStringLiteral("fc");
    obj[QStringLiteral("name")] = QStringLiteral("File Content");

    QJsonArray rules;
    QJsonObject r;
    r[QStringLiteral("type")]    = QStringLiteral("fileContent");
    r[QStringLiteral("pattern")] = QStringLiteral("Cargo.toml:^\\[package\\]");
    r[QStringLiteral("weight")]  = 5;
    rules.append(r);
    obj[QStringLiteral("detection")] = rules;

    auto m = ProfileManifest::fromJson(obj);
    QCOMPARE(m.detectionRules.size(), 1);
    QCOMPARE(m.detectionRules.at(0).type, ProfileDetectionRule::FileContent);
    QCOMPARE(m.detectionRules.at(0).weight, 5);
}

void TestProfileManager::testManifestDockDefaults()
{
    QJsonObject obj;
    obj[QStringLiteral("id")]   = QStringLiteral("docks");
    obj[QStringLiteral("name")] = QStringLiteral("Docks");

    QJsonArray docks;
    {
        QJsonObject d;
        d[QStringLiteral("id")]      = QStringLiteral("TerminalDock");
        d[QStringLiteral("visible")] = true;
        docks.append(d);
    }
    {
        QJsonObject d;
        d[QStringLiteral("id")]      = QStringLiteral("GitDock");
        d[QStringLiteral("visible")] = false;
        docks.append(d);
    }
    obj[QStringLiteral("dockDefaults")] = docks;

    auto m = ProfileManifest::fromJson(obj);
    QCOMPARE(m.dockDefaults.size(), 2);
    QCOMPARE(m.dockDefaults.at(0).dockId, QStringLiteral("TerminalDock"));
    QVERIFY(m.dockDefaults.at(0).visible);
    QVERIFY(!m.dockDefaults.at(1).visible);
}

void TestProfileManager::testManifestSettings()
{
    QJsonObject obj;
    obj[QStringLiteral("id")]   = QStringLiteral("settings");
    obj[QStringLiteral("name")] = QStringLiteral("Settings Test");

    QJsonObject sets;
    sets[QStringLiteral("editor.tabSize")] = 4;
    sets[QStringLiteral("editor.wordWrap")] = true;
    obj[QStringLiteral("settings")] = sets;

    auto m = ProfileManifest::fromJson(obj);
    QCOMPARE(m.settings.size(), 2);
}

// ── ProfileManager tests ────────────────────────────────────────────────────

ProfileManifest TestProfileManager::makeTestProfile(const QString &id,
                                                     const QString &name)
{
    ProfileManifest m;
    m.id   = id;
    m.name = name;
    return m;
}

void TestProfileManager::testRegisterProfile()
{
    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(makeTestProfile(QStringLiteral("test"), QStringLiteral("Test")));

    QCOMPARE(mgr.availableProfiles().size(), 1);
    QVERIFY(mgr.availableProfiles().contains(QStringLiteral("test")));
}

void TestProfileManager::testRegisterInvalidProfile()
{
    ProfileManager mgr(nullptr, this);
    ProfileManifest invalid; // empty id/name
    mgr.registerProfile(invalid);
    QCOMPARE(mgr.availableProfiles().size(), 0);
}

void TestProfileManager::testAvailableProfiles()
{
    ProfileManager mgr(nullptr, this);
    QVERIFY(mgr.availableProfiles().isEmpty());

    mgr.registerProfile(makeTestProfile(QStringLiteral("a"), QStringLiteral("A")));
    mgr.registerProfile(makeTestProfile(QStringLiteral("b"), QStringLiteral("B")));
    QCOMPARE(mgr.availableProfiles().size(), 2);
}

void TestProfileManager::testProfileName()
{
    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(makeTestProfile(QStringLiteral("cpp"), QStringLiteral("C++ Native")));

    QCOMPARE(mgr.profileName(QStringLiteral("cpp")), QStringLiteral("C++ Native"));
    QVERIFY(mgr.profileName(QStringLiteral("nonexistent")).isEmpty());
}

void TestProfileManager::testProfileDescription()
{
    ProfileManager mgr(nullptr, this);
    ProfileManifest m = makeTestProfile(QStringLiteral("desc"), QStringLiteral("Desc Test"));
    m.description = QStringLiteral("A test description");
    mgr.registerProfile(m);

    QCOMPARE(mgr.profileDescription(QStringLiteral("desc")),
             QStringLiteral("A test description"));
    QVERIFY(mgr.profileDescription(QStringLiteral("nope")).isEmpty());
}

void TestProfileManager::testActivateProfile()
{
    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(makeTestProfile(QStringLiteral("active"), QStringLiteral("Active")));

    QVERIFY(mgr.activeProfile().isEmpty());
    QVERIFY(mgr.activateProfile(QStringLiteral("active")));
    QCOMPARE(mgr.activeProfile(), QStringLiteral("active"));
}

void TestProfileManager::testActivateNonexistent()
{
    ProfileManager mgr(nullptr, this);
    QVERIFY(!mgr.activateProfile(QStringLiteral("nonexistent")));
    QVERIFY(mgr.activeProfile().isEmpty());
}

void TestProfileManager::testDeactivateProfile()
{
    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(makeTestProfile(QStringLiteral("x"), QStringLiteral("X")));
    mgr.activateProfile(QStringLiteral("x"));
    QCOMPARE(mgr.activeProfile(), QStringLiteral("x"));

    // Deactivate by passing empty string
    QVERIFY(mgr.activateProfile(QString()));
    QVERIFY(mgr.activeProfile().isEmpty());
}

void TestProfileManager::testActiveProfileSignal()
{
    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(makeTestProfile(QStringLiteral("sig"), QStringLiteral("Signal")));

    QSignalSpy spy(&mgr, &ProfileManager::profileChanged);

    mgr.activateProfile(QStringLiteral("sig"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("sig"));
    QVERIFY(spy.at(0).at(1).toString().isEmpty()); // old was empty

    mgr.activateProfile(QString()); // deactivate
    QCOMPARE(spy.count(), 2);
    QVERIFY(spy.at(1).at(0).toString().isEmpty()); // new is empty
    QCOMPARE(spy.at(1).at(1).toString(), QStringLiteral("sig")); // old was "sig"
}

void TestProfileManager::testAutoDetect()
{
    ProfileManager mgr(nullptr, this);
    QVERIFY(mgr.autoDetectEnabled()); // default = true

    mgr.setAutoDetectEnabled(false);
    QVERIFY(!mgr.autoDetectEnabled());

    mgr.setAutoDetectEnabled(true);
    QVERIFY(mgr.autoDetectEnabled());
}

void TestProfileManager::testLoadProfilesFromDirectory()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Write a valid profile JSON
    QJsonObject obj;
    obj[QStringLiteral("id")]   = QStringLiteral("loaded");
    obj[QStringLiteral("name")] = QStringLiteral("Loaded Profile");
    obj[QStringLiteral("description")] = QStringLiteral("From JSON file");

    QFile f(tmpDir.filePath(QStringLiteral("loaded.json")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(obj).toJson());
    f.close();

    // Write an invalid JSON
    QFile bad(tmpDir.filePath(QStringLiteral("bad.json")));
    QVERIFY(bad.open(QIODevice::WriteOnly));
    bad.write("{ invalid json }}}");
    bad.close();

    ProfileManager mgr(nullptr, this);
    mgr.loadProfilesFromDirectory(tmpDir.path());

    QCOMPARE(mgr.availableProfiles().size(), 1);
    QCOMPARE(mgr.profileName(QStringLiteral("loaded")),
             QStringLiteral("Loaded Profile"));
}

void TestProfileManager::testLoadProfilesEmptyDir()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    ProfileManager mgr(nullptr, this);
    mgr.loadProfilesFromDirectory(tmpDir.path());
    QVERIFY(mgr.availableProfiles().isEmpty());
}

void TestProfileManager::testLoadProfilesNonexistentDir()
{
    ProfileManager mgr(nullptr, this);
    mgr.loadProfilesFromDirectory(QStringLiteral("/nonexistent/path/xyz"));
    QVERIFY(mgr.availableProfiles().isEmpty());
}

void TestProfileManager::testDetectProfileFileExists()
{
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());

    // Create a marker file
    QFile marker(QDir(workspace.path()).filePath(QStringLiteral("CMakeLists.txt")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("cmake_minimum_required(VERSION 3.21)\n");
    marker.close();

    ProfileManifest profile;
    profile.id   = QStringLiteral("cmake-detect");
    profile.name = QStringLiteral("CMake");
    profile.detectionThreshold = 1;

    ProfileDetectionRule rule;
    rule.type    = ProfileDetectionRule::FileExists;
    rule.pattern = QStringLiteral("CMakeLists.txt");
    rule.weight  = 2;
    profile.detectionRules.append(rule);

    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(profile);

    QCOMPARE(mgr.detectProfile(workspace.path()),
             QStringLiteral("cmake-detect"));
}

void TestProfileManager::testDetectProfileDirectoryExists()
{
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());

    QDir(workspace.path()).mkdir(QStringLiteral("src"));

    ProfileManifest profile;
    profile.id   = QStringLiteral("dir-detect");
    profile.name = QStringLiteral("Dir");
    profile.detectionThreshold = 1;

    ProfileDetectionRule rule;
    rule.type    = ProfileDetectionRule::DirectoryExists;
    rule.pattern = QStringLiteral("src");
    rule.weight  = 1;
    profile.detectionRules.append(rule);

    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(profile);

    QCOMPARE(mgr.detectProfile(workspace.path()),
             QStringLiteral("dir-detect"));
}

void TestProfileManager::testDetectProfileFileContent()
{
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());

    QFile cargo(QDir(workspace.path()).filePath(QStringLiteral("Cargo.toml")));
    QVERIFY(cargo.open(QIODevice::WriteOnly));
    cargo.write("[package]\nname = \"test\"\n");
    cargo.close();

    ProfileManifest profile;
    profile.id   = QStringLiteral("rust-detect");
    profile.name = QStringLiteral("Rust");
    profile.detectionThreshold = 1;

    ProfileDetectionRule rule;
    rule.type    = ProfileDetectionRule::FileContent;
    rule.pattern = QStringLiteral("Cargo.toml:^\\[package\\]");
    rule.weight  = 3;
    profile.detectionRules.append(rule);

    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(profile);

    QCOMPARE(mgr.detectProfile(workspace.path()),
             QStringLiteral("rust-detect"));
}

void TestProfileManager::testDetectProfileThreshold()
{
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());

    // Create one marker file (weight = 1)
    QFile marker(QDir(workspace.path()).filePath(QStringLiteral("setup.py")));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("from setuptools import setup\n");
    marker.close();

    ProfileManifest profile;
    profile.id   = QStringLiteral("high-threshold");
    profile.name = QStringLiteral("High");
    profile.detectionThreshold = 5; // requires 5, we only match 1

    ProfileDetectionRule rule;
    rule.type    = ProfileDetectionRule::FileExists;
    rule.pattern = QStringLiteral("setup.py");
    rule.weight  = 1;
    profile.detectionRules.append(rule);

    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(profile);

    // Score (1) < threshold (5), so no match
    QVERIFY(mgr.detectProfile(workspace.path()).isEmpty());
}

void TestProfileManager::testDetectProfileBestScore()
{
    QTemporaryDir workspace;
    QVERIFY(workspace.isValid());

    // Create files matching both profiles
    QFile f1(QDir(workspace.path()).filePath(QStringLiteral("CMakeLists.txt")));
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("cmake\n");
    f1.close();

    QFile f2(QDir(workspace.path()).filePath(QStringLiteral("Cargo.toml")));
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("[package]\n");
    f2.close();

    ProfileManifest cmake;
    cmake.id   = QStringLiteral("cmake");
    cmake.name = QStringLiteral("CMake");
    cmake.detectionThreshold = 1;
    {
        ProfileDetectionRule r;
        r.type    = ProfileDetectionRule::FileExists;
        r.pattern = QStringLiteral("CMakeLists.txt");
        r.weight  = 2;
        cmake.detectionRules.append(r);
    }

    ProfileManifest rust;
    rust.id   = QStringLiteral("rust");
    rust.name = QStringLiteral("Rust");
    rust.detectionThreshold = 1;
    {
        ProfileDetectionRule r;
        r.type    = ProfileDetectionRule::FileExists;
        r.pattern = QStringLiteral("Cargo.toml");
        r.weight  = 5; // higher score wins
        rust.detectionRules.append(r);
    }

    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(cmake);
    mgr.registerProfile(rust);

    // Rust has higher score (5 vs 2)
    QCOMPARE(mgr.detectProfile(workspace.path()), QStringLiteral("rust"));
}

void TestProfileManager::testDetectProfileEmptyPath()
{
    ProfileManager mgr(nullptr, this);
    mgr.registerProfile(makeTestProfile(QStringLiteral("x"), QStringLiteral("X")));
    QVERIFY(mgr.detectProfile(QString()).isEmpty());
}

void TestProfileManager::testManifestLookup()
{
    ProfileManager mgr(nullptr, this);
    QVERIFY(mgr.manifest(QStringLiteral("nope")) == nullptr);

    ProfileManifest m = makeTestProfile(QStringLiteral("lookup"), QStringLiteral("Lookup"));
    m.description = QStringLiteral("test description");
    mgr.registerProfile(m);

    const ProfileManifest *found = mgr.manifest(QStringLiteral("lookup"));
    QVERIFY(found != nullptr);
    QCOMPARE(found->description, QStringLiteral("test description"));
}

QTEST_MAIN(TestProfileManager)
#include "test_profilemanager.moc"
