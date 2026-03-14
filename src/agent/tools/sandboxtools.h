#pragma once

#include "../itool.h"

class IFileSystem;

// ── JsonParseFormatTool ──────────────────────────────────────────────────────
// JSON parse, format, validate. Provides ex.json.decode() / ex.json.encode()
// functionality for the AI agent. Useful for data transformation.

class JsonParseFormatTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ── CurrentTimeTool ──────────────────────────────────────────────────────────
// Returns current wall-clock time. Provides ex.time.now() for the agent.
// Useful for timestamps, logging, scheduling awareness.

class CurrentTimeTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ── RegexTestTool ────────────────────────────────────────────────────────────
// Validate regex patterns and test them against inputs.
// Useful for building and debugging regular expressions.

class RegexTestTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static QString formatMatch(const QRegularExpressionMatch &m);
};

// ── EnvironmentVariablesTool ─────────────────────────────────────────────────
// Read system environment variables. Useful for understanding build configs,
// PATH, compiler locations, SDK paths, etc.

class EnvironmentVariablesTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ── FileHashTool ─────────────────────────────────────────────────────────────
// Compute SHA256 or MD5 hash of a file for verification.

class FileHashTool : public ITool
{
public:
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString resolvePath(const QString &path) const;

    QString m_workspaceRoot;
};

// ── WorkspaceConfigTool ──────────────────────────────────────────────────────
// Read workspace configuration files like .exorcist/, CMakePresets.json,
// .editorconfig, .clang-format, etc.

class WorkspaceConfigTool : public ITool
{
public:
    explicit WorkspaceConfigTool(IFileSystem *fs) : m_fs(fs) {}

    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ToolExecResult scanConfigs() const;
    ToolExecResult readConfig(const QString &file) const;

    IFileSystem *m_fs;
    QString      m_workspaceRoot;
};
