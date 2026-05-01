#include "builtintemplateprovider.h"
#include "projecttypes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

BuiltinTemplateProvider::BuiltinTemplateProvider(QObject *parent)
    : QObject(parent)
{
}

QList<ProjectTemplate> BuiltinTemplateProvider::templates() const
{
    return {
        // ── C++ ──────────────────────────────────────────────────────────
        {QStringLiteral("cpp-console"),  QStringLiteral("C++"),
         tr("Console Application"),
         tr("Professional C++17 console app with CMake presets, tests, "
            "clang-format, clang-tidy, and full project structure."),
         QStringLiteral(":/lang-icons/cpp.svg")},
        {QStringLiteral("cpp-library"),  QStringLiteral("C++"),
         tr("Static Library"),
         tr("A C++ static library with CMake build system."),
         QStringLiteral(":/lang-icons/cpp.svg")},
        {QStringLiteral("cmake-project"), QStringLiteral("C++"),
         tr("Empty CMake Project"),
         tr("A minimal CMake project with an empty source directory."),
         QStringLiteral(":/lang-icons/cmake.svg")},

        // ── C ────────────────────────────────────────────────────────────
        {QStringLiteral("c-console"),    QStringLiteral("C"),
         tr("Console Application"),
         tr("A C console application with CMake build system."),
         QStringLiteral(":/lang-icons/c.svg")},

        // ── Rust ─────────────────────────────────────────────────────────
        {QStringLiteral("rust-binary"),  QStringLiteral("Rust"),
         tr("Binary (Cargo)"),
         tr("A Rust binary project with Cargo.toml."),
         QStringLiteral(":/lang-icons/rust.svg")},
        {QStringLiteral("rust-library"), QStringLiteral("Rust"),
         tr("Library (Cargo)"),
         tr("A Rust library crate with Cargo.toml."),
         QStringLiteral(":/lang-icons/rust.svg")},

        // ── Python ───────────────────────────────────────────────────────
        {QStringLiteral("python-script"), QStringLiteral("Python"),
         tr("Script"),
         tr("A single Python script with main entry point."),
         QStringLiteral(":/lang-icons/python.svg")},
        {QStringLiteral("python-package"), QStringLiteral("Python"),
         tr("Package"),
         tr("A Python package with pyproject.toml and __init__.py."),
         QStringLiteral(":/lang-icons/python.svg")},

        // ── JavaScript ───────────────────────────────────────────────────
        {QStringLiteral("js-node"),      QStringLiteral("JavaScript"),
         tr("Node.js Application"),
         tr("A Node.js application with package.json."),
         QStringLiteral(":/lang-icons/javascript.svg")},
        {QStringLiteral("js-web"),       QStringLiteral("JavaScript"),
         tr("Web Project"),
         tr("A basic web project with HTML, CSS, and JavaScript."),
         QStringLiteral(":/lang-icons/html5.svg")},

        // ── TypeScript ───────────────────────────────────────────────────
        {QStringLiteral("ts-node"),      QStringLiteral("TypeScript"),
         tr("Node.js Application"),
         tr("A TypeScript Node.js application with tsconfig.json."),
         QStringLiteral(":/lang-icons/typescript.svg")},

        // ── Go ───────────────────────────────────────────────────────────
        {QStringLiteral("go-module"),    QStringLiteral("Go"),
         tr("Module"),
         tr("A Go module with go.mod and main.go."),
         QStringLiteral(":/lang-icons/go.svg")},

        // ── Java ─────────────────────────────────────────────────────────
        {QStringLiteral("java-console"), QStringLiteral("Java"),
         tr("Console Application"),
         tr("A Java console application with Maven-style layout."),
         QStringLiteral(":/lang-icons/java.svg")},

        // ── C# ──────────────────────────────────────────────────────────
        {QStringLiteral("csharp-console"), QStringLiteral("C#"),
         tr("Console Application"),
         tr("A .NET console application with .csproj."),
         QStringLiteral(":/lang-icons/csharp.svg")},

        // ── Swift ────────────────────────────────────────────────────────
        {QStringLiteral("swift-package"), QStringLiteral("Swift"),
         tr("Package"),
         tr("A Swift package with Package.swift."),
         QStringLiteral(":/lang-icons/swift.svg")},

        // ── Kotlin ───────────────────────────────────────────────────────
        {QStringLiteral("kotlin-jvm"),   QStringLiteral("Kotlin"),
         tr("JVM Application"),
         tr("A Kotlin/JVM application with Gradle build."),
         QStringLiteral(":/lang-icons/kotlin.svg")},

        // ── Zig ──────────────────────────────────────────────────────────
        {QStringLiteral("zig-project"),  QStringLiteral("Zig"),
         tr("Application"),
         tr("A Zig project with build.zig."),
         QStringLiteral(":/lang-icons/zig.svg")},

        // ── Lua ──────────────────────────────────────────────────────────
        {QStringLiteral("lua-script"),   QStringLiteral("Lua"),
         tr("Script"),
         tr("A Lua script project."),
         QStringLiteral(":/lang-icons/lua.svg")},

        // ── Ruby ─────────────────────────────────────────────────────────
        {QStringLiteral("ruby-script"),  QStringLiteral("Ruby"),
         tr("Script"),
         tr("A Ruby script with Gemfile."),
         QStringLiteral(":/lang-icons/ruby.svg")},

        // ── PHP ──────────────────────────────────────────────────────────
        {QStringLiteral("php-project"),  QStringLiteral("PHP"),
         tr("Project"),
         tr("A PHP project with composer.json."),
         QStringLiteral(":/lang-icons/php.svg")},

        // ── Dart ─────────────────────────────────────────────────────────
        {QStringLiteral("dart-console"), QStringLiteral("Dart"),
         tr("Console Application"),
         tr("A Dart console application with pubspec.yaml."),
         QStringLiteral(":/lang-icons/dart.svg")},

        // ── Haskell ──────────────────────────────────────────────────────
        {QStringLiteral("haskell-stack"), QStringLiteral("Haskell"),
         tr("Stack Project"),
         tr("A Haskell project with stack.yaml and .cabal."),
         QStringLiteral(":/lang-icons/haskell.svg")},

        // ── Scala ────────────────────────────────────────────────────────
        {QStringLiteral("scala-sbt"),    QStringLiteral("Scala"),
         tr("SBT Project"),
         tr("A Scala project with build.sbt."),
         QStringLiteral(":/lang-icons/scala.svg")},

        // ── Qt ───────────────────────────────────────────────────────────
        {QStringLiteral("qt-widget-app"), QStringLiteral("Qt"),
         tr("Qt Widget Application"),
         tr("A Qt 6 desktop GUI application using QWidget. "
            "Includes QMainWindow subclass, .ui form, and CMake build with "
            "AUTOMOC/AUTOUIC/AUTORCC enabled."),
         QStringLiteral(":/lang-icons/cpp.svg")},
        {QStringLiteral("qt-quick-app"), QStringLiteral("Qt"),
         tr("Qt Quick Application"),
         tr("A Qt 6 application using QML and Qt Quick. "
            "Uses qt_add_qml_module with the modern resource layout under "
            "/qt/qml/<project>/."),
         QStringLiteral(":/lang-icons/cpp.svg")},
        {QStringLiteral("qt-plugin"), QStringLiteral("Qt"),
         tr("Qt Plugin"),
         tr("A generic Qt 6 plugin shared library (MODULE) with plugin.json "
            "metadata, exporting a Q_PLUGIN_METADATA-tagged class implementing "
            "an interface."),
         QStringLiteral(":/lang-icons/cpp.svg")},
        {QStringLiteral("qt-console-app"), QStringLiteral("Qt"),
         tr("Qt Console Application"),
         tr("A non-GUI Qt 6 application using QCoreApplication. "
            "Demonstrates the Qt event loop with QTimer + signals/slots."),
         QStringLiteral(":/lang-icons/cpp.svg")},
    };
}

