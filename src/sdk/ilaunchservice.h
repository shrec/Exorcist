#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>

// ── Launch Service ───────────────────────────────────────────────────────────
//
// Stable SDK interface for launching executables with or without a debugger.
// Registered in ServiceRegistry as "launchService".

/// Configuration for a single launch target.
struct LaunchConfig
{
    QString name;
    QString executable;
    QStringList args;
    QString workingDir;
    QJsonObject env;
    QString debuggerPath;

    bool isValid() const { return !executable.isEmpty(); }
};

class ILaunchService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// Start debugging the given target (build first if configured).
    virtual void startDebugging(const LaunchConfig &config) = 0;

    /// Run the target without a debugger (build first if configured).
    virtual void startWithoutDebugging(const LaunchConfig &config) = 0;

    /// Stop the current debug/run session.
    virtual void stopSession() = 0;

    /// Whether to build-before-run.
    virtual bool buildBeforeRun() const = 0;
    virtual void setBuildBeforeRun(bool enabled) = 0;

signals:
    void processStarted(const QString &executable);
    void processFinished(int exitCode);
    void launchError(const QString &message);
};
