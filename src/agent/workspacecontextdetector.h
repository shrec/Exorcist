#pragma once

#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QStringList>

// ── WorkspaceContextDetector ──────────────────────────────────────────────────
// Scans workspace files to determine which language/framework contexts are
// active. Used by ToolRegistry to filter managed tools.
//
// Detection is fast: checks first ~500 files, plus root-level config files.

class WorkspaceContextDetector
{
public:
    /// Scan the workspace root and return detected contexts.
    /// Examples: "python", "cpp", "c", "web", "node", "rust", "go",
    ///           "java", "csharp", "notebook"
    static QSet<QString> detect(const QString &workspaceRoot)
    {
        QSet<QString> ctx;
        if (workspaceRoot.isEmpty())
            return ctx;

        // ── Root-level config file checks (instant) ───────────────────────
        const QDir root(workspaceRoot);

        // Python
        if (root.exists(QStringLiteral("setup.py"))
            || root.exists(QStringLiteral("setup.cfg"))
            || root.exists(QStringLiteral("pyproject.toml"))
            || root.exists(QStringLiteral("Pipfile"))
            || root.exists(QStringLiteral("requirements.txt")))
            ctx.insert(QStringLiteral("python"));

        // Node / Web
        if (root.exists(QStringLiteral("package.json")))
            ctx.insert(QStringLiteral("node"));
        if (root.exists(QStringLiteral("tsconfig.json"))
            || root.exists(QStringLiteral("next.config.js"))
            || root.exists(QStringLiteral("vite.config.ts"))
            || root.exists(QStringLiteral("webpack.config.js"))
            || root.exists(QStringLiteral("angular.json")))
            ctx.insert(QStringLiteral("web"));

        // Rust
        if (root.exists(QStringLiteral("Cargo.toml")))
            ctx.insert(QStringLiteral("rust"));

        // Go
        if (root.exists(QStringLiteral("go.mod")))
            ctx.insert(QStringLiteral("go"));

        // Java / Kotlin
        if (root.exists(QStringLiteral("pom.xml"))
            || root.exists(QStringLiteral("build.gradle"))
            || root.exists(QStringLiteral("build.gradle.kts")))
            ctx.insert(QStringLiteral("java"));

        // C# / .NET
        if (!root.entryList({QStringLiteral("*.csproj"), QStringLiteral("*.sln")},
                            QDir::Files).isEmpty())
            ctx.insert(QStringLiteral("csharp"));

        // CMake / C/C++
        if (root.exists(QStringLiteral("CMakeLists.txt"))
            || root.exists(QStringLiteral("Makefile"))
            || root.exists(QStringLiteral("meson.build")))
            ctx.insert(QStringLiteral("cpp"));

        // ── File extension scan (cap at ~500 files) ───────────────────────
        QDirIterator it(workspaceRoot, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        int scanned = 0;
        const int maxScan = 500;

        while (it.hasNext() && scanned < maxScan) {
            it.next();
            ++scanned;

            const QString suffix = it.fileInfo().suffix().toLower();

            if (suffix == QLatin1String("py") || suffix == QLatin1String("pyi"))
                ctx.insert(QStringLiteral("python"));
            else if (suffix == QLatin1String("ipynb"))
                ctx.insert(QStringLiteral("notebook"));
            else if (suffix == QLatin1String("cpp") || suffix == QLatin1String("cxx")
                     || suffix == QLatin1String("cc") || suffix == QLatin1String("hpp")
                     || suffix == QLatin1String("hxx"))
                ctx.insert(QStringLiteral("cpp"));
            else if (suffix == QLatin1String("c") || suffix == QLatin1String("h"))
                ctx.insert(QStringLiteral("c"));
            else if (suffix == QLatin1String("rs"))
                ctx.insert(QStringLiteral("rust"));
            else if (suffix == QLatin1String("go"))
                ctx.insert(QStringLiteral("go"));
            else if (suffix == QLatin1String("java") || suffix == QLatin1String("kt"))
                ctx.insert(QStringLiteral("java"));
            else if (suffix == QLatin1String("cs"))
                ctx.insert(QStringLiteral("csharp"));
            else if (suffix == QLatin1String("ts") || suffix == QLatin1String("tsx")
                     || suffix == QLatin1String("jsx"))
                ctx.insert(QStringLiteral("web"));
            else if (suffix == QLatin1String("js") || suffix == QLatin1String("mjs"))
                ctx.insert(QStringLiteral("node"));
            else if (suffix == QLatin1String("rb"))
                ctx.insert(QStringLiteral("ruby"));
            else if (suffix == QLatin1String("swift"))
                ctx.insert(QStringLiteral("swift"));
            else if (suffix == QLatin1String("lua"))
                ctx.insert(QStringLiteral("lua"));
        }

        // If "node" is detected, also mark "web" (overlap is common)
        if (ctx.contains(QStringLiteral("web")) && !ctx.contains(QStringLiteral("node")))
            ctx.insert(QStringLiteral("node"));

        return ctx;
    }
};
