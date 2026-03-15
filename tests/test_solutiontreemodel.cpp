#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "project/solutiontreemodel.h"
#include "project/projectmanager.h"

/// Unit tests for SolutionTreeModel — file/directory exclusion filtering.
class TestSolutionTreeModel : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultHiddenDirsNotEmpty();
    void testDefaultHiddenFilePatternsNotEmpty();
    void testHiddenDirsContainCommonEntries();
    void testHiddenFilePatternsContainProjectFiles();
    void testHiddenFilePatternsContainBuildArtifacts();
    void testDirFilteringHidesGitDir();
    void testDirFilteringHidesBuildDir();
    void testDirFilteringAllowsSourceDirs();
    void testFileFilteringHidesExcpprj();
    void testFileFilteringHidesObjectFiles();
    void testFileFilteringHidesGitignore();
    void testFileFilteringAllowsSourceFiles();
    void testShowHiddenFilesDisablesFiltering();
    void testAddRemoveExcludedDirPattern();
    void testAddRemoveExcludedFilePattern();
    void testTreeFiltersFilesInProject();
};

void TestSolutionTreeModel::testDefaultHiddenDirsNotEmpty()
{
    const auto dirs = SolutionTreeModel::defaultHiddenDirs();
    QVERIFY(dirs.size() >= 20);
}

void TestSolutionTreeModel::testDefaultHiddenFilePatternsNotEmpty()
{
    const auto pats = SolutionTreeModel::defaultHiddenFilePatterns();
    QVERIFY(pats.size() >= 15);
}

void TestSolutionTreeModel::testHiddenDirsContainCommonEntries()
{
    const auto dirs = SolutionTreeModel::defaultHiddenDirs();
    QVERIFY(dirs.contains(QStringLiteral(".git")));
    QVERIFY(dirs.contains(QStringLiteral("node_modules")));
    QVERIFY(dirs.contains(QStringLiteral("build")));
    QVERIFY(dirs.contains(QStringLiteral("build-debug")));
    QVERIFY(dirs.contains(QStringLiteral("CMakeFiles")));
    QVERIFY(dirs.contains(QStringLiteral("__pycache__")));
    QVERIFY(dirs.contains(QStringLiteral("target")));
    QVERIFY(dirs.contains(QStringLiteral(".exorcist")));
}

void TestSolutionTreeModel::testHiddenFilePatternsContainProjectFiles()
{
    const auto pats = SolutionTreeModel::defaultHiddenFilePatterns();
    QVERIFY(pats.contains(QStringLiteral("*.excpprj")));
    QVERIFY(pats.contains(QStringLiteral("*.excprj")));
    QVERIFY(pats.contains(QStringLiteral("*.exrsprj")));
    QVERIFY(pats.contains(QStringLiteral("*.exsln")));
    QVERIFY(pats.contains(QStringLiteral("*.exprj")));
}

void TestSolutionTreeModel::testHiddenFilePatternsContainBuildArtifacts()
{
    const auto pats = SolutionTreeModel::defaultHiddenFilePatterns();
    QVERIFY(pats.contains(QStringLiteral("*.o")));
    QVERIFY(pats.contains(QStringLiteral("*.obj")));
    QVERIFY(pats.contains(QStringLiteral("*.exe")));
    QVERIFY(pats.contains(QStringLiteral("*.pdb")));
    QVERIFY(pats.contains(QStringLiteral(".clang-format")));
    QVERIFY(pats.contains(QStringLiteral(".clang-tidy")));
    QVERIFY(pats.contains(QStringLiteral("CMakePresets.json")));
    QVERIFY(pats.contains(QStringLiteral(".gitignore")));
}

void TestSolutionTreeModel::testDirFilteringHidesGitDir()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create a .git directory
    QDir(tmp.path()).mkpath(QStringLiteral(".git"));

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    QFileInfo gitDir(QDir(tmp.path()).filePath(QStringLiteral(".git")));
    QVERIFY(model.shouldHideEntry(gitDir));
}

