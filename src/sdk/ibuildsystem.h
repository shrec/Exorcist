#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// ── Build System Service ─────────────────────────────────────────────────────
//
// Stable SDK interface for build system operations (configure, build, clean).
// Plugins and agent tools should use this instead of directly accessing
// CMakeIntegration or any concrete build system implementation.
//
// Registered in ServiceRegistry as "buildSystem".

class IBuildSystem : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Set the workspace root — tells the build system where the project lives.
    /// Must be called when a workspace is opened or changed.
    virtual void setProjectRoot(const QString &root) = 0;

    /// Whether a build system project is detected in the workspace.
    virtual bool hasProject() const = 0;

    /// Configure the build system (e.g., cmake -S . -B <buildDir>).
    virtual void configure() = 0;

    /// Build the project, optionally targeting a specific target.
    virtual void build(const QString &target = {}) = 0;

    /// Clean build artifacts.
    virtual void clean() = 0;

    /// Cancel a running build.
    virtual void cancelBuild() = 0;

    /// Whether a build is currently in progress.
    virtual bool isBuilding() const = 0;

    /// Discover available build targets (executables).
    virtual QStringList targets() const = 0;

    /// Get the build output directory for the active configuration.
    virtual QString buildDirectory() const = 0;

    /// Get the compile_commands.json path (for LSP / clangd).
    virtual QString compileCommandsPath() const = 0;

signals:
    /// Emitted for each line of build output.
    void buildOutput(const QString &line, bool isError);

    /// Emitted when configure completes.
    void configureFinished(bool success, const QString &error);

    /// Emitted when build completes.
    void buildFinished(bool success, int exitCode);

    /// Emitted when clean completes.
    void cleanFinished(bool success);
};