// ── Template dispatch ────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createProject(const QString &templateId,
                                            const QString &projectName,
                                            const QString &location,
                                            QString *error)
{
    // C++
    if (templateId == QLatin1String("cpp-console"))
        return createCppConsole(projectName, location, error);
    if (templateId == QLatin1String("cpp-library"))
        return createCppLibrary(projectName, location, error);
    if (templateId == QLatin1String("cmake-project"))
        return createCMakeProject(projectName, location, error);
    // C
    if (templateId == QLatin1String("c-console"))
        return createCConsole(projectName, location, error);
    // Rust
    if (templateId == QLatin1String("rust-binary"))
        return createRustBinary(projectName, location, error);
    if (templateId == QLatin1String("rust-library"))
        return createRustLibrary(projectName, location, error);
    // Python
    if (templateId == QLatin1String("python-script"))
        return createPythonScript(projectName, location, error);
    if (templateId == QLatin1String("python-package"))
        return createPythonPackage(projectName, location, error);
    // Go
    if (templateId == QLatin1String("go-module"))
        return createGoModule(projectName, location, error);
    // JavaScript
    if (templateId == QLatin1String("js-node"))
        return createNodeJs(projectName, location, error);
    if (templateId == QLatin1String("js-web"))
        return createWebProject(projectName, location, error);
    // TypeScript
    if (templateId == QLatin1String("ts-node"))
        return createTypeScriptNode(projectName, location, error);
    // Zig
    if (templateId == QLatin1String("zig-project"))
        return createZigProject(projectName, location, error);
    // Qt
    if (templateId == QLatin1String("qt-widget-app"))
        return createQtWidgetApp(projectName, location, error);
    if (templateId == QLatin1String("qt-quick-app"))
        return createQtQuickApp(projectName, location, error);
    if (templateId == QLatin1String("qt-plugin"))
        return createQtPlugin(projectName, location, error);
    if (templateId == QLatin1String("qt-console-app"))
        return createQtConsoleApp(projectName, location, error);

    // ── Generic single-file scaffolds ────────────────────────────────────
    // Java
    if (templateId == QLatin1String("java-console")) {
        return createGenericProject(projectName, location,
            QStringLiteral("src/Main.java"),
            QStringLiteral("public class Main {\n"
                           "    public static void main(String[] args) {\n"
                           "        System.out.println(\"Hello, %1!\");\n"
                           "    }\n"
                           "}\n").arg(projectName),
            QStringLiteral("Java"), templateId, error);
    }
    // C#
    if (templateId == QLatin1String("csharp-console"))
        return createGenericProject(projectName, location,
            QStringLiteral("Program.cs"),
            QStringLiteral("Console.WriteLine(\"Hello, %1!\");\n").arg(projectName),
            QStringLiteral("C#"), templateId, error);
    // Swift
    if (templateId == QLatin1String("swift-package"))
        return createGenericProject(projectName, location,
            QStringLiteral("Sources/main.swift"),
            QStringLiteral("print(\"Hello, %1!\")\n").arg(projectName),
            QStringLiteral("Swift"), templateId, error);
    // Kotlin
    if (templateId == QLatin1String("kotlin-jvm"))
        return createGenericProject(projectName, location,
            QStringLiteral("src/main/kotlin/Main.kt"),
            QStringLiteral("fun main() {\n"
                           "    println(\"Hello, %1!\")\n"
                           "}\n").arg(projectName),
            QStringLiteral("Kotlin"), templateId, error);
    // Lua
    if (templateId == QLatin1String("lua-script"))
        return createGenericProject(projectName, location,
            QStringLiteral("main.lua"),
            QStringLiteral("print(\"Hello, %1!\")\n").arg(projectName),
            QStringLiteral("Lua"), templateId, error);
    // Ruby
    if (templateId == QLatin1String("ruby-script"))
        return createGenericProject(projectName, location,
            QStringLiteral("main.rb"),
            QStringLiteral("puts \"Hello, %1!\"\n").arg(projectName),
            QStringLiteral("Ruby"), templateId, error);
    // PHP
    if (templateId == QLatin1String("php-project"))
        return createGenericProject(projectName, location,
            QStringLiteral("index.php"),
            QStringLiteral("<?php\necho \"Hello, %1!\\n\";\n").arg(projectName),
            QStringLiteral("PHP"), templateId, error);
    // Dart
    if (templateId == QLatin1String("dart-console"))
        return createGenericProject(projectName, location,
            QStringLiteral("bin/main.dart"),
            QStringLiteral("void main() {\n"
                           "  print('Hello, %1!');\n"
                           "}\n").arg(projectName),
            QStringLiteral("Dart"), templateId, error);
    // Haskell
    if (templateId == QLatin1String("haskell-stack"))
        return createGenericProject(projectName, location,
            QStringLiteral("app/Main.hs"),
            QStringLiteral("module Main where\n\n"
                           "main :: IO ()\n"
                           "main = putStrLn \"Hello, %1!\"\n").arg(projectName),
            QStringLiteral("Haskell"), templateId, error);
    // Scala
    if (templateId == QLatin1String("scala-sbt"))
        return createGenericProject(projectName, location,
            QStringLiteral("src/main/scala/Main.scala"),
            QStringLiteral("object Main extends App {\n"
                           "  println(\"Hello, %1!\")\n"
                           "}\n").arg(projectName),
            QStringLiteral("Scala"), templateId, error);

    if (error)
        *error = tr("Unknown template: %1").arg(templateId);
    return false;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

static bool writeFile(const QString &path, const QString &content, QString *error)
{
    // Ensure parent directory exists
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QObject::tr("Cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    QTextStream out(&f);
    out << content;
    return true;
}

static bool ensureDir(const QString &dir, QString *error)
{
    if (!QDir().mkpath(dir)) {
        if (error) *error = QObject::tr("Cannot create directory: %1").arg(dir);
        return false;
    }
    return true;
}

/// Write a .ex*prj project file (JSON) into the project directory.
///
/// Format:
/// {
///     "name": "MyApp",
///     "language": "C++",
///     "templateId": "cpp-console",
///     "version": 1,
///     "buildSystem": "cmake",
///     "sources": [ "src/main.cpp" ],
///     "includes": [ "include" ]
/// }
static bool writeProjectFile(const QString &dir,
                              const QString &name,
                              const QString &language,
                              const QString &templateId,
                              const QString &buildSystem,
                              const QStringList &sources,
                              const QStringList &includes,
                              QString *error)
{
    const QString ext = ExProjectExt::forLanguage(language);
    const QString path = QDir(dir).filePath(name + ext);

    QJsonObject obj;
    obj[QStringLiteral("name")] = name;
    obj[QStringLiteral("language")] = language;
    obj[QStringLiteral("templateId")] = templateId;
    obj[QStringLiteral("version")] = 1;
    if (!buildSystem.isEmpty())
        obj[QStringLiteral("buildSystem")] = buildSystem;
    if (!sources.isEmpty())
        obj[QStringLiteral("sources")] = QJsonArray::fromStringList(sources);
    if (!includes.isEmpty())
        obj[QStringLiteral("includes")] = QJsonArray::fromStringList(includes);

    const QJsonDocument doc(obj);
    return writeFile(path, QString::fromUtf8(doc.toJson(QJsonDocument::Indented)), error);
}

// ── Generic scaffold ─────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createGenericProject(const QString &name,
                                                   const QString &dir,
                                                   const QString &filename,
                                                   const QString &content,
                                                   const QString &language,
                                                   const QString &templateId,
                                                   QString *error)
{
    if (!ensureDir(dir, error))
        return false;
    if (!writeFile(QDir(dir).filePath(filename), content, error))
        return false;
    return writeProjectFile(dir, name, language, templateId,
                            {}, {filename}, {}, error);
}

// ── C++ ──────────────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createCppConsole(const QString &name,
                                               const QString &dir,
                                               QString *error)
{
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src")) ||
        !d.mkpath(QStringLiteral("include/%1").arg(name)) ||
        !d.mkpath(QStringLiteral("tests"))) {
        if (error) *error = tr("Cannot create project directories: %1").arg(dir);
        return false;
    }

    // ── CMakeLists.txt ───────────────────────────────────────────────────
    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1\n"
        "    VERSION 0.1.0\n"
        "    LANGUAGES CXX\n"
        "    DESCRIPTION \"%1 console application\"\n"
        ")\n\n"
        "# ── C++ Standard ─────────────────────────────────────────────────\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "set(CMAKE_CXX_EXTENSIONS OFF)\n\n"
        "# ── Compiler warnings ────────────────────────────────────────────\n"
        "add_compile_options(\n"
        "    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->\n"
        "    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>\n"
        ")\n\n"
        "# ── Export compile_commands.json for LSP/clangd ──────────────────\n"
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n"
        "# ── Main executable ──────────────────────────────────────────────\n"
        "add_executable(%1\n"
        "    src/main.cpp\n"
        ")\n\n"
        "target_include_directories(%1 PUBLIC\n"
        "    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>\n"
        "    $<INSTALL_INTERFACE:include>\n"
        ")\n\n"
        "# ── Tests ────────────────────────────────────────────────────────\n"
        "option(BUILD_TESTS \"Build unit tests\" ON)\n"
        "if(BUILD_TESTS)\n"
        "    enable_testing()\n"
        "    add_subdirectory(tests)\n"
        "endif()\n\n"
        "# ── Install ──────────────────────────────────────────────────────\n"
        "install(TARGETS %1 RUNTIME DESTINATION bin)\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    // CMakePresets.json is NOT generated here — it is written by
    // writeCMakePresetsJson() in MainWindow after project creation,
    // using the kit selected in the wizard (compiler + generator paths).

    // ── src/main.cpp ─────────────────────────────────────────────────────
    const QString mainCpp = QStringLiteral(
        "#include <%1/version.h>\n\n"
        "#include <iostream>\n"
        "#include <string>\n\n"
        "int main(int argc, char *argv[])\n"
        "{\n"
        "    std::cout << \"%1 v\" << %1::version() << std::endl;\n\n"
        "    if (argc > 1) {\n"
        "        std::cout << \"Arguments:\" << std::endl;\n"
        "        for (int i = 1; i < argc; ++i)\n"
        "            std::cout << \"  [\" << i << \"] \" << argv[i] << std::endl;\n"
        "    }\n\n"
        "    return 0;\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/main.cpp")), mainCpp, error))
        return false;

    // ── include/<name>/version.h ─────────────────────────────────────────
    const QString versionH = QStringLiteral(
        "#pragma once\n\n"
        "#include <string>\n\n"
        "namespace %1 {\n\n"
        "inline std::string version() { return \"0.1.0\"; }\n\n"
        "} // namespace %1\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("include/%1/version.h").arg(name)),
                   versionH, error))
        return false;

    // ── tests/CMakeLists.txt ─────────────────────────────────────────────
    const QString testsCmake = QStringLiteral(
        "add_executable(test_%1\n"
        "    test_main.cpp\n"
        ")\n\n"
        "target_include_directories(test_%1 PRIVATE\n"
        "    ${CMAKE_SOURCE_DIR}/include\n"
        ")\n\n"
        "add_test(NAME %1_tests COMMAND test_%1)\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("tests/CMakeLists.txt")),
                   testsCmake, error))
        return false;

    // ── tests/test_main.cpp ──────────────────────────────────────────────
    const QString testMain = QStringLiteral(
        "#include <%1/version.h>\n\n"
        "#include <cassert>\n"
        "#include <iostream>\n\n"
        "void test_version()\n"
        "{\n"
        "    assert(!%1::version().empty());\n"
        "    std::cout << \"[PASS] version is not empty\" << std::endl;\n"
        "}\n\n"
        "int main()\n"
        "{\n"
        "    test_version();\n"
        "    std::cout << \"All tests passed!\" << std::endl;\n"
        "    return 0;\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("tests/test_main.cpp")),
                   testMain, error))
        return false;

    // ── .gitignore ───────────────────────────────────────────────────────
    const QString gitignore = QStringLiteral(
        "# Build directories\n"
        "build*/\n"
        "out/\n\n"
        "# IDE / editor\n"
        ".vs/\n"
        ".vscode/\n"
        ".idea/\n"
        "*.user\n"
        "*.suo\n"
        "*.swp\n"
        "*~\n\n"
        "# Compiled objects\n"
        "*.o\n"
        "*.obj\n"
        "*.a\n"
        "*.lib\n"
        "*.so\n"
        "*.dylib\n"
        "*.dll\n"
        "*.exe\n\n"
        "# CMake\n"
        "CMakeCache.txt\n"
        "CMakeFiles/\n"
        "cmake_install.cmake\n"
        "compile_commands.json\n"
        "CTestTestfile.cmake\n"
        "Makefile\n"
        "*.ninja\n");

    if (!writeFile(d.filePath(QStringLiteral(".gitignore")), gitignore, error))
        return false;

    // ── .clang-format ────────────────────────────────────────────────────
    const QString clangfmt = QStringLiteral(
        "BasedOnStyle: LLVM\n"
        "IndentWidth: 4\n"
        "ColumnLimit: 100\n"
        "BreakBeforeBraces: Allman\n"
        "AllowShortFunctionsOnASingleLine: Inline\n"
        "AllowShortIfStatementsOnASingleLine: Never\n"
        "SortIncludes: CaseSensitive\n"
        "IncludeBlocks: Regroup\n");

    if (!writeFile(d.filePath(QStringLiteral(".clang-format")), clangfmt, error))
        return false;

    // ── .clang-tidy ──────────────────────────────────────────────────────
    const QString clangtidy = QStringLiteral(
        "Checks: >-\n"
        "  -*,\n"
        "  bugprone-*,\n"
        "  clang-analyzer-*,\n"
        "  cppcoreguidelines-*,\n"
        "  modernize-*,\n"
        "  performance-*,\n"
        "  readability-*,\n"
        "  -modernize-use-trailing-return-type,\n"
        "  -readability-magic-numbers,\n"
        "  -cppcoreguidelines-avoid-magic-numbers\n"
        "WarningsAsErrors: ''\n"
        "HeaderFilterRegex: 'include/.*'\n");

    if (!writeFile(d.filePath(QStringLiteral(".clang-tidy")), clangtidy, error))
        return false;

    // ── README.md ────────────────────────────────────────────────────────
    const QString readme = QStringLiteral(
        "# %1\n\n"
        "A C++ console application.\n\n"
        "## Build\n\n"
        "```bash\n"
        "cmake --preset debug\n"
        "cmake --build build-debug\n"
        "```\n\n"
        "## Run\n\n"
        "```bash\n"
        "./build-debug/%1\n"
        "```\n\n"
        "## Test\n\n"
        "```bash\n"
        "cd build-debug && ctest --output-on-failure\n"
        "```\n\n"
        "## Project Structure\n\n"
        "```\n"
        "%1/\n"
        "+-- CMakeLists.txt          # Build configuration\n"
        "+-- CMakePresets.json        # Build presets (debug/release)\n"
        "+-- include/%1/             # Public headers\n"
        "+-- src/                     # Source files\n"
        "+-- tests/                   # Unit tests\n"
        "+-- .clang-format            # Code formatting rules\n"
        "+-- .clang-tidy              # Static analysis config\n"
        "```\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("README.md")), readme, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("C++"),
                            QStringLiteral("cpp-console"), QStringLiteral("cmake"),
                            {QStringLiteral("src/main.cpp")},
                            {QStringLiteral("include")}, error);
}

