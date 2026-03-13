#include "agentuibus.h"
#include "iagentuirenderer.h"

AgentUIBus::AgentUIBus(QObject *parent)
    : QObject(parent)
{
}

void AgentUIBus::addRenderer(IAgentUIRenderer *renderer)
{
    if (renderer && !m_renderers.contains(renderer))
        m_renderers.append(renderer);
}

void AgentUIBus::removeRenderer(IAgentUIRenderer *renderer)
{
    m_renderers.removeAll(renderer);
}

void AgentUIBus::post(const AgentUIEvent &event)
{
    AgentUIEvent stamped = event;
    if (stamped.timestamp == 0)
        stamped.timestamp = QDateTime::currentMSecsSinceEpoch();

    // Track current mission
    if (stamped.type == AgentUIEventType::MissionCreated) {
        m_currentMissionId = stamped.missionId;
        m_missionOrder.append(stamped.missionId);
        // Evict oldest missions beyond cap
        while (m_missionOrder.size() > kMaxMissions) {
            const QString oldest = m_missionOrder.takeFirst();
            m_history.remove(oldest);
        }
    }

    // Store in history
    auto &history = m_history[stamped.missionId];
    history.append(stamped);
    if (history.size() > kMaxEventsPerMission)
        history.remove(0, history.size() - kMaxEventsPerMission);

    // Dispatch to all renderers
    for (auto *r : m_renderers) {
        if (r)
            r->handleEvent(stamped);
    }

    Q_EMIT eventDispatched(stamped);
}

QString AgentUIBus::currentMissionId() const
{
    return m_currentMissionId;
}

void AgentUIBus::clearMission(const QString &missionId)
{
    const QString id = missionId.isEmpty() ? m_currentMissionId : missionId;
    m_history.remove(id);
    m_missionOrder.removeAll(id);
    if (id == m_currentMissionId)
        m_currentMissionId.clear();

    for (auto *r : m_renderers) {
        if (r)
            r->clearDashboard();
    }
}

QList<AgentUIEvent> AgentUIBus::eventsForMission(const QString &missionId) const
{
    return m_history.value(missionId);
}
