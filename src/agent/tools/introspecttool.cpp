#include "introspecttool.h"

IntrospectTool::IntrospectTool(QueryHandler handler)
    : m_handler(std::move(handler)) {}

ToolSpec IntrospectTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("introspect");
    s.description = QStringLiteral(
        "Query the running application state. Available queries:\n"
        "- 'services': List all registered services in ServiceRegistry\n"
        "- 'plugins': List loaded plugins with names, versions, errors\n"
        "- 'memory': Current RSS, heap usage, startup profile\n"
        "- 'toolchain': Detected compilers, debuggers, build systems\n"
        "- 'sessions': Active chat sessions and current session info\n"
        "- 'tools': List all registered agent tools with permissions\n"
        "- 'config': Current settings (theme, font, LSP, etc.)");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("query"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("What to query: services, plugins, memory, "
                                "toolchain, sessions, tools, config")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("services"),
                    QStringLiteral("plugins"),
                    QStringLiteral("memory"),
                    QStringLiteral("toolchain"),
                    QStringLiteral("sessions"),
                    QStringLiteral("tools"),
                    QStringLiteral("config")
                }}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}}
    };
    return s;
}

ToolExecResult IntrospectTool::invoke(const QJsonObject &args)
{
    const QString query = args[QLatin1String("query")].toString();
    if (query.isEmpty())
        return {false, {}, {}, QStringLiteral("'query' parameter is required.")};

    const QString result = m_handler(query);
    if (result.isEmpty())
        return {false, {}, {},
                QStringLiteral("Unknown query '%1'. Use: services, plugins, "
                               "memory, toolchain, sessions, tools, config").arg(query)};

    return {true, {}, result, {}};
}
