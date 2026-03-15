#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "project/iprojecttemplateprovider.h"
#include "project/projecttemplateregistry.h"
#include "project/builtintemplateprovider.h"

class TestProjectTemplates : public QObject
{
    Q_OBJECT

private slots:
    void testBuiltinProviderHasTemplates();
    void testLanguageList();
    void testTemplatesForLanguage();
    void testRegistryCollectsProviders();
    void testCppConsoleCreation();
    void testCppLibraryCreation();
    void testCmakeProjectCreation();
    void testCConsoleCreation();
    void testRustBinaryCreation();
    void testRustLibraryCreation();
    void testPythonScriptCreation();
    void testPythonPackageCreation();
    void testGoModuleCreation();
    void testNodeJsCreation();
    void testWebProjectCreation();
    void testTypeScriptNodeCreation();
    void testZigProjectCreation();
    void testGenericTemplateCreation();
    void testUnknownTemplateReturnsError();
    void testCreateInNonexistentDir();
    void testAllTemplatesHaveUniqueIds();
};

void TestProjectTemplates::testBuiltinProviderHasTemplates()
{
    BuiltinTemplateProvider provider;
    const auto tmpls = provider.templates();
    QVERIFY(tmpls.size() >= 20);  // top 20 languages

    for (const auto &t : tmpls) {
        QVERIFY(!t.id.isEmpty());
        QVERIFY(!t.language.isEmpty());
        QVERIFY(!t.name.isEmpty());
        QVERIFY(!t.description.isEmpty());
    }
}

void TestProjectTemplates::testLanguageList()
{
    BuiltinTemplateProvider provider;
    ProjectTemplateRegistry registry;
    registry.addProvider(&provider);

    const QStringList langs = registry.languages();
    QVERIFY(langs.size() >= 15);  // at least 15 unique languages
    QVERIFY(langs.contains(QStringLiteral("C++")));
    QVERIFY(langs.contains(QStringLiteral("Rust")));
    QVERIFY(langs.contains(QStringLiteral("Python")));
    QVERIFY(langs.contains(QStringLiteral("Go")));
    QVERIFY(langs.contains(QStringLiteral("JavaScript")));
}

void TestProjectTemplates::testTemplatesForLanguage()
{
    BuiltinTemplateProvider provider;
    ProjectTemplateRegistry registry;
    registry.addProvider(&provider);

    const auto cppTemplates = registry.templatesForLanguage(QStringLiteral("C++"));
    QVERIFY(cppTemplates.size() >= 3);  // console, library, cmake

    const auto rustTemplates = registry.templatesForLanguage(QStringLiteral("Rust"));
    QVERIFY(rustTemplates.size() >= 2);  // binary, library

    // Non-existent language
    const auto none = registry.templatesForLanguage(QStringLiteral("COBOL"));
    QCOMPARE(none.size(), 0);
}

void TestProjectTemplates::testRegistryCollectsProviders()
{
    ProjectTemplateRegistry registry;
    QCOMPARE(registry.allTemplates().size(), 0);
    QCOMPARE(registry.languages().size(), 0);

    BuiltinTemplateProvider provider;
    registry.addProvider(&provider);
    QVERIFY(registry.allTemplates().size() > 0);

    // Adding same provider again should not duplicate
    registry.addProvider(&provider);
    const int count = registry.allTemplates().size();
    QCOMPARE(count, provider.templates().size());
}

// ── Creation tests ───────────────────────────────────────────────────────────

void TestProjectTemplates::testCppConsoleCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("MyCppApp"));

    ProjectTemplateRegistry registry;
    BuiltinTemplateProvider provider;
    registry.addProvider(&provider);

    QString error;
    QVERIFY(registry.createProject(QStringLiteral("cpp-console"),
                                   QStringLiteral("MyCppApp"), dir, &error));
    QVERIFY(error.isEmpty());
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("CMakeLists.txt"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/main.cpp"))));
}

