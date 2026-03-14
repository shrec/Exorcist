#pragma once

#include "../itool.h"
#include "../ui/agentuibus.h"
#include "../ui/agentuievent.h"

// ── Agent Dashboard Tools ────────────────────────────────────────────────────
//
// ITool implementations that allow agents to control the live dashboard.
// These are registered in ToolRegistry and exposed to the model as callable
// functions. The agent calls these tools to push structured events to the
// AgentUIBus → AgentDashboardPanel rendering pipeline.
//
// Tools:
//   - create_dashboard_mission : Create a new mission (resets dashboard)
//   - update_dashboard_step    : Add or update a step in the mission
//   - update_dashboard_metric  : Set a metric value
//   - add_dashboard_log        : Add a log entry
//   - add_dashboard_artifact   : Register an output artifact
//   - complete_dashboard_mission : Mark mission as done


// ── CreateDashboardMissionTool ───────────────────────────────────────────────

class CreateDashboardMissionTool : public ITool
{
public:
    explicit CreateDashboardMissionTool(AgentUIBus *bus);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentUIBus *m_bus;
};


// ── UpdateDashboardStepTool ──────────────────────────────────────────────────

class UpdateDashboardStepTool : public ITool
{
public:
    explicit UpdateDashboardStepTool(AgentUIBus *bus);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentUIBus *m_bus;
};


// ── UpdateDashboardMetricTool ────────────────────────────────────────────────

class UpdateDashboardMetricTool : public ITool
{
public:
    explicit UpdateDashboardMetricTool(AgentUIBus *bus);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentUIBus *m_bus;
};


// ── AddDashboardLogTool ──────────────────────────────────────────────────────

class AddDashboardLogTool : public ITool
{
public:
    explicit AddDashboardLogTool(AgentUIBus *bus);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentUIBus *m_bus;
};


// ── AddDashboardArtifactTool ─────────────────────────────────────────────────

class AddDashboardArtifactTool : public ITool
{
public:
    explicit AddDashboardArtifactTool(AgentUIBus *bus);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentUIBus *m_bus;
};


// ── CompleteDashboardMissionTool ─────────────────────────────────────────────

class CompleteDashboardMissionTool : public ITool
{
public:
    explicit CompleteDashboardMissionTool(AgentUIBus *bus);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    AgentUIBus *m_bus;
};
