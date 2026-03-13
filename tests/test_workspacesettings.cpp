#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "settings/workspacesettings.h"

class TestWorkspaceSettings : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // ── Global-only resolution ───────────────────────────────────────────
    void globalDefault_noWorkspace();
    void globalOverride_returnsGlobalValue();

    // ── Workspace override ───────────────────────────────────────────────
    void workspaceOverride_takePriority();
    void workspaceOverride_missingKey_fallsToGlobal();
    void hasWorkspaceOverride_true();
    void hasWorkspaceOverride_false();

    // ── Set / remove workspace values ────────────────────────────────────
    void setWorkspaceValue_createsFile();
    void setWorkspaceValue_updatesExisting();
    void removeWorkspaceValue_revertsToGlobal();
    void removeWorkspaceValue_removesEmptyGroup();

    // ── File I/O ─────────────────────────────────────────────────────────
    void loadFromDisk();
    void malformedJson_ignored();
    void missingFile_emptyOverrides();

    // ── Typed accessors ──────────────────────────────────────────────────
    void fontFamily_default();
    void fontSize_workspaceOverride();
    void tabSize_workspaceOverride();
    void wordWrap_workspaceOverride();
    void showLineNumbers_default();
    void showMinimap_default();

    // ── Signals ──────────────────────────────────────────────────────────
    void settingsChanged_emittedOnLoad();
    void settingsChanged_emittedOnSet();
    void settingsChanged_emittedOnRemove();

    // ── Multiple groups ──────────────────────────────────────────────────
    void multipleGroups_independent();

    // ── setWorkspaceRoot clears old ──────────────────────────────────────
    void changeWorkspaceRoot_clearsOldSettings();
    void clearWorkspaceRoot_revertsAll();

    // ── reload ───────────────────────────────────────────────────────────
    void reload_picksUpExternalChanges();

private:
    QTemporaryDir m_tmpDir;
    void writeSettingsFile(const QJsonObject &json);
    void ensureExorcistDir();
    void setGlobalSetting(const QString &group, const QString &key, const QVariant &value);
    void clearGlobalSettings();
};

void TestWorkspaceSettings::init()
{
    QVERIFY(m_tmpDir.isValid());
    clearGlobalSettings();
}

void TestWorkspaceSettings::cleanup()
{
    clearGlobalSettings();
}

void TestWorkspaceSettings::writeSettingsFile(const QJsonObject &json)
{
    ensureExorcistDir();
    const QString path = m_tmpDir.path() + QStringLiteral("/.exorcist/settings.json");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(json).toJson());
}

void TestWorkspaceSettings::ensureExorcistDir()
{
    QDir(m_tmpDir.path()).mkpath(QStringLiteral(".exorcist"));
}

void TestWorkspaceSettings::setGlobalSetting(const QString &group, const QString &key,
                                              const QVariant &value)
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(group);
    s.setValue(key, value);
    s.endGroup();
}

void TestWorkspaceSettings::clearGlobalSettings()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    // Save and restore only the keys we didn't set for testing
    // For test isolation: clear only our test keys
    s.beginGroup(QStringLiteral("_test"));
    s.remove(QString());
    s.endGroup();
}

// ── Global-only resolution ───────────────────────────────────────────────────

void TestWorkspaceSettings::globalDefault_noWorkspace()
{
    WorkspaceSettings ws;
    // No workspace root set — should return default
    QCOMPARE(ws.value(QStringLiteral("editor"), QStringLiteral("tabSize"), 4).toInt(), 4);
}

void TestWorkspaceSettings::globalOverride_returnsGlobalValue()
{
    setGlobalSetting(QStringLiteral("_test"), QStringLiteral("someKey"), 42);
    WorkspaceSettings ws;
    QCOMPARE(ws.value(QStringLiteral("_test"), QStringLiteral("someKey"), 0).toInt(), 42);
}

// ── Workspace override ───────────────────────────────────────────────────────

void TestWorkspaceSettings::workspaceOverride_takePriority()
{
    setGlobalSetting(QStringLiteral("editor"), QStringLiteral("tabSize"), 4);

    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 2);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.value(QStringLiteral("editor"), QStringLiteral("tabSize"), 4).toInt(), 2);
}

void TestWorkspaceSettings::workspaceOverride_missingKey_fallsToGlobal()
{
    setGlobalSetting(QStringLiteral("_test"), QStringLiteral("globalOnly"), QStringLiteral("hello"));

    // Workspace has editor group but not _test group
    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 2);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.value(QStringLiteral("_test"), QStringLiteral("globalOnly")).toString(),
             QStringLiteral("hello"));
}

