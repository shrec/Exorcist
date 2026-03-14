#pragma once

#include "../itool.h"

class TerminalSessionManager;

// ── PythonEnvTool ─────────────────────────────────────────────────────────────
// Reports Python interpreter version, pip packages, and virtual environment
// status. Context: {"python"} — only active in Python workspaces.

class PythonEnvTool : public ITool
{
public:
    explicit PythonEnvTool(TerminalSessionManager *mgr) : m_mgr(mgr) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    TerminalSessionManager *m_mgr;
};

// ── InstallPythonPackagesTool ─────────────────────────────────────────────────
// Installs Python packages via pip. Context: {"python"}.

class InstallPythonPackagesTool : public ITool
{
public:
    explicit InstallPythonPackagesTool(TerminalSessionManager *mgr) : m_mgr(mgr) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    TerminalSessionManager *m_mgr;
};

// ── RunPythonTool ─────────────────────────────────────────────────────────────
// Runs a Python code snippet directly. Context: {"python"}.

class RunPythonTool : public ITool
{
public:
    explicit RunPythonTool(TerminalSessionManager *mgr) : m_mgr(mgr) {}

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    TerminalSessionManager *m_mgr;
};