bool BuiltinTemplateProvider::createCppLibrary(const QString &name,
                                               const QString &dir,
                                               QString *error)
{
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src")) || !d.mkpath(QStringLiteral("include"))) {
        if (error) *error = tr("Cannot create directory: %1").arg(dir);
        return false;
    }

    const QString headerGuard = name.toUpper();

    // CMakeLists.txt
    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1 LANGUAGES CXX)\n\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "add_library(%1 STATIC src/%1.cpp)\n"
        "target_include_directories(%1 PUBLIC include)\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    // include/name.h
    const QString header = QStringLiteral(
        "#pragma once\n\n"
        "namespace %1 {\n\n"
        "int version();\n\n"
        "} // namespace %1\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("include/%1.h").arg(name)), header, error))
        return false;

    // src/name.cpp
    const QString source = QStringLiteral(
        "#include \"%1.h\"\n\n"
        "namespace %1 {\n\n"
        "int version() { return 1; }\n\n"
        "} // namespace %1\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/%1.cpp").arg(name)), source, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("C++"),
                            QStringLiteral("cpp-library"), QStringLiteral("cmake"),
                            {QStringLiteral("src/%1.cpp").arg(name)},
                            {QStringLiteral("include")}, error);
}

bool BuiltinTemplateProvider::createCMakeProject(const QString &name,
                                                 const QString &dir,
                                                 QString *error)
{
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src"))) {
        if (error) *error = tr("Cannot create directory: %1").arg(dir);
        return false;
    }

    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1 LANGUAGES CXX)\n\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("C++"),
                            QStringLiteral("cmake-project"), QStringLiteral("cmake"),
                            {}, {QStringLiteral("src")}, error);
}

