#include "cmakeintegration.h"
#include "toolchainmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

CMakeIntegration::CMakeIntegration(QObject *parent)
    : QObject(parent)
{
}

// ── Project root ──────────────────────────────────────────────────────────────

void CMakeIntegration::setProjectRoot(const QString &root)
{
    m_projectRoot = root;
}

bool CMakeIntegration::hasCMakeProject() const
{
    return QFileInfo::exists(m_projectRoot + QStringLiteral("/CMakeLists.txt"));
}

bool CMakeIntegration::hasPresets() const
{
    return QFileInfo::exists(m_projectRoot + QStringLiteral("/CMakePresets.json"))
        || QFileInfo::exists(m_projectRoot + QStringLiteral("/CMakeUserPresets.json"));
}

// ── Presets ───────────────────────────────────────────────────────────────────

QStringList CMakeIntegration::detectPresets() const
{
    QStringList presets;

    auto parsePresetFile = [&](const QString &path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;

        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        const QJsonArray configPresets = doc.object()
            .value(QLatin1String("configurePresets")).toArray();

        for (const auto &val : configPresets) {
            const QJsonObject obj = val.toObject();
            if (obj.value(QLatin1String("hidden")).toBool()) continue;
            const QString name = obj.value(QLatin1String("name")).toString();
            if (!name.isEmpty())
                presets.append(name);
        }
    };

    parsePresetFile(m_projectRoot + QStringLiteral("/CMakePresets.json"));
    parsePresetFile(m_projectRoot + QStringLiteral("/CMakeUserPresets.json"));

    return presets;
}

// ── Build config management ───────────────────────────────────────────────────

void CMakeIntegration::setActiveBuildConfig(int index)
{
    if (index >= 0 && index < m_configs.size()) {
        m_activeConfig = index;
        // Persist selection per project
        if (!m_projectRoot.isEmpty()) {
            QSettings settings;
            const QString key = QStringLiteral("cmake/activePreset/")
                + QString::fromUtf8(m_projectRoot.toUtf8().toBase64());
            const auto &cfg = m_configs[index];
            settings.setValue(key, cfg.cmakePreset.isEmpty() ? cfg.name : cfg.cmakePreset);
        }
    }
}

BuildConfig CMakeIntegration::activeBuildConfig() const
{
    if (m_activeConfig >= 0 && m_activeConfig < m_configs.size())
        return m_configs.at(m_activeConfig);
    return {};
}