void TestProjectTemplates::testCppLibraryCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("MyLib"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("cpp-library"),
                                   QStringLiteral("MyLib"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("CMakeLists.txt"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("include/MyLib.h"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/MyLib.cpp"))));
}

void TestProjectTemplates::testCmakeProjectCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("Proj"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("cmake-project"),
                                   QStringLiteral("Proj"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("CMakeLists.txt"))));
}

void TestProjectTemplates::testCConsoleCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("MyCApp"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("c-console"),
                                   QStringLiteral("MyCApp"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("CMakeLists.txt"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/main.c"))));
}

void TestProjectTemplates::testRustBinaryCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("myrust"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("rust-binary"),
                                   QStringLiteral("myrust"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("Cargo.toml"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/main.rs"))));
}

void TestProjectTemplates::testRustLibraryCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("mylib"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("rust-library"),
                                   QStringLiteral("mylib"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("Cargo.toml"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/lib.rs"))));
}

void TestProjectTemplates::testPythonScriptCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("mypy"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("python-script"),
                                   QStringLiteral("mypy"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("main.py"))));
}

void TestProjectTemplates::testPythonPackageCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("MyPkg"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("python-package"),
                                   QStringLiteral("MyPkg"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("pyproject.toml"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("mypkg/__init__.py"))));
}

void TestProjectTemplates::testGoModuleCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("mygo"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("go-module"),
                                   QStringLiteral("mygo"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("go.mod"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("main.go"))));
}

void TestProjectTemplates::testNodeJsCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("mynode"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("js-node"),
                                   QStringLiteral("mynode"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("package.json"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("index.js"))));
}

void TestProjectTemplates::testWebProjectCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("myweb"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("js-web"),
                                   QStringLiteral("myweb"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("index.html"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("css/style.css"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("js/main.js"))));
}

void TestProjectTemplates::testTypeScriptNodeCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("myts"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("ts-node"),
                                   QStringLiteral("myts"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("package.json"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("tsconfig.json"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/index.ts"))));
}

void TestProjectTemplates::testZigProjectCreation()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("myzig"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("zig-project"),
                                   QStringLiteral("myzig"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("build.zig"))));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/main.zig"))));
}

void TestProjectTemplates::testGenericTemplateCreation()
{
    // Test one of the generic scaffold templates (Java)
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("MyJava"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("java-console"),
                                   QStringLiteral("MyJava"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("src/Main.java"))));
}

void TestProjectTemplates::testUnknownTemplateReturnsError()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    ProjectTemplateRegistry registry;
    BuiltinTemplateProvider provider;
    registry.addProvider(&provider);

    QString error;
    QVERIFY(!registry.createProject(QStringLiteral("nonexistent-template"),
                                    QStringLiteral("Test"), tmp.path(), &error));
    QVERIFY(!error.isEmpty());
}

void TestProjectTemplates::testCreateInNonexistentDir()
{
    // Creating in a deep path that doesn't exist yet — should auto-create
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString dir = tmp.filePath(QStringLiteral("deep/nested/project"));

    BuiltinTemplateProvider provider;
    QString error;
    QVERIFY(provider.createProject(QStringLiteral("python-script"),
                                   QStringLiteral("myproj"), dir, &error));
    QVERIFY(QFile::exists(QDir(dir).filePath(QStringLiteral("main.py"))));
}

void TestProjectTemplates::testAllTemplatesHaveUniqueIds()
{
    BuiltinTemplateProvider provider;
    const auto tmpls = provider.templates();

    QSet<QString> ids;
    for (const auto &t : tmpls) {
        QVERIFY2(!ids.contains(t.id),
                 qPrintable(QStringLiteral("Duplicate template ID: %1").arg(t.id)));
        ids.insert(t.id);
    }
}

QTEST_MAIN(TestProjectTemplates)
#include "test_projecttemplates.moc"
