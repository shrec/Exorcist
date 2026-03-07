#include "testscaffold.h"

#include <QDir>
#include <QFileInfo>

TestScaffold::TestScaffold(QObject *parent)
    : QObject(parent)
{
}

TestScaffold::Framework TestScaffold::detectFramework(const QString &workspaceRoot) const
{
    const QDir root(workspaceRoot);

    // C++ frameworks
    if (root.exists(QStringLiteral("CMakeLists.txt"))) {
        QFile cmake(root.filePath(QStringLiteral("CMakeLists.txt")));
        if (cmake.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(cmake.readAll());
            if (content.contains(QStringLiteral("GTest"), Qt::CaseInsensitive) ||
                content.contains(QStringLiteral("gtest"), Qt::CaseInsensitive))
                return GoogleTest;
            if (content.contains(QStringLiteral("Catch2"), Qt::CaseInsensitive))
                return Catch2;
            if (content.contains(QStringLiteral("Qt6Test"), Qt::CaseInsensitive) ||
                content.contains(QStringLiteral("Qt5Test"), Qt::CaseInsensitive))
                return QtTest;
        }
    }

    // Python
    if (root.exists(QStringLiteral("pytest.ini")) ||
        root.exists(QStringLiteral("setup.cfg")) ||
        root.exists(QStringLiteral("pyproject.toml")))
        return Pytest;

    // JavaScript
    if (root.exists(QStringLiteral("package.json"))) {
        QFile pkg(root.filePath(QStringLiteral("package.json")));
        if (pkg.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(pkg.readAll());
            if (content.contains(QStringLiteral("jest")))
                return Jest;
            if (content.contains(QStringLiteral("mocha")))
                return Mocha;
        }
    }

    return Unknown;
}

QString TestScaffold::frameworkName(Framework fw) const
{
    switch (fw) {
    case GoogleTest: return QStringLiteral("Google Test");
    case Catch2:     return QStringLiteral("Catch2");
    case QtTest:     return QStringLiteral("Qt Test");
    case Pytest:     return QStringLiteral("pytest");
    case Jest:       return QStringLiteral("Jest");
    case Mocha:      return QStringLiteral("Mocha");
    default:         return QStringLiteral("Unknown");
    }
}

QString TestScaffold::testFilePath(const QString &sourceFile, Framework fw) const
{
    QFileInfo fi(sourceFile);
    const QString base = fi.completeBaseName();
    const QString dir  = fi.absolutePath();

    switch (fw) {
    case GoogleTest:
    case Catch2:
    case QtTest:
        return dir + QStringLiteral("/test_") + base + QStringLiteral(".cpp");
    case Pytest:
        return dir + QStringLiteral("/test_") + base + QStringLiteral(".py");
    case Jest:
    case Mocha:
        return dir + QStringLiteral("/") + base + QStringLiteral(".test.js");
    default:
        return dir + QStringLiteral("/test_") + base + fi.suffix();
    }
}

QString TestScaffold::generateScaffold(const QString &sourceFile,
                                        const QString &sourceContent,
                                        Framework fw) const
{
    Q_UNUSED(sourceContent)
    const QFileInfo fi(sourceFile);
    const QString base = fi.completeBaseName();

    switch (fw) {
    case GoogleTest:
        return QStringLiteral(
            "#include <gtest/gtest.h>\n"
            "#include \"%1\"\n\n"
            "TEST(%2Test, BasicTest) {\n"
            "    // TODO: Add test cases\n"
            "    EXPECT_TRUE(true);\n"
            "}\n\n"
            "int main(int argc, char **argv) {\n"
            "    ::testing::InitGoogleTest(&argc, argv);\n"
            "    return RUN_ALL_TESTS();\n"
            "}\n")
            .arg(fi.fileName(), base);

    case Catch2:
        return QStringLiteral(
            "#include <catch2/catch_test_macros.hpp>\n"
            "#include \"%1\"\n\n"
            "TEST_CASE(\"%2 tests\", \"[%2]\") {\n"
            "    SECTION(\"basic test\") {\n"
            "        REQUIRE(true);\n"
            "    }\n"
            "}\n")
            .arg(fi.fileName(), base);

    case QtTest:
        return QStringLiteral(
            "#include <QTest>\n"
            "#include \"%1\"\n\n"
            "class Test%2 : public QObject {\n"
            "    Q_OBJECT\n"
            "private slots:\n"
            "    void testBasic() {\n"
            "        QVERIFY(true);\n"
            "    }\n"
            "};\n\n"
            "QTEST_MAIN(Test%2)\n"
            "#include \"test_%3.moc\"\n")
            .arg(fi.fileName(), base, base.toLower());

    case Pytest:
        return QStringLiteral(
            "import pytest\n"
            "# from %1 import *\n\n"
            "def test_basic():\n"
            "    \"\"\"Basic test for %1\"\"\"\n"
            "    assert True\n")
            .arg(base);

    case Jest:
    case Mocha:
        return QStringLiteral(
            "// const { } = require('./%1');\n\n"
            "describe('%1', () => {\n"
            "    test('should work', () => {\n"
            "        expect(true).toBe(true);\n"
            "    });\n"
            "});\n")
            .arg(base);

    default:
        return QStringLiteral("// Test file for %1\n// TODO: Add test cases\n").arg(base);
    }
}
