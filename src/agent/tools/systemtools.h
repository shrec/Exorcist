#pragma once

#include "../itool.h"

// ── ProcessManagementTool ────────────────────────────────────────────────────
// List running processes, check ports, kill processes.

class ProcessManagementTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static ToolExecResult listProcesses(const QString &filter);
    static ToolExecResult checkPort(int port);
    static ToolExecResult findByPort(int port);
    static ToolExecResult killProcess(int pid);
};

// ── NetworkTool ──────────────────────────────────────────────────────────────
// Network diagnostic utilities: ping, DNS lookup, port check.

class NetworkTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static ToolExecResult ping(const QJsonObject &args);
    static ToolExecResult dnsLookup(const QJsonObject &args);
    static ToolExecResult portScan(const QJsonObject &args);
    static ToolExecResult localIp();
};
