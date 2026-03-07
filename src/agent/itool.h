#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

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

    /// Register a tool. ToolRegistry takes ownership.
    ~ToolRegistry() override { qDeleteAll(m_tools); }

    void registerTool(ITool *tool)
    {
        if (!tool) return;
        const QString name = tool->spec().name;
        delete m_tools.value(name, nullptr);
        m_tools[name] = tool;
    }

    /// Unregister (and destroy) a tool by name.
    void removeTool(const QString &name)
    {
        delete m_tools.take(name);
    }

    // ── Lookup ────────────────────────────────────────────────────────────

    ITool *tool(const QString &name) const
    {
        return m_tools.value(name, nullptr);
    }

    bool hasTool(const QString &name) const
    {
        return m_tools.contains(name);
    }

    QStringList toolNames() const
    {
        return m_tools.keys();
    }

    // ── Build ToolDefinition list for model requests ──────────────────────

    QList<ToolDefinition> allDefinitions() const
    {
        QList<ToolDefinition> defs;
        for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
            defs.append(toolToDefinition(it.value()));
        }
        return defs;
    }

    /// Definitions filtered by permission level
    QList<ToolDefinition> definitions(AgentToolPermission maxLevel) const
    {
        QList<ToolDefinition> defs;
        for (auto it = m_tools.begin(); it != m_tools.end(); ++it) {
            if (m_disabled.contains(it.key()))
                continue;
            if (static_cast<int>(it.value()->spec().permission) <= static_cast<int>(maxLevel))
                defs.append(toolToDefinition(it.value()));
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

    QHash<QString, ITool *> m_tools;
    QSet<QString>           m_disabled;
};
