#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

class ToolchainManager;

// ── Build configuration ───────────────────────────────────────────────────────

struct BuildConfig
{
    QString name;               // e.g. "Debug", "Release", "RelWithDebInfo"
    QString buildDir;           // e.g. "build-debug"
    QString cmakePreset;        // CMake preset name (if using presets)
    QString buildType;          // CMAKE_BUILD_TYPE value
    QString generator;          // "Ninja", "Unix Makefiles", etc.
    QString toolchainId;        // links to Toolchain::id
    QStringList extraCmakeArgs; // additional cmake args

    bool isValid() const { return !name.isEmpty() && !buildDir.isEmpty(); }
};

// ── CMakeIntegration ──────────────────────────────────────────────────────────

/// Manages CMake configure/build/clean lifecycle for a project.
/// Detects CMake presets, generates compile_commands.json for clangd,
/// and discovers build targets for the run/debug pipeline.
class CMakeIntegration : public QObject
{
    Q_OBJECT

public:
    explicit CMakeIntegration(QObject *parent = nullptr);

    /// Set the workspace root (where CMakeLists.txt lives).
    void setProjectRoot(const QString &root);

    /// Set the toolchain manager for compiler discovery.
    void setToolchainManager(ToolchainManager *mgr) { m_toolchainMgr = mgr; }

    /// Check if the project root has a CMakeLists.txt.
    bool hasCMakeProject() const;

    /// Check if CMakePresets.json exists.
    bool hasPresets() const;

    /// Detect available CMake presets from CMakePresets.json.
    QStringList detectPresets() const;

    /// Get or create build configurations.
    QList<BuildConfig> buildConfigs() const { return m_configs; }

    /// Set active build configuration.
    void setActiveBuildConfig(int index);
    int activeBuildConfigIndex() const { return m_activeConfig; }
    BuildConfig activeBuildConfig() const;

    /// Auto-detect build configurations from the project.
    void autoDetectConfigs();

    /// Configure the CMake project (runs cmake -S . -B <buildDir>).
    void configure();

    /// Build the CMake project (runs cmake --build <buildDir>).
    void build(const QString &target = {});

    /// Clean the build directory.
    void clean();

    /// Discover executable targets from the build.
    QStringList discoverTargets() const;

    /// Get the compile_commands.json path for the active config.
    QString compileCommandsPath() const;

    /// Get the build directory for the active config.
    QString buildDirectory() const;

    /// Whether a build is currently running.
    bool isBuilding() const { return m_process && m_process->state() != QProcess::NotRunning; }

    /// Cancel a running build.
    void cancelBuild();

signals:
    /// Emitted for each line of build output.
    void buildOutput(const QString &line, bool isError);

    /// Emitted when configure completes.
    void configureFinished(bool success, const QString &error);

    /// Emitted when build completes.
    void buildFinished(bool success, int exitCode);

    /// Emitted when clean completes.
    void cleanFinished(bool success);

    /// Emitted when configs change.
    void configsChanged();

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    enum class Operation { None, Configure, Build, Clean };

    void startProcess(const QString &program, const QStringList &args,
                      const QString &workDir, Operation op);
    QStringList cmakeConfigureArgs() const;
    QStringList cmakeBuildArgs(const QString &target) const;

    QString m_projectRoot;
    ToolchainManager *m_toolchainMgr = nullptr;
    QList<BuildConfig> m_configs;
    int m_activeConfig = 0;
    QProcess *m_process = nullptr;
    Operation m_currentOp = Operation::None;
    QString m_buildTarget;
};
