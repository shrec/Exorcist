#include "builtintemplateprovider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
         tr("A C++ console application with CMake build system."),
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

    // ── Generic single-file scaffolds ────────────────────────────────────
    // Java
    if (templateId == QLatin1String("java-console")) {
        const QString cls = projectName;
        cls[0].toUpper();
        return createGenericProject(projectName, location,
            QStringLiteral("src/Main.java"),
            QStringLiteral("public class Main {\n"
                           "    public static void main(String[] args) {\n"
                           "        System.out.println(\"Hello, %1!\");\n"
                           "    }\n"
                           "}\n").arg(projectName), error);
    }
    // C#
    if (templateId == QLatin1String("csharp-console"))
        return createGenericProject(projectName, location,
            QStringLiteral("Program.cs"),
            QStringLiteral("Console.WriteLine(\"Hello, %1!\");\n").arg(projectName),
            error);
    // Swift
    if (templateId == QLatin1String("swift-package"))
        return createGenericProject(projectName, location,
            QStringLiteral("Sources/main.swift"),
            QStringLiteral("print(\"Hello, %1!\")\n").arg(projectName), error);
    // Kotlin
    if (templateId == QLatin1String("kotlin-jvm"))
        return createGenericProject(projectName, location,
            QStringLiteral("src/main/kotlin/Main.kt"),
            QStringLiteral("fun main() {\n"
                           "    println(\"Hello, %1!\")\n"
                           "}\n").arg(projectName), error);
    // Lua
    if (templateId == QLatin1String("lua-script"))
        return createGenericProject(projectName, location,
            QStringLiteral("main.lua"),
            QStringLiteral("print(\"Hello, %1!\")\n").arg(projectName), error);
    // Ruby
    if (templateId == QLatin1String("ruby-script"))
        return createGenericProject(projectName, location,
            QStringLiteral("main.rb"),
            QStringLiteral("puts \"Hello, %1!\"\n").arg(projectName), error);
    // PHP
    if (templateId == QLatin1String("php-project"))
        return createGenericProject(projectName, location,
            QStringLiteral("index.php"),
            QStringLiteral("<?php\necho \"Hello, %1!\\n\";\n").arg(projectName), error);
    // Dart
    if (templateId == QLatin1String("dart-console"))
        return createGenericProject(projectName, location,
            QStringLiteral("bin/main.dart"),
            QStringLiteral("void main() {\n"
                           "  print('Hello, %1!');\n"
                           "}\n").arg(projectName), error);
    // Haskell
    if (templateId == QLatin1String("haskell-stack"))
        return createGenericProject(projectName, location,
            QStringLiteral("app/Main.hs"),
            QStringLiteral("module Main where\n\n"
                           "main :: IO ()\n"
                           "main = putStrLn \"Hello, %1!\"\n").arg(projectName), error);
    // Scala
    if (templateId == QLatin1String("scala-sbt"))
        return createGenericProject(projectName, location,
            QStringLiteral("src/main/scala/Main.scala"),
            QStringLiteral("object Main extends App {\n"
                           "  println(\"Hello, %1!\")\n"
                           "}\n").arg(projectName), error);

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

// ── Generic scaffold ─────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createGenericProject(const QString &name,
                                                   const QString &dir,
                                                   const QString &filename,
                                                   const QString &content,
                                                   QString *error)
{
    Q_UNUSED(name)
    if (!ensureDir(dir, error))
        return false;
    return writeFile(QDir(dir).filePath(filename), content, error);
}

// ── C++ ──────────────────────────────────────────────────────────────────────

bool BuiltinTemplateProvider::createCppConsole(const QString &name,
                                               const QString &dir,
                                               QString *error)
{
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("src"))) {
        if (error) *error = tr("Cannot create directory: %1").arg(dir);
        return false;
    }

    // CMakeLists.txt
    const QString cmake = QStringLiteral(
        "cmake_minimum_required(VERSION 3.21)\n"
        "project(%1 LANGUAGES CXX)\n\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
        "add_executable(%1 src/main.cpp)\n").arg(name);

    if (!writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error))
        return false;

    // src/main.cpp
    const QString main = QStringLiteral(
        "#include <iostream>\n\n"
        "int main()\n"
        "{\n"
        "    std::cout << \"Hello, %1!\" << std::endl;\n"
        "    return 0;\n"
        "}\n").arg(name);

    return writeFile(d.filePath(QStringLiteral("src/main.cpp")), main, error);
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

    return writeFile(d.filePath(QStringLiteral("src/%1.cpp").arg(name)), source, error);
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

    return writeFile(d.filePath(QStringLiteral("CMakeLists.txt")), cmake, error);
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

    return writeFile(d.filePath(QStringLiteral("src/main.c")), main, error);
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

    return writeFile(d.filePath(QStringLiteral("src/main.rs")), main, error);
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

    return writeFile(d.filePath(QStringLiteral("src/lib.rs")), lib, error);
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

    return writeFile(d.filePath(QStringLiteral("main.py")), main, error);
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

    return writeFile(d.filePath(QStringLiteral("%1/__main__.py").arg(pkg)),
        QStringLiteral("from . import *\n\nprint(\"Hello, %1!\")\n").arg(name), error);
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

    return writeFile(d.filePath(QStringLiteral("main.go")), main, error);
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

    return writeFile(d.filePath(QStringLiteral("index.js")), index, error);
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

    return writeFile(d.filePath(QStringLiteral("js/main.js")),
        QStringLiteral("console.log('Hello, %1!');\n").arg(name), error);
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

    return writeFile(d.filePath(QStringLiteral("src/index.ts")),
        QStringLiteral("console.log('Hello, %1!');\n").arg(name), error);
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

    return writeFile(d.filePath(QStringLiteral("src/main.zig")), main, error);
}