// ── C ────────────────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createCConsole(const QString &name,
                                             const QString &dir,
                                             QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    d.mkpath(QStringLiteral("src"));

    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1 LANGUAGES C)\n\n"
        "set(CMAKE_C_STANDARD 11)\n"
        "set(CMAKE_C_STANDARD_REQUIRED ON)\n\n"
        "add_executable(%1 src/main.c)\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    const QString main = QStringLiteral(
        "#include <stdio.h>\n\n"
        "int main(void)\n"
        "{\n"
        "    printf(\"Hello, %1!\\n\");\n"
        "    return 0;\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/main.c")), main, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("C"),
                            QStringLiteral("c-console"), QStringLiteral("cmake"),
                            {QStringLiteral("src/main.c")}, {}, error);
}

// ── Rust ─────────────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createRustBinary(const QString &name,
                                               const QString &dir,
                                               QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    d.mkpath(QStringLiteral("src"));

    const QString cargo = QStringLiteral(
        "[package]\n"
        "name = \"%1\"\n"
        "version = \"0.1.0\"\n"
        "edition = \"2021\"\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("Cargo.toml")), cargo, error))
        return false;

    const QString main = QStringLiteral(
        "fn main() {\n"
        "    println!(\"Hello, %1!\");\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/main.rs")), main, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Rust"),
                            QStringLiteral("rust-binary"), QStringLiteral("cargo"),
                            {QStringLiteral("src/main.rs")}, {}, error);
}

