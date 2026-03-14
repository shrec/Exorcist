#include "askusertool.h"

AskUserTool::AskUserTool(UserPrompter prompter)
    : m_prompter(std::move(prompter)) {}

ToolSpec AskUserTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("ask_user");
    s.description = QStringLiteral(
        "Ask the user a question and wait for their response. Use this "
        "when you need:\n"
        "  - Clarification on ambiguous requirements\n"
        "  - Confirmation before destructive/irreversible actions\n"
        "  - A choice between multiple valid approaches\n"
        "  - Information you cannot determine from code (API keys, "
        "preferences, business logic)\n\n"
        "Do NOT overuse this — only ask when the answer genuinely affects "
        "your next action. If you can reasonably infer the answer from "
        "context, code, or conventions, do that instead.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 300000; // 5 minutes — user may take time to respond
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("question"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The question to ask. Be specific and concise. "
                                "Include context about why you're asking.")}
            }},
            {QStringLiteral("options"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")}
                }},
                {QStringLiteral("description"),
                 QStringLiteral("Optional list of choices. If provided, user "
                                "picks one. If omitted, user types free text.")}
            }},
            {QStringLiteral("urgency"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("info"),
                    QStringLiteral("warning"),
                    QStringLiteral("critical")
                }},
                {QStringLiteral("description"),
                 QStringLiteral("Urgency level. 'critical' for destructive "
                                "confirmations, 'warning' for important choices, "
                                "'info' for general questions. Default: info.")}
            }},
            {QStringLiteral("allowFreeText"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Allow free text input alongside options. "
                                "Default: true if no options, false if options.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("question")}}
    };
    return s;
}

ToolExecResult AskUserTool::invoke(const QJsonObject &args)
{
    const QString question = args[QLatin1String("question")].toString();
    if (question.isEmpty())
        return {false, {}, {}, QStringLiteral("'question' is required.")};

    QStringList options;
    const QJsonArray optArr = args[QLatin1String("options")].toArray();
    for (const auto &v : optArr)
        options.append(v.toString());

    const bool hasOptions = !options.isEmpty();
    const bool allowFreeText = args[QLatin1String("allowFreeText")].toBool(!hasOptions);

    const UserResponse response = m_prompter(question, options, allowFreeText);

    if (!response.answered)
        return {false, {}, {},
                QStringLiteral("User dismissed the question without answering.")};

    QJsonObject data;
    data[QLatin1String("response")] = response.response;
    if (response.selectedIndex >= 0)
        data[QLatin1String("selectedIndex")] = response.selectedIndex;

    return {true, data,
            QStringLiteral("User responded: %1").arg(response.response), {}};
}
