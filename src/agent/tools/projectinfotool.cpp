#include "projectinfotool.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

ToolSpec GetProjectSetupInfoTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("get_project_setup_info");
    s.description = QStringLiteral(
        "Get information about the project setup: detected languages, "
        "build system, package managers, configuration files, entry points, "
        "and other setup details. Useful for understanding a project before "
        "making changes.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
    };
    return s;
}

ToolExecResult GetProjectSetupInfoTool::invoke(const QJsonObject & /*args*/)
{
    if (m_root.isEmpty())
        return {false, {}, {},
                QStringLiteral("No workspace root configured.")};

    QDir rootDir(m_root);
    if (!rootDir.exists())
        return {false, {}, {},
                QStringLiteral("Workspace root does not exist: %1").arg(m_root)};

    QJsonObject info;
    info[QStringLiteral("workspaceRoot")] = m_root;

    // ── Detect config files ──────────────────────────────────────
    QStringList configFiles;
    const QStringList knownConfigs = {
        QStringLiteral("CMakeLists.txt"),
        QStringLiteral("Makefile"),
        QStringLiteral("meson.build"),
        QStringLiteral("build.gradle"),
        QStringLiteral("pom.xml"),
        QStringLiteral("package.json"),
        QStringLiteral("Cargo.toml"),
        QStringLiteral("go.mod"),
        QStringLiteral("pyproject.toml"),
        QStringLiteral("setup.py"),
        QStringLiteral("setup.cfg"),
        QStringLiteral("requirements.txt"),
        QStringLiteral("Pipfile"),
        QStringLiteral("Gemfile"),
        QStringLiteral(".csproj"),
        QStringLiteral(".sln"),
        QStringLiteral("tsconfig.json"),
        QStringLiteral("webpack.config.js"),
        QStringLiteral("vite.config.ts"),
        QStringLiteral("vite.config.js"),
        QStringLiteral(".eslintrc.json"),
        QStringLiteral(".eslintrc.js"),
        QStringLiteral("Dockerfile"),
        QStringLiteral("docker-compose.yml"),
        QStringLiteral("docker-compose.yaml"),
        QStringLiteral(".gitignore"),
        QStringLiteral("CMakePresets.json"),
    };

    for (const QString &name : knownConfigs) {
        if (rootDir.exists(name))
            configFiles.append(name);
    }
    // Also search for .sln / .csproj (globbed)
    {
        const QStringList slnFiles = rootDir.entryList(
            {QStringLiteral("*.sln")}, QDir::Files);
        for (const QString &f : slnFiles)
            configFiles.append(f);
        const QStringList csprojFiles = rootDir.entryList(
            {QStringLiteral("*.csproj")}, QDir::Files);
        for (const QString &f : csprojFiles)
            configFiles.append(f);
    }

    configFiles.removeDuplicates();
    info[QStringLiteral("configFiles")] = QJsonArray::fromStringList(configFiles);

    // ── Detect build system ──────────────────────────────────────
    QString buildSystem;
    if (configFiles.contains(QStringLiteral("CMakeLists.txt")))
        buildSystem = QStringLiteral("CMake");
    else if (configFiles.contains(QStringLiteral("Makefile")))
        buildSystem = QStringLiteral("Make");
    else if (configFiles.contains(QStringLiteral("meson.build")))
        buildSystem = QStringLiteral("Meson");
    else if (configFiles.contains(QStringLiteral("build.gradle")))
        buildSystem = QStringLiteral("Gradle");
    else if (configFiles.contains(QStringLiteral("pom.xml")))
        buildSystem = QStringLiteral("Maven");
    else if (configFiles.contains(QStringLiteral("Cargo.toml")))
        buildSystem = QStringLiteral("Cargo");
    else if (configFiles.contains(QStringLiteral("go.mod")))
        buildSystem = QStringLiteral("Go Modules");
    else if (configFiles.contains(QStringLiteral("package.json")))
        buildSystem = QStringLiteral("npm/yarn");

    if (!buildSystem.isEmpty())
        info[QStringLiteral("buildSystem")] = buildSystem;

    // ── Detect languages ─────────────────────────────────────────
    QSet<QString> languages;
    QHash<QString, int> extCounts;
    int fileCount = 0;

    QDirIterator it(m_root, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext() && fileCount < 2000) {
        const QString path = it.next();
        // Skip build / hidden / vendor dirs
        if (path.contains(QLatin1String("/.git")) ||
            path.contains(QLatin1String("\\.git")) ||
            path.contains(QLatin1String("/build")) ||
            path.contains(QLatin1String("\\build")) ||
            path.contains(QLatin1String("/node_modules")) ||
            path.contains(QLatin1String("\\node_modules")) ||
            path.contains(QLatin1String("/target")) ||
            path.contains(QLatin1String("\\target")))
            continue;

        ++fileCount;
        const QString suffix = QFileInfo(path).suffix().toLower();
        if (!suffix.isEmpty())
            extCounts[suffix]++;
    }

    // Map extensions to languages
    static const QHash<QString, QString> extToLang = {
        {QStringLiteral("cpp"), QStringLiteral("C++")},
        {QStringLiteral("cc"),  QStringLiteral("C++")},
        {QStringLiteral("cxx"), QStringLiteral("C++")},
        {QStringLiteral("c"),   QStringLiteral("C")},
        {QStringLiteral("h"),   QStringLiteral("C/C++")},
        {QStringLiteral("hpp"), QStringLiteral("C++")},
        {QStringLiteral("py"),  QStringLiteral("Python")},
        {QStringLiteral("js"),  QStringLiteral("JavaScript")},
        {QStringLiteral("ts"),  QStringLiteral("TypeScript")},
        {QStringLiteral("jsx"), QStringLiteral("JavaScript (React)")},
        {QStringLiteral("tsx"), QStringLiteral("TypeScript (React)")},
        {QStringLiteral("rs"),  QStringLiteral("Rust")},
        {QStringLiteral("go"),  QStringLiteral("Go")},
        {QStringLiteral("java"),QStringLiteral("Java")},
        {QStringLiteral("kt"),  QStringLiteral("Kotlin")},
        {QStringLiteral("cs"),  QStringLiteral("C#")},
        {QStringLiteral("rb"),  QStringLiteral("Ruby")},
        {QStringLiteral("swift"),QStringLiteral("Swift")},
        {QStringLiteral("lua"), QStringLiteral("Lua")},
        {QStringLiteral("html"),QStringLiteral("HTML")},
        {QStringLiteral("css"), QStringLiteral("CSS")},
        {QStringLiteral("scss"),QStringLiteral("SCSS")},
        {QStringLiteral("vue"), QStringLiteral("Vue")},
        {QStringLiteral("svelte"), QStringLiteral("Svelte")},
    };

    for (auto jt = extCounts.constBegin(); jt != extCounts.constEnd(); ++jt) {
        const auto langIt = extToLang.constFind(jt.key());
        if (langIt != extToLang.constEnd() && jt.value() >= 2)
            languages.insert(langIt.value());
    }

    QStringList langList(languages.begin(), languages.end());
    langList.sort();
    info[QStringLiteral("languages")] = QJsonArray::fromStringList(langList);
    info[QStringLiteral("totalFiles")] = fileCount;

    // ── Read package.json scripts if present ─────────────────────
    if (configFiles.contains(QStringLiteral("package.json"))) {
        QFile pkgFile(rootDir.filePath(QStringLiteral("package.json")));
        if (pkgFile.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(pkgFile.readAll());
            const QJsonObject pkg = doc.object();
            if (pkg.contains(QStringLiteral("scripts")))
                info[QStringLiteral("npmScripts")] =
                    pkg[QStringLiteral("scripts")].toObject();
            if (pkg.contains(QStringLiteral("name")))
                info[QStringLiteral("projectName")] =
                    pkg[QStringLiteral("name")].toString();
        }
    }

    // ── Read CMakePresets.json if present ────────────────────────
    if (configFiles.contains(QStringLiteral("CMakePresets.json"))) {
        QFile presetsFile(rootDir.filePath(QStringLiteral("CMakePresets.json")));
        if (presetsFile.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(presetsFile.readAll());
            const QJsonObject presets = doc.object();
            QJsonArray presetNames;
            const auto configPresets = presets[QStringLiteral("configurePresets")].toArray();
            for (const auto &p : configPresets) {
                presetNames.append(p.toObject()[QStringLiteral("name")].toString());
            }
            if (!presetNames.isEmpty())
                info[QStringLiteral("cmakePresets")] = presetNames;
        }
    }

    // ── Top-level directory listing ──────────────────────────────
    const QStringList topEntries = rootDir.entryList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    QStringList topDirs, topFiles;
    for (const QString &e : topEntries) {
        if (QFileInfo(rootDir.filePath(e)).isDir())
            topDirs.append(e + QLatin1Char('/'));
        else
            topFiles.append(e);
    }
    info[QStringLiteral("topLevelDirs")] = QJsonArray::fromStringList(topDirs);

    // Format output
    const QString text = QJsonDocument(info).toJson(QJsonDocument::Indented);
    return {true, info, text, {}};
}