bool BuiltinTemplateProvider::createRustLibrary(const QString &name,
                                                const QString &dir,
                                                QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    d.mkpath(QStringLiteral("src"));

    const QString cargo = QStringLiteral(
        "[package]\n"
        "name = \"%1\"\n"
        "version = \"0.1.0\"\n"
        "edition = \"2021\"\n\n"
        "[lib]\n"
        "name = \"%1\"\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("Cargo.toml")), cargo, error))
        return false;

    const QString lib = QStringLiteral(
        "pub fn hello() -> &'static str {\n"
        "    \"Hello, %1!\"\n"
        "}\n\n"
        "#[cfg(test)]\n"
        "mod tests {\n"
        "    use super::*;\n\n"
        "    #[test]\n"
        "    fn it_works() {\n"
        "        assert_eq!(hello(), \"Hello, %1!\");\n"
        "    }\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/lib.rs")), lib, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Rust"),
                            QStringLiteral("rust-library"), QStringLiteral("cargo"),
                            {QStringLiteral("src/lib.rs")}, {}, error);
}

// ── Python ───────────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createPythonScript(const QString &name,
                                                 const QString &dir,
                                                 QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);

    const QString main = QStringLiteral(
        "def main():\n"
        "    print(\"Hello, %1!\")\n\n\n"
        "if __name__ == \"__main__\":\n"
        "    main()\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("main.py")), main, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Python"),
                            QStringLiteral("python-script"), {},
                            {QStringLiteral("main.py")}, {}, error);
}

bool BuiltinTemplateProvider::createPythonPackage(const QString &name,
                                                  const QString &dir,
                                                  QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    const QString pkg = name.toLower();
    d.mkpath(pkg);

    const QString pyproject = QStringLiteral(
        "[project]\n"
        "name = \"%1\"\n"
        "version = \"0.1.0\"\n"
        "description = \"\"\n"
        "requires-python = \">=3.9\"\n").arg(pkg);

    if (!writeFile(d.filePath(QStringLiteral("pyproject.toml")), pyproject, error))
        return false;

    if (!writeFile(d.filePath(QStringLiteral("%1/__init__.py").arg(pkg)),
                   QStringLiteral("\"\"\"Package %1.\"\"\"\n").arg(pkg), error))
        return false;

    if (!writeFile(d.filePath(QStringLiteral("%1/__main__.py").arg(pkg)),
        QStringLiteral("from . import *\n\nprint(\"Hello, %1!\")\n").arg(name), error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Python"),
                            QStringLiteral("python-package"), {},
                            {QStringLiteral("%1/__init__.py").arg(pkg),
                             QStringLiteral("%1/__main__.py").arg(pkg)}, {}, error);
}

// ── Go ───────────────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createGoModule(const QString &name,
                                             const QString &dir,
                                             QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);

    const QString gomod = QStringLiteral(
        "module %1\n\n"
        "go 1.21\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("go.mod")), gomod, error))
        return false;

    const QString main = QStringLiteral(
        "package main\n\n"
        "import \"fmt\"\n\n"
        "func main() {\n"
        "\tfmt.Println(\"Hello, %1!\")\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("main.go")), main, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Go"),
                            QStringLiteral("go-module"), QStringLiteral("go"),
                            {QStringLiteral("main.go")}, {}, error);
}

// ── JavaScript / Node.js ─────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createNodeJs(const QString &name,
                                           const QString &dir,
                                           QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);

    const QString pkg = QStringLiteral(
        "{\n"
        "  \"name\": \"%1\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"main\": \"index.js\",\n"
        "  \"scripts\": {\n"
        "    \"start\": \"node index.js\"\n"
        "  }\n"
        "}\n").arg(name.toLower());

    if (!writeFile(d.filePath(QStringLiteral("package.json")), pkg, error))
        return false;

    const QString index = QStringLiteral(
        "console.log('Hello, %1!');\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("index.js")), index, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("JavaScript"),
                            QStringLiteral("js-node"), QStringLiteral("npm"),
                            {QStringLiteral("index.js")}, {}, error);
}

bool BuiltinTemplateProvider::createWebProject(const QString &name,
                                               const QString &dir,
                                               QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    d.mkpath(QStringLiteral("css"));
    d.mkpath(QStringLiteral("js"));

    const QString html = QStringLiteral(
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "  <title>%1</title>\n"
        "  <link rel=\"stylesheet\" href=\"css/style.css\">\n"
        "</head>\n"
        "<body>\n"
        "  <h1>%1</h1>\n"
        "  <script src=\"js/main.js\"></script>\n"
        "</body>\n"
        "</html>\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("index.html")), html, error))
        return false;
    if (!writeFile(d.filePath(QStringLiteral("css/style.css")),
                   QStringLiteral("body {\n  font-family: sans-serif;\n  margin: 2rem;\n}\n"), error))
        return false;

    if (!writeFile(d.filePath(QStringLiteral("js/main.js")),
        QStringLiteral("console.log('Hello, %1!');\n").arg(name), error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Web"),
                            QStringLiteral("js-web"), {},
                            {QStringLiteral("index.html"),
                             QStringLiteral("css/style.css"),
                             QStringLiteral("js/main.js")}, {}, error);
}

// ── TypeScript ───────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createTypeScriptNode(const QString &name,
                                                   const QString &dir,
                                                   QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    d.mkpath(QStringLiteral("src"));

    const QString pkg = QStringLiteral(
        "{\n"
        "  \"name\": \"%1\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"main\": \"dist/index.js\",\n"
        "  \"scripts\": {\n"
        "    \"build\": \"tsc\",\n"
        "    \"start\": \"node dist/index.js\"\n"
        "  },\n"
        "  \"devDependencies\": {\n"
        "    \"typescript\": \"^5.0.0\"\n"
        "  }\n"
        "}\n").arg(name.toLower());

    if (!writeFile(d.filePath(QStringLiteral("package.json")), pkg, error))
        return false;

    const QString tsconfig = QStringLiteral(
        "{\n"
        "  \"compilerOptions\": {\n"
        "    \"target\": \"ES2020\",\n"
        "    \"module\": \"commonjs\",\n"
        "    \"outDir\": \"dist\",\n"
        "    \"rootDir\": \"src\",\n"
        "    \"strict\": true\n"
        "  }\n"
        "}\n");

    if (!writeFile(d.filePath(QStringLiteral("tsconfig.json")), tsconfig, error))
        return false;

    if (!writeFile(d.filePath(QStringLiteral("src/index.ts")),
        QStringLiteral("console.log('Hello, %1!');\n").arg(name), error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("TypeScript"),
                            QStringLiteral("ts-node"), QStringLiteral("npm"),
                            {QStringLiteral("src/index.ts")}, {}, error);
}

// ── Zig ──────────────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createZigProject(const QString &name,
                                               const QString &dir,
                                               QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    d.mkpath(QStringLiteral("src"));

    const QString buildZig = QStringLiteral(
        "const std = @import(\"std\");\n\n"
        "pub fn build(b: *std.Build) void {\n"
        "    const exe = b.addExecutable(.{\n"
        "        .name = \"%1\",\n"
        "        .root_source_file = b.path(\"src/main.zig\"),\n"
        "        .target = b.standardTargetOptions(.{}),\n"
        "        .optimize = b.standardOptimizeOption(.{}),\n"
        "    });\n"
        "    b.installArtifact(exe);\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("build.zig")), buildZig, error))
        return false;

    const QString main = QStringLiteral(
        "const std = @import(\"std\");\n\n"
        "pub fn main() !void {\n"
        "    const stdout = std.io.getStdOut().writer();\n"
        "    try stdout.print(\"Hello, %1!\\n\", .{});\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/main.zig")), main, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Zig"),
                            QStringLiteral("zig-project"), QStringLiteral("zig"),
                            {QStringLiteral("src/main.zig")}, {}, error);
}

