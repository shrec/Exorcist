#include "cmakeintegration.h"
#include "toolchainmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
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
    if (index >= 0 && index < m_configs.size())
        m_activeConfig = index;
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

    // If presets exist, use them
    if (hasPresets()) {
        const QStringList presets = detectPresets();
        for (const auto &preset : presets) {
            BuildConfig cfg;
            cfg.name = preset;
            cfg.cmakePreset = preset;

            // Try to infer build dir from preset file
            QFile f(m_projectRoot + QStringLiteral("/CMakePresets.json"));
            if (f.open(QIODevice::ReadOnly)) {
                const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                const QJsonArray arr = doc.object()
                    .value(QLatin1String("configurePresets")).toArray();
                for (const auto &val : arr) {
                    const QJsonObject obj = val.toObject();
                    if (obj.value(QLatin1String("name")).toString() == preset) {
                        cfg.buildDir = obj.value(QLatin1String("binaryDir")).toString();
                        cfg.generator = obj.value(QLatin1String("generator")).toString();
                        // Resolve ${sourceDir} variable
                        cfg.buildDir.replace(
                            QLatin1String("${sourceDir}"), m_projectRoot);
                        break;
                    }
                }
            }

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

    // Auto-select the config whose build directory already exists
    // (prefer one with compile_commands.json for clangd)
    m_activeConfig = 0;
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
        emit configureFinished(success, success ? QString() : tr("Configure failed (exit %1)").arg(exitCode));
        break;
    case Operation::Build:
        emit buildFinished(success, exitCode);
        break;
    case Operation::Clean:
        emit cleanFinished(success);
        break;
    case Operation::None:
        break;
    }

    m_currentOp = Operation::None;
}