void TestSolutionTreeModel::testDirFilteringHidesBuildDir()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir(tmp.path()).mkpath(QStringLiteral("build"));
    QDir(tmp.path()).mkpath(QStringLiteral("build-debug"));

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    QVERIFY(model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("build")))));
    QVERIFY(model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("build-debug")))));
}

void TestSolutionTreeModel::testDirFilteringAllowsSourceDirs()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir(tmp.path()).mkpath(QStringLiteral("src"));
    QDir(tmp.path()).mkpath(QStringLiteral("include"));
    QDir(tmp.path()).mkpath(QStringLiteral("tests"));

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("src")))));
    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("include")))));
    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("tests")))));
}

void TestSolutionTreeModel::testFileFilteringHidesExcpprj()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create a .excpprj file
    QFile f(QDir(tmp.path()).filePath(QStringLiteral("MyProject.excpprj")));
    f.open(QIODevice::WriteOnly);
    f.write("{}");
    f.close();

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    QVERIFY(model.shouldHideEntry(QFileInfo(f.fileName())));
}

void TestSolutionTreeModel::testFileFilteringHidesObjectFiles()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QFile f(QDir(tmp.path()).filePath(QStringLiteral("main.o")));
    f.open(QIODevice::WriteOnly);
    f.close();

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    QVERIFY(model.shouldHideEntry(QFileInfo(f.fileName())));
}

void TestSolutionTreeModel::testFileFilteringHidesGitignore()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QFile f(QDir(tmp.path()).filePath(QStringLiteral(".gitignore")));
    f.open(QIODevice::WriteOnly);
    f.close();

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    QVERIFY(model.shouldHideEntry(QFileInfo(f.fileName())));
}

void TestSolutionTreeModel::testFileFilteringAllowsSourceFiles()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create source files
    for (const auto &name : {QStringLiteral("main.cpp"), QStringLiteral("CMakeLists.txt"),
                              QStringLiteral("README.md"), QStringLiteral("version.h")}) {
        QFile f(QDir(tmp.path()).filePath(name));
        f.open(QIODevice::WriteOnly);
        f.close();
    }

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("main.cpp")))));
    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("CMakeLists.txt")))));
    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("README.md")))));
    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral("version.h")))));
}

void TestSolutionTreeModel::testShowHiddenFilesDisablesFiltering()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QDir(tmp.path()).mkpath(QStringLiteral(".git"));
    QFile f(QDir(tmp.path()).filePath(QStringLiteral("MyProject.excpprj")));
    f.open(QIODevice::WriteOnly);
    f.close();

    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    // Hidden by default
    QVERIFY(model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral(".git")))));
    QVERIFY(model.shouldHideEntry(QFileInfo(f.fileName())));

    // Show hidden files
    model.setShowHiddenFiles(true);

    // Nothing hidden anymore
    QVERIFY(!model.shouldHideEntry(QFileInfo(QDir(tmp.path()).filePath(QStringLiteral(".git")))));
    QVERIFY(!model.shouldHideEntry(QFileInfo(f.fileName())));
}

void TestSolutionTreeModel::testAddRemoveExcludedDirPattern()
{
    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    // Add custom excluded dir
    model.addExcludedDirPattern(QStringLiteral("my_custom_dir"));
    QVERIFY(model.excludedDirPatterns().contains(QStringLiteral("my_custom_dir")));

    // Remove it
    model.removeExcludedDirPattern(QStringLiteral("my_custom_dir"));
    QVERIFY(!model.excludedDirPatterns().contains(QStringLiteral("my_custom_dir")));
}

