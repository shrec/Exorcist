#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "project/projectmanager.h"

class TestProjectManager : public QObject
{
    Q_OBJECT

private slots:

    // ── Construction ──────────────────────────────────────────────────────

    void defaultState()
    {
        ProjectManager pm;
        QVERIFY(pm.solution().name.isEmpty());
        QVERIFY(pm.solution().filePath.isEmpty());
        QVERIFY(pm.solution().projects.isEmpty());
        QVERIFY(pm.activeSolutionDir().isEmpty());
    }

    // ── openFolder — no .exsln ────────────────────────────────────────────

    void openFolder_noSolution()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        ProjectManager pm;
        QSignalSpy spy(&pm, &ProjectManager::solutionChanged);

        bool result = pm.openFolder(tmp.path());
        QVERIFY(!result); // no .exsln → returns false, creates transient

        QCOMPARE(spy.count(), 1);
        QCOMPARE(pm.solution().projects.size(), 1);
        QCOMPARE(pm.solution().projects[0].rootPath, QDir(tmp.path()).absolutePath());
        QVERIFY(pm.solution().filePath.isEmpty()); // transient — no persisted file
    }

    void openFolder_folderNameBecomesSolutionName()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        ProjectManager pm;
        pm.openFolder(tmp.path());
        QCOMPARE(pm.solution().name, QDir(tmp.path()).dirName());
    }

    // ── openFolder — with .exsln ──────────────────────────────────────────

    void openFolder_withSolution()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Write a minimal .exsln file
        QJsonObject obj;
        obj["name"] = QStringLiteral("TestSln");
        obj["projects"] = QJsonArray();
        QFile sln(QDir(tmp.path()).filePath("project.exsln"));
        QVERIFY(sln.open(QIODevice::WriteOnly));
        sln.write(QJsonDocument(obj).toJson());
        sln.close();

        ProjectManager pm;
        QSignalSpy spy(&pm, &ProjectManager::solutionChanged);

        bool result = pm.openFolder(tmp.path());
        QVERIFY(result);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(pm.solution().name, QStringLiteral("TestSln"));
    }

    // ── openSolution ──────────────────────────────────────────────────────

    void openSolution_valid()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Create solution with one project
        QJsonObject proj;
        proj["name"] = QStringLiteral("core");
        proj["path"] = QStringLiteral("src");

        QJsonObject obj;
        obj["name"] = QStringLiteral("MySolution");
        obj["projects"] = QJsonArray{proj};

        const QString slnPath = QDir(tmp.path()).filePath("test.exsln");
        QFile f(slnPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(obj).toJson());
        f.close();

        // Create the src directory so the path exists
        QDir(tmp.path()).mkdir("src");

        ProjectManager pm;
        QVERIFY(pm.openSolution(slnPath));
        QCOMPARE(pm.solution().name, QStringLiteral("MySolution"));
        QCOMPARE(pm.solution().filePath, slnPath);
        QCOMPARE(pm.solution().projects.size(), 1);
        QCOMPARE(pm.solution().projects[0].name, QStringLiteral("core"));
    }

    void openSolution_nonexistentFile()
    {
        ProjectManager pm;
        QVERIFY(!pm.openSolution(QStringLiteral("/nonexistent/path/test.exsln")));
    }

    void openSolution_invalidJson()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString slnPath = QDir(tmp.path()).filePath("bad.exsln");
        QFile f(slnPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not json");
        f.close();

        ProjectManager pm;
        QVERIFY(!pm.openSolution(slnPath));
    }

    void openSolution_emptyJsonObject()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString slnPath = QDir(tmp.path()).filePath("empty.exsln");
        QFile f(slnPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(QJsonObject()).toJson());
        f.close();

        ProjectManager pm;
        QVERIFY(pm.openSolution(slnPath));
        QVERIFY(pm.solution().name.isEmpty());
        QCOMPARE(pm.solution().projects.size(), 0);
    }

    // ── createSolution ────────────────────────────────────────────────────

    void createSolution_writesFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString slnPath = QDir(tmp.path()).filePath("new.exsln");

        ProjectManager pm;
        QSignalSpy changeSpy(&pm, &ProjectManager::solutionChanged);
        QSignalSpy savedSpy(&pm, &ProjectManager::solutionSaved);

        QVERIFY(pm.createSolution("NewSln", slnPath));
        QCOMPARE(changeSpy.count(), 1);
        QCOMPARE(savedSpy.count(), 1);
        QCOMPARE(pm.solution().name, QStringLiteral("NewSln"));
        QVERIFY(QFile::exists(slnPath));
    }

    // ── addProject / removeProject ────────────────────────────────────────

    void addProject_basic()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        ProjectManager pm;
        pm.openFolder(tmp.path());

        QSignalSpy addSpy(&pm, &ProjectManager::projectAdded);
        QSignalSpy changeSpy(&pm, &ProjectManager::solutionChanged);

        QVERIFY(pm.addProject("lib", tmp.path()));

        QCOMPARE(addSpy.count(), 1);
        QCOMPARE(addSpy[0][0].toInt(), pm.solution().projects.size() - 1);
        QVERIFY(changeSpy.count() >= 1);
    }

    void addProject_emptyPath()
    {
        ProjectManager pm;
        QVERIFY(!pm.addProject("test", QString()));
    }

    void removeProject_valid()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        ProjectManager pm;
        pm.openFolder(tmp.path());
        pm.addProject("extra", tmp.path());
        const int count = pm.solution().projects.size();

        QSignalSpy removeSpy(&pm, &ProjectManager::projectRemoved);
        QVERIFY(pm.removeProject(count - 1));
        QCOMPARE(removeSpy.count(), 1);
        QCOMPARE(pm.solution().projects.size(), count - 1);
    }

    void removeProject_invalidIndex()
    {
        ProjectManager pm;
        QVERIFY(!pm.removeProject(-1));
        QVERIFY(!pm.removeProject(0)); // empty solution
        QVERIFY(!pm.removeProject(100));
    }

    // ── closeSolution ─────────────────────────────────────────────────────

    void closeSolution_clearsState()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        ProjectManager pm;
        pm.openFolder(tmp.path());
        QVERIFY(!pm.solution().projects.isEmpty());

        QSignalSpy spy(&pm, &ProjectManager::solutionChanged);
        pm.closeSolution();
        QCOMPARE(spy.count(), 1);
        QVERIFY(pm.solution().name.isEmpty());
        QVERIFY(pm.solution().projects.isEmpty());
    }

    // ── saveSolution ──────────────────────────────────────────────────────

    void saveSolution_noFilePath()
    {
        ProjectManager pm;
        QVERIFY(!pm.saveSolution()); // no filePath → false
    }

    void saveSolution_validRoundtrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString slnPath = QDir(tmp.path()).filePath("rt.exsln");

        ProjectManager pm1;
        QVERIFY(pm1.createSolution("RoundTrip", slnPath));
        QVERIFY(pm1.addProject("core", tmp.path()));

        QSignalSpy savedSpy(&pm1, &ProjectManager::solutionSaved);
        QVERIFY(pm1.saveSolution());
        QCOMPARE(savedSpy.count(), 1);

        // Reload in a new manager
        ProjectManager pm2;
        QVERIFY(pm2.openSolution(slnPath));
        QCOMPARE(pm2.solution().name, QStringLiteral("RoundTrip"));
        QCOMPARE(pm2.solution().projects.size(), 1);
        QCOMPARE(pm2.solution().projects[0].name, QStringLiteral("core"));
    }

    // ── saveSolutionAs ────────────────────────────────────────────────────

    void saveSolutionAs_newPath()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString orig = QDir(tmp.path()).filePath("orig.exsln");
        const QString copy = QDir(tmp.path()).filePath("copy.exsln");

        ProjectManager pm;
        QVERIFY(pm.createSolution("Orig", orig));

        QSignalSpy savedSpy(&pm, &ProjectManager::solutionSaved);
        QSignalSpy changeSpy(&pm, &ProjectManager::solutionChanged);
        QVERIFY(pm.saveSolutionAs(copy));
        QCOMPARE(savedSpy.count(), 1);
        QCOMPARE(changeSpy.count(), 1);
        QCOMPARE(pm.solution().filePath, copy);
        QVERIFY(QFile::exists(copy));
    }

    // ── activeSolutionDir ────────────────────────────────────────────────

    void activeSolutionDir_fromFilePath()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        const QString slnPath = QDir(tmp.path()).filePath("sln.exsln");
        ProjectManager pm;
        QVERIFY(pm.createSolution("Test", slnPath));
        QCOMPARE(pm.activeSolutionDir(), QDir(tmp.path()).absolutePath());
    }

    void activeSolutionDir_fromFirstProject()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        ProjectManager pm;
        pm.openFolder(tmp.path()); // transient — no filePath
        QCOMPARE(pm.activeSolutionDir(), QDir(tmp.path()).absolutePath());
    }

    // ── Relative paths in JSON ────────────────────────────────────────────

    void relativePaths_preserved()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QDir(tmp.path()).mkdir("subdir");
        const QString slnPath = QDir(tmp.path()).filePath("test.exsln");

        ProjectManager pm;
        QVERIFY(pm.createSolution("RelTest", slnPath));
        QVERIFY(pm.addProject("sub", QDir(tmp.path()).filePath("subdir")));
        QVERIFY(pm.saveSolution());

        // Read JSON and verify relative path
        QFile f(slnPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        const QJsonArray projects = doc.object()["projects"].toArray();
        QCOMPARE(projects.size(), 1);
        QCOMPARE(projects[0].toObject()["path"].toString(), QStringLiteral("subdir"));
    }
};

QTEST_MAIN(TestProjectManager)
#include "test_projectmanager.moc"
