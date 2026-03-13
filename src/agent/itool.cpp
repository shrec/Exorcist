#include "itool.h"

#include <QJsonArray>

// ── ToolRegistry ──────────────────────────────────────────────────────────

ToolRegistry::ToolRegistry(QObject *parent) : QObject(parent) {}

void ToolRegistry::registerTool(std::unique_ptr<ITool> tool)
{
    if (!tool) return;
    const QString name = tool->spec().name;
    m_tools[name] = std::move(tool);
}

void ToolRegistry::removeTool(const QString &name)
{
    m_tools.erase(name);
}

ITool *ToolRegistry::tool(const QString &name) const
{
    auto it = m_tools.find(name);
    return it != m_tools.end() ? it->second.get() : nullptr;
}

bool ToolRegistry::hasTool(const QString &name) const
{
    return m_tools.find(name) != m_tools.end();
}

QStringList ToolRegistry::toolNames() const
{
    QStringList names;
    names.reserve(static_cast<int>(m_tools.size()));
    for (const auto &[key, _] : m_tools)
        names.append(key);
    return names;
}

QStringList ToolRegistry::availableToolNames() const
{
    QStringList names;
    for (const auto &[name, t] : m_tools) {
        if (m_disabled.contains(name))
            continue;
        const ToolSpec &sp = t->spec();
        if (!sp.contexts.isEmpty() && !m_activeContexts.isEmpty()) {
            bool match = false;
            for (const QString &ctx : sp.contexts) {
                if (m_activeContexts.contains(ctx)) { match = true; break; }
            }
            if (!match) continue;
        }
        names.append(name);
    }
    return names;
}

QList<ToolDefinition> ToolRegistry::allDefinitions() const
{
    QList<ToolDefinition> defs;
    for (const auto &[name, t] : m_tools)
        defs.append(toolToDefinition(t.get()));
    return defs;
}

QList<ToolDefinition> ToolRegistry::definitions(AgentToolPermission maxLevel) const
{
    QList<ToolDefinition> defs;
    for (const auto &[name, t] : m_tools) {
        if (m_disabled.contains(name))
            continue;
        const ToolSpec &sp = t->spec();
        if (static_cast<int>(sp.permission) > static_cast<int>(maxLevel))
            continue;
        if (!sp.contexts.isEmpty() && !m_activeContexts.isEmpty()) {
            bool match = false;
            for (const QString &ctx : sp.contexts) {
                if (m_activeContexts.contains(ctx)) { match = true; break; }
            }
            if (!match) continue;
        }
        defs.append(toolToDefinition(t.get()));
    }
    return defs;
}

ToolDefinition ToolRegistry::toolToDefinition(ITool *tool)
{
    const ToolSpec &s = tool->spec();
    ToolDefinition td;
    td.name        = s.name;
    td.description = s.description;
    td.inputSchema = s.inputSchema;
    const QJsonObject props =
        s.inputSchema[QLatin1String("properties")].toObject();
    const QJsonArray req =
        s.inputSchema[QLatin1String("required")].toArray();
    for (auto pit = props.begin(); pit != props.end(); ++pit) {
        ToolParameter tp;
        tp.name        = pit.key();
        tp.type        = pit.value().toObject()[QLatin1String("type")].toString();
        tp.description = pit.value().toObject()[QLatin1String("description")].toString();
        tp.required    = req.contains(pit.key());
        td.parameters.append(tp);
    }
    return td;
}