void CMakeIntegration::autoDetectConfigs()
{
    m_configs.clear();

    // If presets exist, use them — skip presets whose explicit paths don't exist locally
    if (hasPresets()) {
        const QStringList presets = detectPresets();

        // Parse preset file once for all presets
        QJsonArray allPresets;
        QFile f(m_projectRoot + QStringLiteral("/CMakePresets.json"));
        if (f.open(QIODevice::ReadOnly)) {
            allPresets = QJsonDocument::fromJson(f.readAll()).object()
                .value(QLatin1String("configurePresets")).toArray();
        }

        for (const auto &preset : presets) {
            BuildConfig cfg;
            cfg.cmakePreset = preset;
            bool viable = true;

            for (const auto &val : allPresets) {
                const QJsonObject obj = val.toObject();
                if (obj.value(QLatin1String("name")).toString() != preset)
                    continue;

                cfg.buildDir = obj.value(QLatin1String("binaryDir")).toString();
                cfg.generator = obj.value(QLatin1String("generator")).toString();
                const QString displayName =
                    obj.value(QLatin1String("displayName")).toString();
                cfg.name = displayName.isEmpty() ? preset : displayName;
                cfg.buildDir.replace(
                    QLatin1String("${sourceDir}"), m_projectRoot);

                // Check if preset-specific paths exist on this machine
                const QJsonObject vars =
                    obj.value(QLatin1String("cacheVariables")).toObject();
                const QString prefixPath =
                    vars.value(QLatin1String("CMAKE_PREFIX_PATH")).toString();
                const QString cCompiler =
                    vars.value(QLatin1String("CMAKE_C_COMPILER")).toString();
                const QString cxxCompiler =
                    vars.value(QLatin1String("CMAKE_CXX_COMPILER")).toString();

                if (!prefixPath.isEmpty() && !QDir(prefixPath).exists())
                    viable = false;
                if (!cCompiler.isEmpty() && !QFileInfo::exists(cCompiler))
                    viable = false;
                if (!cxxCompiler.isEmpty() && !QFileInfo::exists(cxxCompiler))
                    viable = false;

                // Presets with no explicit paths require Qt findable via system —
                // skip them unless they have a matching build directory already
                if (prefixPath.isEmpty() && cCompiler.isEmpty()) {
                    const QString bd = cfg.buildDir.isEmpty()
                        ? m_projectRoot + QStringLiteral("/build-") + preset
                        : cfg.buildDir;
                    if (!QDir(bd).exists())
                        viable = false;
                }

                break;
            }

            if (!viable)
                continue;

            if (cfg.name.isEmpty())
                cfg.name = preset;
            if (cfg.buildDir.isEmpty())
                cfg.buildDir = m_projectRoot + QStringLiteral("/build-") + preset;

            m_configs.append(cfg);
        }
    }

    // If no presets, check for existing build directories
    if (m_configs.isEmpty()) {
        const QStringList buildDirNames = {
            QStringLiteral("build"),
            QStringLiteral("build-debug"),
            QStringLiteral("build-release"),
            QStringLiteral("build-llvm"),
            QStringLiteral("cmake-build-debug"),
            QStringLiteral("cmake-build-release"),
        };

        for (const auto &dirName : buildDirNames) {
            const QString dirPath = m_projectRoot + QLatin1Char('/') + dirName;
            if (QDir(dirPath).exists()
                && QFileInfo::exists(dirPath + QStringLiteral("/CMakeCache.txt"))) {
                BuildConfig cfg;
                cfg.buildDir = dirPath;

                // Parse CMakeCache.txt for build type
                QFile cache(dirPath + QStringLiteral("/CMakeCache.txt"));
                if (cache.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    while (!cache.atEnd()) {
                        const QString line = QString::fromUtf8(cache.readLine()).trimmed();
                        if (line.startsWith(QLatin1String("CMAKE_BUILD_TYPE:"))) {
                            cfg.buildType = line.section(QLatin1Char('='), 1);
                        } else if (line.startsWith(QLatin1String("CMAKE_MAKE_PROGRAM:"))) {
                            const QString makeProg = line.section(QLatin1Char('='), 1);
                            if (makeProg.contains(QLatin1String("ninja"), Qt::CaseInsensitive))
                                cfg.generator = QStringLiteral("Ninja");
                        }
                    }
                }

                cfg.name = cfg.buildType.isEmpty()
                    ? dirName
                    : QStringLiteral("%1 (%2)").arg(dirName, cfg.buildType);
                m_configs.append(cfg);
            }
        }
    }

    // If still nothing, create default Debug and Release configurations
    if (m_configs.isEmpty() && hasCMakeProject()) {
        // Determine generator
        QString generator;
        if (m_toolchainMgr) {
            const auto cmake = m_toolchainMgr->findBuildSystem(QStringLiteral("cmake"));
            const auto ninja = m_toolchainMgr->findBuildSystem(QStringLiteral("ninja"));
            if (ninja.isValid())
                generator = QStringLiteral("Ninja");
        }

        for (const auto &type : {QStringLiteral("Debug"), QStringLiteral("Release")}) {
            BuildConfig cfg;
            cfg.name = type;
            cfg.buildType = type;
            cfg.buildDir = m_projectRoot + QStringLiteral("/build-")
                           + type.toLower();
            cfg.generator = generator;
            cfg.extraCmakeArgs = {
                QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")
            };
            m_configs.append(cfg);
        }
    }

    // Restore last-used config from QSettings (per project)
    m_activeConfig = 0;
    bool restored = false;
    {
        QSettings settings;
        const QString key = QStringLiteral("cmake/activePreset/")
            + QString::fromUtf8(m_projectRoot.toUtf8().toBase64());
        const QString savedPreset = settings.value(key).toString();
        if (!savedPreset.isEmpty()) {
            for (int i = 0; i < m_configs.size(); ++i) {
                if (m_configs[i].cmakePreset == savedPreset
                    || m_configs[i].name == savedPreset) {
                    m_activeConfig = i;
                    restored = true;
                    break;
                }
            }
        }
    }

    // If no saved preference, auto-select the config with compile_commands.json
    if (!restored) {
        for (int i = 0; i < m_configs.size(); ++i) {
            const QString ccPath = m_configs[i].buildDir
                + QStringLiteral("/compile_commands.json");
            if (QFileInfo::exists(ccPath)) {
                m_activeConfig = i;
                break;
            }
        }
        // Fallback: pick first config with an existing build dir
        if (m_activeConfig == 0 && m_configs.size() > 1) {
            for (int i = 0; i < m_configs.size(); ++i) {
                if (QDir(m_configs[i].buildDir).exists()) {
                    m_activeConfig = i;
                    break;
                }
            }
        }
    }

    emit configsChanged();
}

// ── Configure ─────────────────────────────────────────────────────────────────