void TestWorkspaceSettings::hasWorkspaceOverride_true()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 2);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QVERIFY(ws.hasWorkspaceOverride(QStringLiteral("editor"), QStringLiteral("tabSize")));
}

void TestWorkspaceSettings::hasWorkspaceOverride_false()
{
    QTemporaryDir emptyDir;
    QVERIFY(emptyDir.isValid());
    WorkspaceSettings ws;
    ws.setWorkspaceRoot(emptyDir.path());
    QVERIFY(!ws.hasWorkspaceOverride(QStringLiteral("editor"), QStringLiteral("tabSize")));
}

// ── Set / remove workspace values ────────────────────────────────────────────

void TestWorkspaceSettings::setWorkspaceValue_createsFile()
{
    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    ws.setWorkspaceValue(QStringLiteral("editor"), QStringLiteral("tabSize"), 8);

    // File should exist now
    const QString path = m_tmpDir.path() + QStringLiteral("/.exorcist/settings.json");
    QVERIFY(QFileInfo::exists(path));

    // Read back
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QJsonObject json = QJsonDocument::fromJson(f.readAll()).object();
    QCOMPARE(json[QStringLiteral("editor")].toObject()[QStringLiteral("tabSize")].toInt(), 8);
}

void TestWorkspaceSettings::setWorkspaceValue_updatesExisting()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 2);
    editor.insert(QStringLiteral("fontFamily"), QStringLiteral("Mono"));
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    ws.setWorkspaceValue(QStringLiteral("editor"), QStringLiteral("tabSize"), 4);

    // tabSize updated, fontFamily preserved
    QCOMPARE(ws.value(QStringLiteral("editor"), QStringLiteral("tabSize")).toInt(), 4);
    QCOMPARE(ws.value(QStringLiteral("editor"), QStringLiteral("fontFamily")).toString(),
             QStringLiteral("Mono"));
}

void TestWorkspaceSettings::removeWorkspaceValue_revertsToGlobal()
{
    setGlobalSetting(QStringLiteral("editor"), QStringLiteral("tabSize"), 4);

    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 2);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.tabSize(), 2);

    ws.removeWorkspaceValue(QStringLiteral("editor"), QStringLiteral("tabSize"));
    QCOMPARE(ws.tabSize(), 4); // reverts to global
}

void TestWorkspaceSettings::removeWorkspaceValue_removesEmptyGroup()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 2);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    ws.removeWorkspaceValue(QStringLiteral("editor"), QStringLiteral("tabSize"));

    // Group should be removed entirely since it's empty
    QVERIFY(ws.workspaceJson().isEmpty());
}

// ── File I/O ─────────────────────────────────────────────────────────────────

void TestWorkspaceSettings::loadFromDisk()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("fontSize"), 14);
    editor.insert(QStringLiteral("wordWrap"), true);
    QJsonObject indexer;
    indexer.insert(QStringLiteral("maxFileSizeKB"), 256);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    root.insert(QStringLiteral("indexer"), indexer);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());

    QCOMPARE(ws.fontSize(), 14);
    QCOMPARE(ws.wordWrap(), true);
    QCOMPARE(ws.value(QStringLiteral("indexer"), QStringLiteral("maxFileSizeKB")).toInt(), 256);
}

void TestWorkspaceSettings::malformedJson_ignored()
{
    ensureExorcistDir();
    const QString path = m_tmpDir.path() + QStringLiteral("/.exorcist/settings.json");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("{ invalid json !!!");
    f.close();

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    // Should not crash, should have empty overrides
    QVERIFY(ws.workspaceJson().isEmpty());
}

void TestWorkspaceSettings::missingFile_emptyOverrides()
{
    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path()); // no .exorcist/ dir
    QVERIFY(ws.workspaceJson().isEmpty());
}

// ── Typed accessors ──────────────────────────────────────────────────────────

void TestWorkspaceSettings::fontFamily_default()
{
    WorkspaceSettings ws;
    QCOMPARE(ws.fontFamily(), QStringLiteral("Consolas"));
}

void TestWorkspaceSettings::fontSize_workspaceOverride()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("fontSize"), 16);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.fontSize(), 16);
}

void TestWorkspaceSettings::tabSize_workspaceOverride()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 8);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.tabSize(), 8);
}

void TestWorkspaceSettings::wordWrap_workspaceOverride()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("wordWrap"), true);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QVERIFY(ws.wordWrap());
}