// ── Qt ───────────────────────────────────────────────────────────────────────
//
// All Qt templates use Qt 6 with CMake. Substitution placeholders are
// rendered via QString::arg(name) — the "{{PROJECT_NAME}}" style is mapped
// to %1 / %2 here for parity with the rest of the file.

bool BuiltinTemplateProvider::createQtWidgetApp(const QString &name,
                                                const QString &dir,
                                                QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src"))) {
        if (error) *error = tr("Cannot create project directories: %1").arg(dir);
        return false;
    }

    // ── CMakeLists.txt ───────────────────────────────────────────────────
    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1\n"
        "    VERSION 0.1.0\n"
        "    LANGUAGES CXX\n"
        "    DESCRIPTION \"%1 Qt Widget application\"\n"
        ")\n\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "set(CMAKE_AUTOMOC ON)\n"
        "set(CMAKE_AUTOUIC ON)\n"
        "set(CMAKE_AUTORCC ON)\n\n"
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n"
        "find_package(Qt6 REQUIRED COMPONENTS Widgets)\n\n"
        "qt_standard_project_setup()\n\n"
        "qt_add_executable(%1 WIN32 MACOSX_BUNDLE\n"
        "    src/main.cpp\n"
        "    src/mainwindow.cpp\n"
        "    src/mainwindow.h\n"
        "    src/mainwindow.ui\n"
        ")\n\n"
        "target_link_libraries(%1 PRIVATE Qt6::Widgets)\n\n"
        "install(TARGETS %1\n"
        "    BUNDLE  DESTINATION .\n"
        "    RUNTIME DESTINATION bin\n"
        ")\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    // ── src/main.cpp ─────────────────────────────────────────────────────
    const QString mainCpp = QStringLiteral(
        "#include \"mainwindow.h\"\n\n"
        "#include <QApplication>\n\n"
        "int main(int argc, char *argv[])\n"
        "{\n"
        "    QApplication app(argc, argv);\n"
        "    QApplication::setApplicationName(QStringLiteral(\"%1\"));\n"
        "    QApplication::setOrganizationName(QStringLiteral(\"%1\"));\n\n"
        "    MainWindow w;\n"
        "    w.show();\n"
        "    return app.exec();\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/main.cpp")), mainCpp, error))
        return false;

    // ── src/mainwindow.h ─────────────────────────────────────────────────
    const QString mwH = QStringLiteral(
        "#pragma once\n\n"
        "#include <QMainWindow>\n\n"
        "QT_BEGIN_NAMESPACE\n"
        "namespace Ui { class MainWindow; }\n"
        "QT_END_NAMESPACE\n\n"
        "class MainWindow : public QMainWindow\n"
        "{\n"
        "    Q_OBJECT\n\n"
        "public:\n"
        "    explicit MainWindow(QWidget *parent = nullptr);\n"
        "    ~MainWindow() override;\n\n"
        "private slots:\n"
        "    void onAboutTriggered();\n"
        "    void onQuitTriggered();\n\n"
        "private:\n"
        "    Ui::MainWindow *ui;\n"
        "};\n");

    if (!writeFile(d.filePath(QStringLiteral("src/mainwindow.h")), mwH, error))
        return false;

    // ── src/mainwindow.cpp ───────────────────────────────────────────────
    const QString mwCpp = QStringLiteral(
        "#include \"mainwindow.h\"\n"
        "#include \"ui_mainwindow.h\"\n\n"
        "#include <QAction>\n"
        "#include <QApplication>\n"
        "#include <QMenuBar>\n"
        "#include <QMessageBox>\n"
        "#include <QStatusBar>\n\n"
        "MainWindow::MainWindow(QWidget *parent)\n"
        "    : QMainWindow(parent)\n"
        "    , ui(new Ui::MainWindow)\n"
        "{\n"
        "    ui->setupUi(this);\n"
        "    setWindowTitle(QStringLiteral(\"%1\"));\n\n"
        "    auto *fileMenu = menuBar()->addMenu(tr(\"&File\"));\n"
        "    auto *quitAct  = fileMenu->addAction(tr(\"&Quit\"));\n"
        "    quitAct->setShortcut(QKeySequence::Quit);\n"
        "    connect(quitAct, &QAction::triggered,\n"
        "            this, &MainWindow::onQuitTriggered);\n\n"
        "    auto *helpMenu = menuBar()->addMenu(tr(\"&Help\"));\n"
        "    auto *aboutAct = helpMenu->addAction(tr(\"&About\"));\n"
        "    connect(aboutAct, &QAction::triggered,\n"
        "            this, &MainWindow::onAboutTriggered);\n\n"
        "    statusBar()->showMessage(tr(\"Ready\"));\n"
        "}\n\n"
        "MainWindow::~MainWindow()\n"
        "{\n"
        "    delete ui;\n"
        "}\n\n"
        "void MainWindow::onAboutTriggered()\n"
        "{\n"
        "    QMessageBox::about(this, tr(\"About %1\"),\n"
        "        tr(\"<b>%1</b><br/>A Qt Widget application.\"));\n"
        "}\n\n"
        "void MainWindow::onQuitTriggered()\n"
        "{\n"
        "    QApplication::quit();\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/mainwindow.cpp")), mwCpp, error))
        return false;

    // ── src/mainwindow.ui (Qt Designer XML) ──────────────────────────────
    const QString mwUi = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<ui version=\"4.0\">\n"
        " <class>MainWindow</class>\n"
        " <widget class=\"QMainWindow\" name=\"MainWindow\">\n"
        "  <property name=\"geometry\">\n"
        "   <rect>\n"
        "    <x>0</x>\n"
        "    <y>0</y>\n"
        "    <width>640</width>\n"
        "    <height>480</height>\n"
        "   </rect>\n"
        "  </property>\n"
        "  <property name=\"windowTitle\">\n"
        "   <string>%1</string>\n"
        "  </property>\n"
        "  <widget class=\"QWidget\" name=\"centralwidget\">\n"
        "   <layout class=\"QVBoxLayout\" name=\"verticalLayout\">\n"
        "    <item>\n"
        "     <widget class=\"QLabel\" name=\"label\">\n"
        "      <property name=\"text\">\n"
        "       <string>Welcome to %1!</string>\n"
        "      </property>\n"
        "      <property name=\"alignment\">\n"
        "       <set>Qt::AlignCenter</set>\n"
        "      </property>\n"
        "     </widget>\n"
        "    </item>\n"
        "   </layout>\n"
        "  </widget>\n"
        " </widget>\n"
        " <resources/>\n"
        " <connections/>\n"
        "</ui>\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/mainwindow.ui")), mwUi, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Qt"),
                            QStringLiteral("qt-widget-app"),
                            QStringLiteral("cmake"),
                            {QStringLiteral("src/main.cpp"),
                             QStringLiteral("src/mainwindow.cpp"),
                             QStringLiteral("src/mainwindow.h"),
                             QStringLiteral("src/mainwindow.ui")},
                            {QStringLiteral("src")}, error);
}