void CMakeIntegration::configure()
{
    const BuildConfig cfg = activeBuildConfig();
    if (!cfg.isValid()) {
        emit configureFinished(false, tr("No active build configuration"));
        return;
    }

    const auto cmake = m_toolchainMgr
        ? m_toolchainMgr->findBuildSystem(QStringLiteral("cmake"))
        : BuildSystemInfo{};

    const QString cmakePath = cmake.isValid()
        ? cmake.path
        : QStandardPaths::findExecutable(QStringLiteral("cmake"));

    if (cmakePath.isEmpty()) {
        emit configureFinished(false, tr("CMake not found"));
        return;
    }

    // Log which config/preset is being used
    if (!cfg.cmakePreset.isEmpty())
        emit buildOutput(tr("Configuring with preset: %1").arg(cfg.cmakePreset), false);
    else
        emit buildOutput(tr("Configuring: %1").arg(cfg.name), false);

    // Ensure build directory exists
    QDir().mkpath(cfg.buildDir);

    startProcess(cmakePath, cmakeConfigureArgs(), m_projectRoot, Operation::Configure);
}

QStringList CMakeIntegration::cmakeConfigureArgs() const
{
    const BuildConfig cfg = activeBuildConfig();
    QStringList args;

    if (!cfg.cmakePreset.isEmpty()) {
        // Use preset, but ensure compile_commands.json is generated for clangd
        args << QStringLiteral("--preset") << cfg.cmakePreset
             << QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON");
    } else {
        // Manual configuration
        args << QStringLiteral("-S") << QStringLiteral(".")
             << QStringLiteral("-B") << cfg.buildDir;

        if (!cfg.generator.isEmpty())
            args << QStringLiteral("-G") << cfg.generator;

        if (!cfg.buildType.isEmpty())
            args << QStringLiteral("-DCMAKE_BUILD_TYPE=") + cfg.buildType;

        // Always export compile_commands.json for clangd
        args << QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON");

        // If we have a toolchain, set the compiler
        if (m_toolchainMgr && !cfg.toolchainId.isEmpty()) {
            const Toolchain tc = m_toolchainMgr->defaultToolchain();
            if (tc.isValid()) {
                if (tc.cCompiler.isValid())
                    args << QStringLiteral("-DCMAKE_C_COMPILER=") + tc.cCompiler.path;
                if (tc.cxxCompiler.isValid())
                    args << QStringLiteral("-DCMAKE_CXX_COMPILER=") + tc.cxxCompiler.path;
            }
        }
    }

    args << cfg.extraCmakeArgs;
    return args;
}

// ── Build ─────────────────────────────────────────────────────────────────────

void CMakeIntegration::build(const QString &target)
{
    const BuildConfig cfg = activeBuildConfig();
    if (!cfg.isValid()) {
        emit buildFinished(false, -1);
        return;
    }

    const auto cmake = m_toolchainMgr
        ? m_toolchainMgr->findBuildSystem(QStringLiteral("cmake"))
        : BuildSystemInfo{};

    const QString cmakePath = cmake.isValid()
        ? cmake.path
        : QStandardPaths::findExecutable(QStringLiteral("cmake"));

    if (cmakePath.isEmpty()) {
        emit buildOutput(tr("Error: CMake not found"), true);
        emit buildFinished(false, -1);
        return;
    }

    // Guard: check that build directory has valid build system files
    const QDir buildDir(cfg.buildDir);
    const bool hasNinja = buildDir.exists(QStringLiteral("build.ninja"));
    const bool hasMake = buildDir.exists(QStringLiteral("Makefile"));
    const bool hasVcx = !buildDir.entryList({QStringLiteral("*.vcxproj")}).isEmpty();
    if (!hasNinja && !hasMake && !hasVcx) {
        emit buildOutput(tr("Error: No build system files in %1. Run Configure first.").arg(cfg.buildDir), true);
        emit buildFinished(false, -1);
        return;
    }

    // Warn if the build system doesn't match the config's generator
    if (!cfg.generator.isEmpty()) {
        const bool generatorIsNinja = cfg.generator.contains(QLatin1String("Ninja"), Qt::CaseInsensitive);
        if (generatorIsNinja && !hasNinja && hasVcx) {
            emit buildOutput(tr("Warning: Config expects %1 but build dir has MSVC project files. "
                               "Run Configure to regenerate.").arg(cfg.generator), true);
            emit buildFinished(false, -1);
            return;
        }
    }

    m_buildTarget = target;
    startProcess(cmakePath, cmakeBuildArgs(target), m_projectRoot, Operation::Build);
}

QStringList CMakeIntegration::cmakeBuildArgs(const QString &target) const
{
    const BuildConfig cfg = activeBuildConfig();
    QStringList args;
    args << QStringLiteral("--build") << cfg.buildDir;

    if (!target.isEmpty())
        args << QStringLiteral("--target") << target;

    // Parallel build
    args << QStringLiteral("--parallel");

    return args;
}

