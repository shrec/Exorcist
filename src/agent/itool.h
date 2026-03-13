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

    // Context filtering: when non-empty, this tool is only available
    // when the workspace contains files matching one of these contexts.
    // Examples: {"python"}, {"cpp", "c"}, {"web", "typescript"}.
    // Empty list = universal tool, always available.
    QStringList    contexts;
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
    explicit ToolRegistry(QObject *parent = nullptr);
    ~ToolRegistry() override = default;

    // ── Registration ──────────────────────────────────────────────────────

    void registerTool(std::unique_ptr<ITool> tool);
    void removeTool(const QString &name);

    // ── Lookup ────────────────────────────────────────────────────────────

    ITool *tool(const QString &name) const;
    bool hasTool(const QString &name) const;
    QStringList toolNames() const;

    // ── Build ToolDefinition list for model requests ──────────────────────

    QList<ToolDefinition> allDefinitions() const;

    /// Definitions filtered by permission level and active contexts
    QList<ToolDefinition> definitions(AgentToolPermission maxLevel) const;

    // ── Context management ────────────────────────────────────────────────

    void setActiveContexts(const QSet<QString> &contexts) { m_activeContexts = contexts; }
    QSet<QString> activeContexts() const { return m_activeContexts; }

    void setDisabledTools(const QSet<QString> &names) { m_disabled = names; }
    QSet<QString> disabledTools() const { return m_disabled; }
    bool isToolEnabled(const QString &name) const { return !m_disabled.contains(name); }

private:
    static ToolDefinition toolToDefinition(ITool *tool);

    struct QStringHash {
        size_t operator()(const QString &s) const { return qHash(s); }
    };
    std::unordered_map<QString, std::unique_ptr<ITool>, QStringHash> m_tools;
    QSet<QString> m_disabled;
    QSet<QString> m_activeContexts;
};
