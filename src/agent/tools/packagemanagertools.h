#pragma once

#include "../itool.h"

class TerminalSessionManager;

// ── PackageJsonInfoTool ───────────────────────────────────────────────────────
// Reads and reports package.json contents. Context: {"web", "node"}.

class PackageJsonInfoTool : public ITool
{
public:
    PackageJsonInfoTool() = default;

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ── NpmRunTool ────────────────────────────────────────────────────────────────
// Runs an npm/yarn/pnpm script from package.json. Context: {"web", "node"}.

class NpmRunTool : public ITool
{
public:
    explicit NpmRunTool(TerminalSessionManager *mgr) : m_mgr(mgr) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static QString detectPackageManager();

    TerminalSessionManager *m_mgr;
};

// ── InstallNodePackagesTool ───────────────────────────────────────────────────
// Installs Node.js packages. Context: {"web", "node"}.

class InstallNodePackagesTool : public ITool
{
public:
    explicit InstallNodePackagesTool(TerminalSessionManager *mgr) : m_mgr(mgr) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static QString detectPackageManager();

    TerminalSessionManager *m_mgr;
};