// ── Clean ─────────────────────────────────────────────────────────────────────

void CMakeIntegration::clean()
{
    const BuildConfig cfg = activeBuildConfig();
    if (!cfg.isValid()) {
        emit cleanFinished(false);
        return;
    }

    const QString cmakePath = m_toolchainMgr
        ? m_toolchainMgr->findBuildSystem(QStringLiteral("cmake")).path
        : QStandardPaths::findExecutable(QStringLiteral("cmake"));

    if (cmakePath.isEmpty()) {
        emit cleanFinished(false);
        return;
    }

    startProcess(cmakePath, {QStringLiteral("--build"), cfg.buildDir,
                             QStringLiteral("--target"), QStringLiteral("clean")},
                 m_projectRoot, Operation::Clean);
}

// ── Target discovery ──────────────────────────────────────────────────────────

QStringList CMakeIntegration::discoverTargets() const
{
    const BuildConfig cfg = activeBuildConfig();
    if (!cfg.isValid()) return {};

    QStringList targets;

    // Scan build directory for executables
    QDir buildDir(cfg.buildDir);
    if (!buildDir.exists()) return targets;

    // Look in common output locations
    const QStringList searchDirs = {
        cfg.buildDir,
        cfg.buildDir + QStringLiteral("/src"),
        cfg.buildDir + QStringLiteral("/bin"),
    };

    for (const auto &dir : searchDirs) {
        QDir d(dir);
        if (!d.exists()) continue;

#ifdef Q_OS_WIN
        const QStringList exeFiles = d.entryList({QStringLiteral("*.exe")},
                                                  QDir::Files);
        for (const auto &f : exeFiles)
            targets.append(d.absoluteFilePath(f));
#else
        const QFileInfoList files = d.entryInfoList(QDir::Files | QDir::Executable);
        for (const auto &fi : files) {
            // Skip scripts and non-binary files
            if (fi.suffix().isEmpty() || fi.isExecutable())
                targets.append(fi.absoluteFilePath());
        }
#endif
    }

    return targets;
}

QString CMakeIntegration::compileCommandsPath() const
{
    const BuildConfig cfg = activeBuildConfig();
    if (!cfg.isValid()) return {};

    const QString path = cfg.buildDir + QStringLiteral("/compile_commands.json");
    if (QFileInfo::exists(path))
        return path;
    return {};
}

QString CMakeIntegration::buildDirectory() const
{
    const BuildConfig cfg = activeBuildConfig();
    return cfg.buildDir;
}

void CMakeIntegration::cancelBuild()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(2000))
            m_process->kill();
    }
}

// ── Process management ────────────────────────────────────────────────────────

void CMakeIntegration::startProcess(const QString &program, const QStringList &args,
                                    const QString &workDir, Operation op)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        emit buildOutput(tr("A build is already running."), true);
        return;
    }

    m_currentOp = op;

    if (!m_process) {
        m_process = new QProcess(this);
        connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &CMakeIntegration::onProcessFinished);
        connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
            const QByteArray data = m_process->readAllStandardOutput();
            const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
            for (const auto &line : lines) {
                if (!line.trimmed().isEmpty())
                    emit buildOutput(line, false);
            }
        });
        connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
            const QByteArray data = m_process->readAllStandardError();
            const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
            for (const auto &line : lines) {
                if (!line.trimmed().isEmpty())
                    emit buildOutput(line, true);
            }
        });
    }

    emit buildOutput(QStringLiteral("> %1 %2").arg(program, args.join(QLatin1Char(' '))), false);

    m_process->setWorkingDirectory(workDir);
    m_process->start(program, args);
}

void CMakeIntegration::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    const bool success = (status == QProcess::NormalExit && exitCode == 0);

    switch (m_currentOp) {
    case Operation::Configure:
        emit buildOutput(success ? tr("Configure succeeded")
                                 : tr("Configure failed (exit %1)").arg(exitCode),
                         !success);
        emit configureFinished(success, success ? QString() : tr("Configure failed (exit %1)").arg(exitCode));
        break;
    case Operation::Build:
        emit buildOutput(success ? tr("Build succeeded")
                                 : tr("Build failed (exit %1)").arg(exitCode),
                         !success);
        emit buildFinished(success, exitCode);
        break;
    case Operation::Clean:
        emit buildOutput(success ? tr("Clean succeeded")
                                 : tr("Clean failed (exit %1)").arg(exitCode),
                         !success);
        emit cleanFinished(success);
        break;
    case Operation::None:
        break;
    }

    m_currentOp = Operation::None;
}
