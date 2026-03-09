#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>
#include <unordered_map>

#include "../aiinterface.h"

// ── Tool permission levels ────────────────────────────────────────────────────

enum class AgentToolPermission
{
    ReadOnly,    // read_file, search, list_symbols, get_diagnostics
    SafeMutate,  // propose_patch, replace_selection, create_file
    Dangerous,   // run_command, delete_file, rename_files
};

// ── Tool specification ────────────────────────────────────────────────────────
// Describes a tool's contract: name, schema, permissions, limits.

struct ToolSpec
{
    QString        name;
    QString        description;
    QJsonObject    inputSchema;   // JSON Schema for args
    QJsonObject    outputSchema;  // JSON Schema for result
    AgentToolPermission permission = AgentToolPermission::ReadOnly;
    int            timeoutMs  = 30000;
    bool           cancellable = true;
};

// ── Tool execution result ─────────────────────────────────────────────────────

struct ToolExecResult
{
    bool        ok = false;
    QJsonObject data;          // structured result
    QString     textContent;   // human-readable summary (sent back to model)
    QString     error;
};

// ── ITool — abstract tool interface ───────────────────────────────────────────
//
// Every agent tool implements this. Tools are registered in ToolRegistry.
//
// Lifecycle:
//   1. Agent controller receives tool_call from model
//   2. Controller looks up tool by name in ToolRegistry
//   3. Controller validates args against spec().inputSchema
//   4. Controller calls invoke()
//   5. Result is formatted and fed back to model as tool response
//
// Tools receive IDE services through the ServiceRegistry, which they
// access via the constructor. Tools never talk to the model directly.

class ITool
{
public:
    virtual ~ITool() = default;

    virtual ToolSpec spec() const = 0;
    virtual ToolExecResult invoke(const QJsonObject &args) = 0;

    // Optional: cancel a running tool invocation
    virtual void cancel() {}
};

// ── ToolRegistry ──────────────────────────────────────────────────────────────
//
// Central registry for all available tools. Both IDE core tools and
// plugin-provided tools register here. The agent controller queries this
// to build the tool list for model requests and to dispatch tool calls.
//
// Access via ServiceRegistry: registry->service("toolRegistry")

class ToolRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ToolRegistry(QObject *parent = nullptr) : QObject(parent) {}

    // ── Registration ──────────────────────────────────────────────────────

    /// Register a tool. ToolRegistry takes ownership via unique_ptr.
    ~ToolRegistry() override = default;

    void registerTool(std::unique_ptr<ITool> tool)
    {
        if (!tool) return;
        const QString name = tool->spec().name;
        m_tools[name] = std::move(tool);
    }

    /// Unregister (and destroy) a tool by name.
    void removeTool(const QString &name)
    {
        m_tools.erase(name);
    }

    // ── Lookup ────────────────────────────────────────────────────────────

    ITool *tool(const QString &name) const
    {
        auto it = m_tools.find(name);
        return it != m_tools.end() ? it->second.get() : nullptr;
    }

    bool hasTool(const QString &name) const
    {
        return m_tools.find(name) != m_tools.end();
    }

    QStringList toolNames() const
    {
        QStringList names;
        names.reserve(static_cast<int>(m_tools.size()));
        for (const auto &[key, _] : m_tools)
            names.append(key);
        return names;
    }

    // ── Build ToolDefinition list for model requests ──────────────────────

    QList<ToolDefinition> allDefinitions() const
    {
        QList<ToolDefinition> defs;
        for (const auto &[name, t] : m_tools)
            defs.append(toolToDefinition(t.get()));
        return defs;
    }

    /// Definitions filtered by permission level
    QList<ToolDefinition> definitions(AgentToolPermission maxLevel) const
    {
        QList<ToolDefinition> defs;
        for (const auto &[name, t] : m_tools) {
            if (m_disabled.contains(name))
                continue;
            if (static_cast<int>(t->spec().permission) <= static_cast<int>(maxLevel))
                defs.append(toolToDefinition(t.get()));
        }
        return defs;
    }

    /// Enable/disable individual tools
    void setDisabledTools(const QSet<QString> &names) { m_disabled = names; }
    QSet<QString> disabledTools() const { return m_disabled; }
    bool isToolEnabled(const QString &name) const { return !m_disabled.contains(name); }

private:
    static ToolDefinition toolToDefinition(ITool *tool)
    {
        const ToolSpec &s = tool->spec();
        ToolDefinition td;
        td.name        = s.name;
        td.description = s.description;
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

    struct QStringHash {
        size_t operator()(const QString &s) const { return qHash(s); }
    };
    std::unordered_map<QString, std::unique_ptr<ITool>, QStringHash> m_tools;
    QSet<QString> m_disabled;
};
