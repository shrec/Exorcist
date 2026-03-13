#pragma once

#include <QString>
#include <QStringList>

// ── IEnvironment ─────────────────────────────────────────────────────────────
//
// Abstract interface for environment variable access and platform detection.
// Keeps platform-specific code behind a testable boundary.

class IEnvironment
{
public:
    virtual ~IEnvironment() = default;

    virtual QString variable(const QString &name) const = 0;
    virtual bool hasVariable(const QString &name) const = 0;
    virtual void setVariable(const QString &name, const QString &value) = 0;
    virtual QStringList variableNames() const = 0;

    virtual QString platform() const = 0;     // "windows", "linux", "macos"
    virtual QString homeDirectory() const = 0;
    virtual QString tempDirectory() const = 0;
};
