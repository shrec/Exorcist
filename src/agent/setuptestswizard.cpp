#include "setuptestswizard.h"

#include <QDir>

SetupTestsWizard::SetupTestsWizard(QObject *parent) : QObject(parent) {}

QList<SetupTestsWizard::SetupStep> SetupTestsWizard::generateSetupSteps(const QString &workspaceRoot) const
{
    QList<SetupStep> steps;
    const QDir root(workspaceRoot);

    // CMake/C++ project
    if (root.exists(QStringLiteral("CMakeLists.txt"))) {
        steps.append({
            tr("Add GoogleTest dependency"),
            tr("Add GoogleTest via FetchContent in CMakeLists.txt"),
            {},
            workspaceRoot + QStringLiteral("/tests/CMakeLists.txt"),
            QStringLiteral(
                "include(FetchContent)\n"
                "FetchContent_Declare(\n"
                "    googletest\n"
                "    GIT_REPOSITORY https://github.com/google/googletest.git\n"
                "    GIT_TAG v1.14.0\n"
                ")\n"
                "FetchContent_MakeAvailable(googletest)\n\n"
                "enable_testing()\n\n"
                "# Add test executables here:\n"
                "# add_executable(my_test my_test.cpp)\n"
                "# target_link_libraries(my_test GTest::gtest_main)\n"
                "# add_test(NAME my_test COMMAND my_test)\n")
        });
        steps.append({
            tr("Add tests subdirectory"),
            tr("Add add_subdirectory(tests) to root CMakeLists.txt"),
            {}, {}, {}
        });
        steps.append({
            tr("Create sample test"),
            tr("Create a sample test file"),
            {},
            workspaceRoot + QStringLiteral("/tests/sample_test.cpp"),
            QStringLiteral(
                "#include <gtest/gtest.h>\n\n"
                "TEST(SampleTest, BasicAssertions) {\n"
                "    EXPECT_EQ(1 + 1, 2);\n"
                "    EXPECT_TRUE(true);\n"
                "}\n")
        });
        steps.append({
            tr("Build and run tests"),
            tr("Configure, build, and run tests"),
            QStringLiteral("cmake --build build && ctest --test-dir build"),
            {}, {}
        });
        return steps;
    }

    // Node.js project
    if (root.exists(QStringLiteral("package.json"))) {
        steps.append({
            tr("Install Jest"),
            tr("Install Jest test framework"),
            QStringLiteral("npm install --save-dev jest"),
            {}, {}
        });
        steps.append({
            tr("Create sample test"),
            tr("Create a sample test file"),
            {},
            workspaceRoot + QStringLiteral("/tests/sample.test.js"),
            QStringLiteral(
                "describe('Sample Test Suite', () => {\n"
                "  test('basic assertion', () => {\n"
                "    expect(1 + 1).toBe(2);\n"
                "  });\n"
                "});\n")
        });
        steps.append({
            tr("Run tests"),
            tr("Run the test suite"),
            QStringLiteral("npx jest"),
            {}, {}
        });
        return steps;
    }

    // Python project
    if (root.exists(QStringLiteral("pyproject.toml")) ||
        root.exists(QStringLiteral("setup.py")) ||
        root.exists(QStringLiteral("requirements.txt"))) {
        steps.append({
            tr("Install pytest"),
            tr("Install pytest test framework"),
            QStringLiteral("pip install pytest"),
            {}, {}
        });
        steps.append({
            tr("Create sample test"),
            tr("Create a sample test file"),
            {},
            workspaceRoot + QStringLiteral("/tests/test_sample.py"),
            QStringLiteral(
                "import pytest\n\n"
                "def test_basic():\n"
                "    assert 1 + 1 == 2\n\n"
                "def test_string():\n"
                "    assert 'hello'.upper() == 'HELLO'\n")
        });
        steps.append({
            tr("Run tests"),
            tr("Run the test suite"),
            QStringLiteral("pytest"),
            {}, {}
        });
        return steps;
    }

    return steps;
}

QString SetupTestsWizard::buildSetupPrompt(const QString &workspaceRoot) const
{
    const QDir root(workspaceRoot);
    QStringList files;
    for (const auto &f : root.entryInfoList(QDir::Files))
        files << f.fileName();

    return QStringLiteral(
        "The user wants to set up a testing framework for their project.\n\n"
        "**Workspace root:** %1\n"
        "**Files in root:** %2\n\n"
        "Please analyze the project type and suggest the best testing framework.\n"
        "Provide step-by-step instructions to:\n"
        "1. Install the framework\n"
        "2. Configure it\n"
        "3. Create a sample test\n"
        "4. Run the tests\n")
        .arg(workspaceRoot, files.join(QStringLiteral(", ")));
}
