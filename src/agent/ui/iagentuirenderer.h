#pragma once

#include "agentuievent.h"

// ── IAgentUIRenderer — abstract renderer for agent dashboard events ──────────
//
// Any panel that wants to display structured agent UI events implements this.
// The AgentUIBus dispatches events to all registered renderers.
//
// The primary implementation is AgentDashboardPanel (Ultralight-based),
// but the interface allows QWidget-based fallbacks or headless renderers
// (e.g., for logging/testing).

class IAgentUIRenderer
{
public:
    virtual ~IAgentUIRenderer() = default;

    /// Handle a single UI event. Called on the main thread.
    virtual void handleEvent(const AgentUIEvent &event) = 0;

    /// Clear all state — called when a new mission replaces the current one.
    virtual void clearDashboard() = 0;

    /// Whether this renderer is currently visible/active.
    virtual bool isActive() const = 0;
};
