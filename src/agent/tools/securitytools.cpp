#include "securitytools.h"

#include <QJsonArray>

// ── StoreSecretTool ───────────────────────────────────────────────────────────

StoreSecretTool::StoreSecretTool(SecretStorer storer)
    : m_storer(std::move(storer))
{}

ToolSpec StoreSecretTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("store_secret");
    s.description = QStringLiteral(
        "Store a secret (API key, token, password) in the OS secure credential store.\n"
        "On Windows this uses Windows Credential Manager (DPAPI-encrypted).\n"
        "The secret is identified by a service name (e.g. 'openai_api_key', 'github_token').\n"
        "Use this before storing any sensitive values — never write secrets to files.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("service"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Unique service name / key identifier (e.g. 'openai_api_key').")}
            }},
            {QStringLiteral("value"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The secret value to store.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("service"), QStringLiteral("value")}}
    };
    return s;
}

ToolExecResult StoreSecretTool::invoke(const QJsonObject &args)
{
    const QString service = args[QLatin1String("service")].toString().trimmed();
    const QString value   = args[QLatin1String("value")].toString();

    if (service.isEmpty())
        return {false, {}, {}, QStringLiteral("'service' is required.")};
    if (value.isEmpty())
        return {false, {}, {}, QStringLiteral("'value' is required.")};

    const bool ok = m_storer(service, value);
    if (!ok)
        return {false, {}, {},
                QStringLiteral("Failed to store secret '%1'.").arg(service)};

    return {true, {}, QStringLiteral("Secret '%1' stored successfully.").arg(service), {}};
}

// ── GetSecretTool ─────────────────────────────────────────────────────────────

GetSecretTool::GetSecretTool(SecretGetter getter)
    : m_getter(std::move(getter))
{}

ToolSpec GetSecretTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("get_secret");
    s.description = QStringLiteral(
        "Retrieve a secret from the OS secure credential store by service name.\n"
        "Returns the secret value, or empty string if not found.\n"
        "Use this to read API keys, tokens, and passwords stored with store_secret.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("service"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The service name / key identifier to retrieve.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("service")}}
    };
    return s;
}

ToolExecResult GetSecretTool::invoke(const QJsonObject &args)
{
    const QString service = args[QLatin1String("service")].toString().trimmed();
    if (service.isEmpty())
        return {false, {}, {}, QStringLiteral("'service' is required.")};

    const QString value = m_getter(service);
    if (value.isEmpty())
        return {false, {}, {},
                QStringLiteral("Secret '%1' not found.").arg(service)};

    return {true, {}, value, {}};
}

// ── ListSecretsTool ───────────────────────────────────────────────────────────

ListSecretsTool::ListSecretsTool(SecretLister lister)
    : m_lister(std::move(lister))
{}

ToolSpec ListSecretsTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("list_secrets");
    s.description = QStringLiteral(
        "List all service names stored in the OS secure credential store.\n"
        "Returns names only — never returns secret values.\n"
        "Use this to check which API keys or tokens have been stored.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{}},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult ListSecretsTool::invoke(const QJsonObject &)
{
    const QStringList services = m_lister();
    if (services.isEmpty())
        return {true, {}, QStringLiteral("No secrets stored."), {}};

    return {true, {}, services.join(QLatin1Char('\n')), {}};
}

// ── DeleteSecretTool ──────────────────────────────────────────────────────────

DeleteSecretTool::DeleteSecretTool(SecretDeleter deleter)
    : m_deleter(std::move(deleter))
{}

ToolSpec DeleteSecretTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("delete_secret");
    s.description = QStringLiteral(
        "Delete a secret from the OS secure credential store.\n"
        "Returns true if the secret existed and was removed.");
    s.permission  = AgentToolPermission::Dangerous;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("service"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The service name / key identifier to delete.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("service")}}
    };
    return s;
}

ToolExecResult DeleteSecretTool::invoke(const QJsonObject &args)
{
    const QString service = args[QLatin1String("service")].toString().trimmed();
    if (service.isEmpty())
        return {false, {}, {}, QStringLiteral("'service' is required.")};

    const bool ok = m_deleter(service);
    if (!ok)
        return {false, {}, {},
                QStringLiteral("Secret '%1' not found or could not be deleted.").arg(service)};

    return {true, {}, QStringLiteral("Secret '%1' deleted.").arg(service), {}};
}
