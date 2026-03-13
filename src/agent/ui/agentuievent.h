#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QMetaType>
#include <QString>

// ── Agent UI Event Protocol ──────────────────────────────────────────────────
//
// Structured events emitted by agents to control the live dashboard panel.
// Agents produce events; the UI Event Bus dispatches them to renderers.
//
// Architecture:
//   Agent controller → emits AgentUIEvent → AgentUIBus → IAgentUIRenderer(s)
//
// Variant A (default): Agent sends structured JSON data, the dashboard
//   renderer draws it. Agent controls CONTENT, not the rendering framework.

enum class AgentUIEventType
{
    // ── Mission lifecycle ─────────────────────────────────────────────────
    MissionCreated,     // New mission/task started
    MissionUpdated,     // Mission status changed (running, paused, completed, failed)
    MissionCompleted,   // Mission finished (success or failure)

    // ── Step tracking ─────────────────────────────────────────────────────
    StepAdded,          // New step added to mission
    StepUpdated,        // Step status/progress changed

    // ── Metrics ───────────────────────────────────────────────────────────
    MetricUpdated,      // A metric value changed (tokens, files, time, etc.)

    // ── Logs ──────────────────────────────────────────────────────────────
    LogAdded,           // New log entry

    // ── Panels ────────────────────────────────────────────────────────────
    PanelCreated,       // A sub-panel was created (e.g., diff view, file tree)
    PanelUpdated,       // Panel content changed
    PanelRemoved,       // Panel removed

    // ── Artifacts ─────────────────────────────────────────────────────────
    ArtifactAdded,      // File created/modified, diff generated, etc.

    // ── Custom ────────────────────────────────────────────────────────────
    CustomEvent,        // Freeform event with arbitrary payload
};

// ── Log levels for LogAdded events ───────────────────────────────────────────

enum class AgentLogLevel
{
    Debug,
    Info,
    Warning,
    Error,
};

// ── Step status for StepAdded/StepUpdated ────────────────────────────────────

enum class AgentStepStatus
{
    Pending,
    Running,
    Completed,
    Failed,
    Skipped,
};

// ── Mission status for MissionCreated/Updated/Completed ──────────────────────

enum class AgentMissionStatus
{
    Created,
    Running,
    Paused,
    Completed,
    Failed,
    Cancelled,
};

// ── AgentUIEvent — the wire format ───────────────────────────────────────────
//
// Every event has a type and a JSON payload.  The payload schema depends on
// the event type.  The missionId ties events to a specific mission context.

struct AgentUIEvent
{
    AgentUIEventType type;
    QString          missionId;     // groups events by mission
    QJsonObject      payload;       // type-specific structured data
    qint64           timestamp = 0; // ms since epoch (filled by bus)

    // ── Convenience constructors ─────────────────────────────────────────

    static AgentUIEvent missionCreated(const QString &missionId,
                                       const QString &title,
                                       const QString &description = {})
    {
        return {AgentUIEventType::MissionCreated, missionId,
                QJsonObject{{QStringLiteral("title"), title},
                            {QStringLiteral("description"), description},
                            {QStringLiteral("status"), QStringLiteral("created")}}};
    }

    static AgentUIEvent missionUpdated(const QString &missionId,
                                       AgentMissionStatus status)
    {
        static const char *names[] = {"created","running","paused","completed","failed","cancelled"};
        return {AgentUIEventType::MissionUpdated, missionId,
                QJsonObject{{QStringLiteral("status"),
                             QString::fromLatin1(names[static_cast<int>(status)])}}};
    }

    static AgentUIEvent missionCompleted(const QString &missionId,
                                          bool success,
                                          const QString &summary = {})
    {
        return {AgentUIEventType::MissionCompleted, missionId,
                QJsonObject{{QStringLiteral("success"), success},
                            {QStringLiteral("summary"), summary}}};
    }

    static AgentUIEvent stepAdded(const QString &missionId,
                                   const QString &stepId,
                                   const QString &label,
                                   int order = -1)
    {
        return {AgentUIEventType::StepAdded, missionId,
                QJsonObject{{QStringLiteral("stepId"), stepId},
                            {QStringLiteral("label"), label},
                            {QStringLiteral("order"), order},
                            {QStringLiteral("status"), QStringLiteral("pending")}}};
    }

    static AgentUIEvent stepUpdated(const QString &missionId,
                                     const QString &stepId,
                                     AgentStepStatus status,
                                     const QString &detail = {})
    {
        static const char *names[] = {"pending","running","completed","failed","skipped"};
        return {AgentUIEventType::StepUpdated, missionId,
                QJsonObject{{QStringLiteral("stepId"), stepId},
                            {QStringLiteral("status"),
                             QString::fromLatin1(names[static_cast<int>(status)])},
                            {QStringLiteral("detail"), detail}}};
    }

    static AgentUIEvent metricUpdated(const QString &missionId,
                                       const QString &key,
                                       const QJsonValue &value,
                                       const QString &unit = {})
    {
        return {AgentUIEventType::MetricUpdated, missionId,
                QJsonObject{{QStringLiteral("key"), key},
                            {QStringLiteral("value"), value},
                            {QStringLiteral("unit"), unit}}};
    }

    static AgentUIEvent logAdded(const QString &missionId,
                                  AgentLogLevel level,
                                  const QString &message)
    {
        static const char *names[] = {"debug","info","warning","error"};
        return {AgentUIEventType::LogAdded, missionId,
                QJsonObject{{QStringLiteral("level"),
                             QString::fromLatin1(names[static_cast<int>(level)])},
                            {QStringLiteral("message"), message}}};
    }

    static AgentUIEvent artifactAdded(const QString &missionId,
                                       const QString &artifactType,
                                       const QString &path,
                                       const QJsonObject &meta = {})
    {
        QJsonObject p{{QStringLiteral("type"), artifactType},
                      {QStringLiteral("path"), path}};
        if (!meta.isEmpty())
            p[QStringLiteral("meta")] = meta;
        return {AgentUIEventType::ArtifactAdded, missionId, p};
    }

    static AgentUIEvent panelCreated(const QString &missionId,
                                      const QString &panelId,
                                      const QString &title,
                                      const QString &panelType = QStringLiteral("info"))
    {
        return {AgentUIEventType::PanelCreated, missionId,
                QJsonObject{{QStringLiteral("panelId"), panelId},
                            {QStringLiteral("title"), title},
                            {QStringLiteral("panelType"), panelType}}};
    }

    static AgentUIEvent panelUpdated(const QString &missionId,
                                      const QString &panelId,
                                      const QJsonObject &content)
    {
        QJsonObject p{{QStringLiteral("panelId"), panelId},
                      {QStringLiteral("content"), content}};
        return {AgentUIEventType::PanelUpdated, missionId, p};
    }

    static AgentUIEvent panelRemoved(const QString &missionId,
                                      const QString &panelId)
    {
        return {AgentUIEventType::PanelRemoved, missionId,
                QJsonObject{{QStringLiteral("panelId"), panelId}}};
    }

    static AgentUIEvent custom(const QString &missionId,
                                const QString &eventName,
                                const QJsonObject &data)
    {
        QJsonObject p{{QStringLiteral("eventName"), eventName}};
        for (auto it = data.begin(); it != data.end(); ++it)
            p[it.key()] = it.value();
        return {AgentUIEventType::CustomEvent, missionId, p};
    }
};

Q_DECLARE_METATYPE(AgentUIEvent)