bool BuiltinTemplateProvider::createQtQuickApp(const QString &name,
                                               const QString &dir,
                                               QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src")) ||
        !d.mkpath(QStringLiteral("qml"))) {
        if (error) *error = tr("Cannot create project directories: %1").arg(dir);
        return false;
    }

    // ── CMakeLists.txt ───────────────────────────────────────────────────
    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1\n"
        "    VERSION 0.1.0\n"
        "    LANGUAGES CXX\n"
        "    DESCRIPTION \"%1 Qt Quick application\"\n"
        ")\n\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "set(CMAKE_AUTOMOC ON)\n"
        "set(CMAKE_AUTOUIC ON)\n"
        "set(CMAKE_AUTORCC ON)\n\n"
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n"
        "find_package(Qt6 REQUIRED COMPONENTS Quick Qml)\n\n"
        "qt_standard_project_setup(REQUIRES 6.5)\n\n"
        "qt_add_executable(%1app\n"
        "    src/main.cpp\n"
        ")\n\n"
        "qt_add_qml_module(%1app\n"
        "    URI %1\n"
        "    VERSION 1.0\n"
        "    QML_FILES\n"
        "        qml/Main.qml\n"
        ")\n\n"
        "target_link_libraries(%1app PRIVATE\n"
        "    Qt6::Quick\n"
        "    Qt6::Qml\n"
        ")\n\n"
        "install(TARGETS %1app\n"
        "    BUNDLE  DESTINATION .\n"
        "    RUNTIME DESTINATION bin\n"
        ")\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    // ── src/main.cpp ─────────────────────────────────────────────────────
    const QString mainCpp = QStringLiteral(
        "#include <QGuiApplication>\n"
        "#include <QQmlApplicationEngine>\n\n"
        "int main(int argc, char *argv[])\n"
        "{\n"
        "    QGuiApplication app(argc, argv);\n"
        "    QGuiApplication::setApplicationName(QStringLiteral(\"%1\"));\n"
        "    QGuiApplication::setOrganizationName(QStringLiteral(\"%1\"));\n\n"
        "    QQmlApplicationEngine engine;\n"
        "    QObject::connect(\n"
        "        &engine, &QQmlApplicationEngine::objectCreationFailed,\n"
        "        &app,    []() { QCoreApplication::exit(-1); },\n"
        "        Qt::QueuedConnection);\n\n"
        "    engine.loadFromModule(QStringLiteral(\"%1\"),\n"
        "                          QStringLiteral(\"Main\"));\n\n"
        "    return app.exec();\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/main.cpp")), mainCpp, error))
        return false;

    // ── qml/Main.qml ─────────────────────────────────────────────────────
    const QString mainQml = QStringLiteral(
        "import QtQuick\n"
        "import QtQuick.Window\n"
        "import QtQuick.Controls\n\n"
        "Window {\n"
        "    width: 640\n"
        "    height: 480\n"
        "    visible: true\n"
        "    title: qsTr(\"%1\")\n\n"
        "    Text {\n"
        "        anchors.centerIn: parent\n"
        "        text: qsTr(\"Welcome to %1!\")\n"
        "        font.pixelSize: 24\n"
        "    }\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("qml/Main.qml")), mainQml, error))
        return false;

    // ── qml.qrc (legacy resource — kept for reference; modern builds use\n
    //   qt_add_qml_module which auto-generates the resource at /qt/qml/<name>/) ─
    const QString qmlQrc = QStringLiteral(
        "<RCC>\n"
        "    <qresource prefix=\"/qt/qml/%1/\">\n"
        "        <file alias=\"Main.qml\">qml/Main.qml</file>\n"
        "    </qresource>\n"
        "</RCC>\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("qml.qrc")), qmlQrc, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Qt"),
                            QStringLiteral("qt-quick-app"),
                            QStringLiteral("cmake"),
                            {QStringLiteral("src/main.cpp"),
                             QStringLiteral("qml/Main.qml")},
                            {QStringLiteral("src"), QStringLiteral("qml")},
                            error);
}

