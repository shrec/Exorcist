#include "idecommandtool.h"

RunIdeCommandTool::RunIdeCommandTool(CommandExecutor executor,
                                     CommandLister   lister)
    : m_executor(std::move(executor))
    , m_lister(std::move(lister)) {}

ToolSpec RunIdeCommandTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("run_ide_command");
    s.description = QStringLiteral(
        "Execute an IDE command by its ID, or list all available commands. "
        "Commands control IDE actions like opening files, toggling panels, "
        "triggering builds, formatting, and more. "
        "Set action to \"list\" to see all registered command IDs, "
        "or \"execute\" with a commandId to run one.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("action"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("execute"),
                    QStringLiteral("list")
                }},
                {QStringLiteral("description"),
                 QStringLiteral("\"execute\" to run a command, \"list\" to list all available command IDs.")}
            }},
            {QStringLiteral("commandId"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The command ID to execute (e.g. \"workbench.action.save\"). "
                                "Required when action is \"execute\".")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("action")}}
    };
    return s;
}

ToolExecResult RunIdeCommandTool::invoke(const QJsonObject &args)
{
    const QString action = args[QLatin1String("action")].toString();

    if (action == QLatin1String("list")) {
        if (!m_lister)
            return {false, {}, QStringLiteral("Command listing not available."), {}};

        const QStringList commands = m_lister();
        if (commands.isEmpty())
            return {true, {}, QStringLiteral("No commands registered."), {}};

        return {true, {},
                QStringLiteral("%1 available command(s):\n%2")
                    .arg(commands.size())
                    .arg(commands.join(QLatin1Char('\n'))),
                {}};
    }

    if (action == QLatin1String("execute")) {
        const QString cmdId = args[QLatin1String("commandId")].toString().trimmed();
        if (cmdId.isEmpty())
            return {false, {}, QStringLiteral("commandId is required for execute action."), {}};

        if (!m_executor)
            return {false, {}, QStringLiteral("Command execution not available."), {}};

        const bool ok = m_executor(cmdId);
        if (!ok)
            return {false, {},
                    QStringLiteral("Command \"%1\" not found. Use action \"list\" to see available commands.")
                        .arg(cmdId),
                    {}};

        return {true, {},
                QStringLiteral("Command \"%1\" executed successfully.").arg(cmdId),
                {}};
    }

    return {false, {},
            QStringLiteral("Unknown action \"%1\". Use \"execute\" or \"list\".").arg(action),
            {}};
}
