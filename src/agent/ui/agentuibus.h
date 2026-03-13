#pragma once

#include <QObject>
#include <QList>
#include <QDateTime>

#include "agentuievent.h"

class IAgentUIRenderer;

// ── AgentUIBus — central dispatcher for Agent UI events ──────────────────────
//
// Agents emit events here. The bus timestamps them, stores them for the
// current mission, and dispatches to all registered IAgentUIRenderer instances.
//
// Access via ServiceRegistry key: "agentUIBus"

class AgentUIBus : public QObject
{
    Q_OBJECT

public:
    explicit AgentUIBus(QObject *parent = nullptr);

    // ── Renderer registration ─────────────────────────────────────────────
    void addRenderer(IAgentUIRenderer *renderer);
    void removeRenderer(IAgentUIRenderer *renderer);

    // ── Event emission (called by agent tools / controller) ───────────────
    void post(const AgentUIEvent &event);

    // ── Mission management ────────────────────────────────────────────────
    QString currentMissionId() const;
    void    clearMission(const QString &missionId = {});

    // ── Event history (for late-joining renderers) ────────────────────────
    QList<AgentUIEvent> eventsForMission(const QString &missionId) const;

signals:
    void eventDispatched(const AgentUIEvent &event);

private:
    static constexpr int kMaxEventsPerMission = 500;
    static constexpr int kMaxMissions = 20;
    QList<IAgentUIRenderer *>          m_renderers;
    QHash<QString, QList<AgentUIEvent>> m_history; // missionId → events
    QList<QString>                     m_missionOrder; // oldest-first
    QString                            m_currentMissionId;
};