bool BuiltinTemplateProvider::createQtPlugin(const QString &name,
                                             const QString &dir,
                                             QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src"))) {
        if (error) *error = tr("Cannot create project directories: %1").arg(dir);
        return false;
    }

    // ── CMakeLists.txt ───────────────────────────────────────────────────
    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1\n"
        "    VERSION 0.1.0\n"
        "    LANGUAGES CXX\n"
        "    DESCRIPTION \"%1 Qt plugin\"\n"
        ")\n\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "set(CMAKE_AUTOMOC ON)\n\n"
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n"
        "find_package(Qt6 REQUIRED COMPONENTS Core)\n\n"
        "add_library(%1 MODULE\n"
        "    src/plugin.cpp\n"
        "    src/plugin.h\n"
        ")\n\n"
        "target_include_directories(%1 PUBLIC\n"
        "    ${CMAKE_CURRENT_SOURCE_DIR}/src\n"
        ")\n\n"
        "target_link_libraries(%1 PRIVATE Qt6::Core)\n\n"
        "set_target_properties(%1 PROPERTIES\n"
        "    PREFIX \"\"\n"
        ")\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    // ── src/plugin.h ─────────────────────────────────────────────────────
    const QString pluginH = QStringLiteral(
        "#pragma once\n\n"
        "#include <QObject>\n"
        "#include <QtPlugin>\n\n"
        "// ── Generic plugin interface ────────────────────────────────────\n"
        "class IPlugin\n"
        "{\n"
        "public:\n"
        "    virtual ~IPlugin() = default;\n"
        "    virtual QString name() const    = 0;\n"
        "    virtual QString version() const = 0;\n"
        "    virtual bool    initialize()    = 0;\n"
        "    virtual void    shutdown()      = 0;\n"
        "};\n\n"
        "#define %2_PLUGIN_IID \"org.example.%1.IPlugin/1.0\"\n"
        "Q_DECLARE_INTERFACE(IPlugin, %2_PLUGIN_IID)\n\n"
        "// ── Plugin implementation ───────────────────────────────────────\n"
        "class %1Plugin : public QObject, public IPlugin\n"
        "{\n"
        "    Q_OBJECT\n"
        "    Q_PLUGIN_METADATA(IID %2_PLUGIN_IID FILE \"plugin.json\")\n"
        "    Q_INTERFACES(IPlugin)\n\n"
        "public:\n"
        "    explicit %1Plugin(QObject *parent = nullptr);\n"
        "    ~%1Plugin() override;\n\n"
        "    QString name()    const override;\n"
        "    QString version() const override;\n"
        "    bool    initialize() override;\n"
        "    void    shutdown()   override;\n"
        "};\n").arg(name, name.toUpper());

    if (!writeFile(d.filePath(QStringLiteral("src/plugin.h")), pluginH, error))
        return false;

    // ── src/plugin.cpp ───────────────────────────────────────────────────
    const QString pluginCpp = QStringLiteral(
        "#include \"plugin.h\"\n\n"
        "#include <QDebug>\n\n"
        "%1Plugin::%1Plugin(QObject *parent)\n"
        "    : QObject(parent)\n"
        "{\n"
        "}\n\n"
        "%1Plugin::~%1Plugin() = default;\n\n"
        "QString %1Plugin::name() const\n"
        "{\n"
        "    return QStringLiteral(\"%1\");\n"
        "}\n\n"
        "QString %1Plugin::version() const\n"
        "{\n"
        "    return QStringLiteral(\"0.1.0\");\n"
        "}\n\n"
        "bool %1Plugin::initialize()\n"
        "{\n"
        "    qDebug() << \"%1Plugin: initialize()\";\n"
        "    return true;\n"
        "}\n\n"
        "void %1Plugin::shutdown()\n"
        "{\n"
        "    qDebug() << \"%1Plugin: shutdown()\";\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/plugin.cpp")), pluginCpp, error))
        return false;

    // ── plugin.json (metadata) ───────────────────────────────────────────
    const QString pluginJson = QStringLiteral(
        "{\n"
        "    \"name\":        \"%1\",\n"
        "    \"version\":     \"0.1.0\",\n"
        "    \"description\": \"%1 Qt plugin\",\n"
        "    \"author\":      \"\",\n"
        "    \"license\":     \"\",\n"
        "    \"dependencies\": []\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("plugin.json")), pluginJson, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Qt"),
                            QStringLiteral("qt-plugin"),
                            QStringLiteral("cmake"),
                            {QStringLiteral("src/plugin.cpp"),
                             QStringLiteral("src/plugin.h")},
                            {QStringLiteral("src")}, error);
}

bool BuiltinTemplateProvider::createQtConsoleApp(const QString &name,
                                                 const QString &dir,
                                                 QString *error)
{
    if (!ensureDir(dir, error)) return false;
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src"))) {
        if (error) *error = tr("Cannot create project directories: %1").arg(dir);
        return false;
    }

    // ── CMakeLists.txt ───────────────────────────────────────────────────
    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1\n"
        "    VERSION 0.1.0\n"
        "    LANGUAGES CXX\n"
        "    DESCRIPTION \"%1 Qt console application\"\n"
        ")\n\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "set(CMAKE_AUTOMOC ON)\n\n"
        "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n"
        "find_package(Qt6 REQUIRED COMPONENTS Core)\n\n"
        "qt_standard_project_setup()\n\n"
        "qt_add_executable(%1\n"
        "    src/main.cpp\n"
        ")\n\n"
        "target_link_libraries(%1 PRIVATE Qt6::Core)\n\n"
        "install(TARGETS %1 RUNTIME DESTINATION bin)\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    // ── src/main.cpp ─────────────────────────────────────────────────────
    const QString mainCpp = QStringLiteral(
        "#include <QCoreApplication>\n"
        "#include <QDebug>\n"
        "#include <QObject>\n"
        "#include <QTimer>\n\n"
        "// Tiny worker that demonstrates the Qt event loop +\n"
        "// signals/slots in a non-GUI program.\n"
        "class Worker : public QObject\n"
        "{\n"
        "    Q_OBJECT\n"
        "public:\n"
        "    explicit Worker(QObject *parent = nullptr) : QObject(parent) {}\n\n"
        "public slots:\n"
        "    void run()\n"
        "    {\n"
        "        qInfo().noquote() << QStringLiteral(\"Hello from %1!\");\n"
        "        emit finished();\n"
        "    }\n\n"
        "signals:\n"
        "    void finished();\n"
        "};\n\n"
        "#include \"main.moc\"\n\n"
        "int main(int argc, char *argv[])\n"
        "{\n"
        "    QCoreApplication app(argc, argv);\n"
        "    QCoreApplication::setApplicationName(QStringLiteral(\"%1\"));\n\n"
        "    Worker worker;\n"
        "    QObject::connect(&worker, &Worker::finished,\n"
        "                     &app,    &QCoreApplication::quit);\n\n"
        "    // Kick off after the event loop has started.\n"
        "    QTimer::singleShot(0, &worker, &Worker::run);\n\n"
        "    return app.exec();\n"
        "}\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("src/main.cpp")), mainCpp, error))
        return false;

    return writeProjectFile(dir, name, QStringLiteral("Qt"),
                            QStringLiteral("qt-console-app"),
                            QStringLiteral("cmake"),
                            {QStringLiteral("src/main.cpp")},
                            {QStringLiteral("src")}, error);
}