void TestWorkspaceSettings::showLineNumbers_default()
{
    WorkspaceSettings ws;
    QVERIFY(ws.showLineNumbers()); // default true
}

void TestWorkspaceSettings::showMinimap_default()
{
    WorkspaceSettings ws;
    QVERIFY(!ws.showMinimap()); // default false
}

// ── Signals ──────────────────────────────────────────────────────────────────

void TestWorkspaceSettings::settingsChanged_emittedOnLoad()
{
    QJsonObject root;
    root.insert(QStringLiteral("editor"), QJsonObject{{QStringLiteral("tabSize"), 2}});
    writeSettingsFile(root);

    WorkspaceSettings ws;
    QSignalSpy spy(&ws, &WorkspaceSettings::settingsChanged);
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(spy.count(), 1);
}

void TestWorkspaceSettings::settingsChanged_emittedOnSet()
{
    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());

    QSignalSpy spy(&ws, &WorkspaceSettings::settingsChanged);
    ws.setWorkspaceValue(QStringLiteral("editor"), QStringLiteral("tabSize"), 8);
    QCOMPARE(spy.count(), 1);
}

void TestWorkspaceSettings::settingsChanged_emittedOnRemove()
{
    QJsonObject root;
    root.insert(QStringLiteral("editor"), QJsonObject{{QStringLiteral("tabSize"), 2}});
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());

    QSignalSpy spy(&ws, &WorkspaceSettings::settingsChanged);
    ws.removeWorkspaceValue(QStringLiteral("editor"), QStringLiteral("tabSize"));
    QCOMPARE(spy.count(), 1);
}

// ── Multiple groups ──────────────────────────────────────────────────────────

void TestWorkspaceSettings::multipleGroups_independent()
{
    QJsonObject editor;
    editor.insert(QStringLiteral("tabSize"), 2);
    QJsonObject indexer;
    indexer.insert(QStringLiteral("maxFileSizeKB"), 512);
    QJsonObject root;
    root.insert(QStringLiteral("editor"), editor);
    root.insert(QStringLiteral("indexer"), indexer);
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());

    QCOMPARE(ws.value(QStringLiteral("editor"), QStringLiteral("tabSize")).toInt(), 2);
    QCOMPARE(ws.value(QStringLiteral("indexer"), QStringLiteral("maxFileSizeKB")).toInt(), 512);
    QVERIFY(!ws.hasWorkspaceOverride(QStringLiteral("editor"), QStringLiteral("fontSize")));
}

// ── setWorkspaceRoot clears old ──────────────────────────────────────────────

void TestWorkspaceSettings::changeWorkspaceRoot_clearsOldSettings()
{
    QJsonObject root;
    root.insert(QStringLiteral("editor"), QJsonObject{{QStringLiteral("tabSize"), 2}});
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.tabSize(), 2);

    // Switch to a different (empty) workspace
    QTemporaryDir tmp2;
    QVERIFY(tmp2.isValid());
    ws.setWorkspaceRoot(tmp2.path());
    // Should no longer have workspace override
    QVERIFY(!ws.hasWorkspaceOverride(QStringLiteral("editor"), QStringLiteral("tabSize")));
}

void TestWorkspaceSettings::clearWorkspaceRoot_revertsAll()
{
    QJsonObject root;
    root.insert(QStringLiteral("editor"), QJsonObject{{QStringLiteral("tabSize"), 2}});
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.tabSize(), 2);

    ws.setWorkspaceRoot(QString());
    QVERIFY(ws.workspaceJson().isEmpty());
}

// ── reload ───────────────────────────────────────────────────────────────────

void TestWorkspaceSettings::reload_picksUpExternalChanges()
{
    QJsonObject root;
    root.insert(QStringLiteral("editor"), QJsonObject{{QStringLiteral("tabSize"), 2}});
    writeSettingsFile(root);

    WorkspaceSettings ws;
    ws.setWorkspaceRoot(m_tmpDir.path());
    QCOMPARE(ws.tabSize(), 2);

    // Simulate external edit
    QJsonObject newRoot;
    newRoot.insert(QStringLiteral("editor"), QJsonObject{{QStringLiteral("tabSize"), 8}});
    writeSettingsFile(newRoot);

    QSignalSpy spy(&ws, &WorkspaceSettings::settingsChanged);
    ws.reload();
    QCOMPARE(ws.tabSize(), 8);
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestWorkspaceSettings)
#include "test_workspacesettings.moc"
