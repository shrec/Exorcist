#include "dashboardtools.h"

#include <QJsonArray>
#include <QUuid>

// ── CreateDashboardMissionTool ───────────────────────────────────────────────

CreateDashboardMissionTool::CreateDashboardMissionTool(AgentUIBus *bus)
    : m_bus(bus)
{
}

ToolSpec CreateDashboardMissionTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("create_dashboard_mission");
    s.description = QStringLiteral(
        "Create a new agent mission on the live dashboard. "
        "This resets the dashboard and starts fresh. Returns the mission ID.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("title"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Short title for the mission.")}
            }},
            {QStringLiteral("description"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Longer description of what this mission will accomplish.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("title")}}
    };
    return s;
}

ToolExecResult CreateDashboardMissionTool::invoke(const QJsonObject &args)
{
    const QString title = args[QLatin1String("title")].toString();
    if (title.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: title")};

    const QString desc = args[QLatin1String("description")].toString();
    const QString missionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    m_bus->clearMission();
    m_bus->post(AgentUIEvent::missionCreated(missionId, title, desc));
    m_bus->post(AgentUIEvent::missionUpdated(missionId, AgentMissionStatus::Running));

    return {true, {}, QStringLiteral("Mission created: %1 (id: %2)").arg(title, missionId), {}};
}

// ── UpdateDashboardStepTool ──────────────────────────────────────────────────

UpdateDashboardStepTool::UpdateDashboardStepTool(AgentUIBus *bus)
    : m_bus(bus)
{
}

ToolSpec UpdateDashboardStepTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("update_dashboard_step");
    s.description = QStringLiteral(
        "Add a new step or update an existing step on the agent dashboard. "
        "Use to show task progress. Status: pending, running, completed, failed, skipped.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("step_id"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Unique identifier for this step.")}
            }},
            {QStringLiteral("label"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Human-readable label for the step.")}
            }},
            {QStringLiteral("status"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("pending"), QStringLiteral("running"),
                    QStringLiteral("completed"), QStringLiteral("failed"),
                    QStringLiteral("skipped")}},
                {QStringLiteral("description"),
                 QStringLiteral("Current status of the step.")}
            }},
            {QStringLiteral("detail"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Optional detail text shown below the label.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("step_id"), QStringLiteral("label"), QStringLiteral("status")}}
    };
    return s;
}

ToolExecResult UpdateDashboardStepTool::invoke(const QJsonObject &args)
{
    const QString stepId = args[QLatin1String("step_id")].toString();
    const QString label  = args[QLatin1String("label")].toString();
    const QString status = args[QLatin1String("status")].toString();
    const QString detail = args[QLatin1String("detail")].toString();

    if (stepId.isEmpty() || label.isEmpty() || status.isEmpty())
        return {false, {}, {},
            QStringLiteral("Missing required parameters: step_id, label, status")};

    const QString missionId = m_bus->currentMissionId();
    if (missionId.isEmpty())
        return {false, {}, {},
            QStringLiteral("No active mission. Call create_dashboard_mission first.")};

    // Map status string to enum
    static const QHash<QString, AgentStepStatus> statusMap = {
        {QStringLiteral("pending"),   AgentStepStatus::Pending},
        {QStringLiteral("running"),   AgentStepStatus::Running},
        {QStringLiteral("completed"), AgentStepStatus::Completed},
        {QStringLiteral("failed"),    AgentStepStatus::Failed},
        {QStringLiteral("skipped"),   AgentStepStatus::Skipped},
    };

    const AgentStepStatus stepStatus = statusMap.value(status, AgentStepStatus::Pending);

    // Check if step already exists in history (update vs create)
    const auto history = m_bus->eventsForMission(missionId);
    bool exists = false;
    for (const auto &e : history) {
        if (e.type == AgentUIEventType::StepAdded &&
            e.payload.value(QStringLiteral("stepId")).toString() == stepId) {
            exists = true;
            break;
        }
    }

    if (!exists)
        m_bus->post(AgentUIEvent::stepAdded(missionId, stepId, label));

    m_bus->post(AgentUIEvent::stepUpdated(missionId, stepId, stepStatus, detail));

    return {true, {}, QStringLiteral("Step '%1' → %2").arg(stepId, status), {}};
}

// ── UpdateDashboardMetricTool ────────────────────────────────────────────────

UpdateDashboardMetricTool::UpdateDashboardMetricTool(AgentUIBus *bus)
    : m_bus(bus)
{
}

ToolSpec UpdateDashboardMetricTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("update_dashboard_metric");
    s.description = QStringLiteral(
        "Update a metric on the agent dashboard. Displays as a card with "
        "label, value, and optional unit. Examples: tokens used, files modified, time elapsed.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("key"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Metric name (e.g., 'tokens', 'files', 'time').")}
            }},
            {QStringLiteral("value"), QJsonObject{
                {QStringLiteral("description"),
                 QStringLiteral("Metric value (number or string).")}
            }},
            {QStringLiteral("unit"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Optional unit label (e.g., 'ms', 'KB', 'files').")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("key"), QStringLiteral("value")}}
    };
    return s;
}