void TestSolutionTreeModel::testAddRemoveExcludedFilePattern()
{
    ProjectManager pm;
    SolutionTreeModel model(&pm, nullptr);

    // Add custom excluded file pattern
    model.addExcludedFilePattern(QStringLiteral("*.log"));
    QVERIFY(model.excludedFilePatterns().contains(QStringLiteral("*.log")));

    // Remove it
    model.removeExcludedFilePattern(QStringLiteral("*.log"));
    QVERIFY(!model.excludedFilePatterns().contains(QStringLiteral("*.log")));
}

void TestSolutionTreeModel::testTreeFiltersFilesInProject()
{
    // Create a mini project structure and verify the tree model filters correctly
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString projDir = tmp.filePath(QStringLiteral("MyProject"));
    QDir().mkpath(projDir);

    // Create files that should be visible
    for (const auto &name : {QStringLiteral("CMakeLists.txt"), QStringLiteral("README.md")}) {
        QFile f(QDir(projDir).filePath(name));
        f.open(QIODevice::WriteOnly);
        f.write("test");
        f.close();
    }
    QDir(projDir).mkpath(QStringLiteral("src"));
    {
        QFile f(QDir(projDir).filePath(QStringLiteral("src/main.cpp")));
        f.open(QIODevice::WriteOnly);
        f.write("int main() {}");
        f.close();
    }

    // Create files that should be hidden
    for (const auto &name : {QStringLiteral("MyProject.excpprj"),
                              QStringLiteral(".gitignore"),
                              QStringLiteral(".clang-format")}) {
        QFile f(QDir(projDir).filePath(name));
        f.open(QIODevice::WriteOnly);
        f.write("{}");
        f.close();
    }
    QDir(projDir).mkpath(QStringLiteral("build"));
    QDir(projDir).mkpath(QStringLiteral(".git"));

    // Create a solution pointing to this project
    ProjectManager pm;
    pm.createSolution(QStringLiteral("TestSln"), projDir);
    pm.addProject(QStringLiteral("MyProject"), projDir);

    SolutionTreeModel model(&pm, nullptr);

    // The model root should have a Solution node
    const QModelIndex rootIdx;
    QCOMPARE(model.rowCount(rootIdx), 1);  // 1 solution node

    const QModelIndex slnIdx = model.index(0, 0, rootIdx);
    QVERIFY(slnIdx.isValid());
    QCOMPARE(model.rowCount(slnIdx), 1);  // 1 project

    const QModelIndex projIdx = model.index(0, 0, slnIdx);
    QVERIFY(projIdx.isValid());

    // Fetch children (trigger lazy load)
    if (model.canFetchMore(projIdx))
        model.fetchMore(projIdx);

    // Count visible entries — should not include hidden files/dirs
    const int childCount = model.rowCount(projIdx);
    QStringList visibleNames;
    for (int i = 0; i < childCount; ++i) {
        const QModelIndex childIdx = model.index(i, 0, projIdx);
        visibleNames << childIdx.data(Qt::DisplayRole).toString();
    }

    // Source dirs and source files should be visible
    QVERIFY2(visibleNames.contains(QStringLiteral("src")), "src/ should be visible");
    QVERIFY2(visibleNames.contains(QStringLiteral("CMakeLists.txt")), "CMakeLists.txt should be visible");
    QVERIFY2(visibleNames.contains(QStringLiteral("README.md")), "README.md should be visible");

    // Hidden entries should not be visible
    QVERIFY2(!visibleNames.contains(QStringLiteral("MyProject.excpprj")), ".excpprj should be hidden");
    QVERIFY2(!visibleNames.contains(QStringLiteral(".gitignore")), ".gitignore should be hidden");
    QVERIFY2(!visibleNames.contains(QStringLiteral(".clang-format")), ".clang-format should be hidden");
    QVERIFY2(!visibleNames.contains(QStringLiteral("build")), "build/ should be hidden");
    QVERIFY2(!visibleNames.contains(QStringLiteral(".git")), ".git/ should be hidden");
}

QTEST_MAIN(TestSolutionTreeModel)
#include "test_solutiontreemodel.moc"