ToolExecResult UpdateDashboardMetricTool::invoke(const QJsonObject &args)
{
    const QString key = args[QLatin1String("key")].toString();
    if (key.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: key")};

    const QString missionId = m_bus->currentMissionId();
    if (missionId.isEmpty())
        return {false, {}, {},
            QStringLiteral("No active mission. Call create_dashboard_mission first.")};

    const QJsonValue value = args[QLatin1String("value")];
    const QString unit = args[QLatin1String("unit")].toString();

    m_bus->post(AgentUIEvent::metricUpdated(missionId, key, value, unit));

    return {true, {}, QStringLiteral("Metric '%1' updated").arg(key), {}};
}

// ── AddDashboardLogTool ──────────────────────────────────────────────────────

AddDashboardLogTool::AddDashboardLogTool(AgentUIBus *bus)
    : m_bus(bus)
{
}

ToolSpec AddDashboardLogTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("add_dashboard_log");
    s.description = QStringLiteral(
        "Add a log entry to the agent dashboard activity log. "
        "Level: debug, info, warning, error.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("level"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("debug"), QStringLiteral("info"),
                    QStringLiteral("warning"), QStringLiteral("error")}},
                {QStringLiteral("description"),
                 QStringLiteral("Log level.")}
            }},
            {QStringLiteral("message"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Log message text.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("message")}}
    };
    return s;
}

ToolExecResult AddDashboardLogTool::invoke(const QJsonObject &args)
{
    const QString message = args[QLatin1String("message")].toString();
    if (message.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: message")};

    const QString missionId = m_bus->currentMissionId();
    if (missionId.isEmpty())
        return {false, {}, {},
            QStringLiteral("No active mission. Call create_dashboard_mission first.")};

    static const QHash<QString, AgentLogLevel> levelMap = {
        {QStringLiteral("debug"),   AgentLogLevel::Debug},
        {QStringLiteral("info"),    AgentLogLevel::Info},
        {QStringLiteral("warning"), AgentLogLevel::Warning},
        {QStringLiteral("error"),   AgentLogLevel::Error},
    };

    const QString levelStr = args[QLatin1String("level")].toString(QStringLiteral("info"));
    const AgentLogLevel level = levelMap.value(levelStr, AgentLogLevel::Info);

    m_bus->post(AgentUIEvent::logAdded(missionId, level, message));

    return {true, {}, QStringLiteral("Log added: [%1] %2").arg(levelStr, message.left(80)), {}};
}

// ── AddDashboardArtifactTool ─────────────────────────────────────────────────

AddDashboardArtifactTool::AddDashboardArtifactTool(AgentUIBus *bus)
    : m_bus(bus)
{
}

ToolSpec AddDashboardArtifactTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("add_dashboard_artifact");
    s.description = QStringLiteral(
        "Register an output artifact on the agent dashboard. "
        "Artifacts appear as clickable chips (files, diffs, etc.).");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("artifact_type"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Type of artifact (e.g., 'file', 'diff', 'test', 'build').")}
            }},
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path or identifier for the artifact.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("artifact_type"), QStringLiteral("path")}}
    };
    return s;
}

ToolExecResult AddDashboardArtifactTool::invoke(const QJsonObject &args)
{
    const QString type = args[QLatin1String("artifact_type")].toString();
    const QString path = args[QLatin1String("path")].toString();
    if (type.isEmpty() || path.isEmpty())
        return {false, {}, {},
            QStringLiteral("Missing required parameters: artifact_type, path")};

    const QString missionId = m_bus->currentMissionId();
    if (missionId.isEmpty())
        return {false, {}, {},
            QStringLiteral("No active mission. Call create_dashboard_mission first.")};

    m_bus->post(AgentUIEvent::artifactAdded(missionId, type, path));

    return {true, {}, QStringLiteral("Artifact added: [%1] %2").arg(type, path), {}};
}

// ── CompleteDashboardMissionTool ─────────────────────────────────────────────

CompleteDashboardMissionTool::CompleteDashboardMissionTool(AgentUIBus *bus)
    : m_bus(bus)
{
}

ToolSpec CompleteDashboardMissionTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("complete_dashboard_mission");
    s.description = QStringLiteral(
        "Mark the current mission as completed (success or failure) "
        "on the agent dashboard.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("success"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Whether the mission succeeded.")}
            }},
            {QStringLiteral("summary"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Brief summary of what was accomplished.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("success")}}
    };
    return s;
}

ToolExecResult CompleteDashboardMissionTool::invoke(const QJsonObject &args)
{
    const QString missionId = m_bus->currentMissionId();
    if (missionId.isEmpty())
        return {false, {}, {},
            QStringLiteral("No active mission to complete.")};

    const bool success = args[QLatin1String("success")].toBool();
    const QString summary = args[QLatin1String("summary")].toString();

    m_bus->post(AgentUIEvent::missionCompleted(missionId, success, summary));

    return {true, {},
        QStringLiteral("Mission %1: %2")
            .arg(success ? QStringLiteral("completed") : QStringLiteral("failed"),
                 summary.isEmpty() ? QStringLiteral("(no summary)") : summary),
        {}};
}
