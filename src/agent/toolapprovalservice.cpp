#include "toolapprovalservice.h"

ToolApprovalService::ToolApprovalService(QObject *parent)
    : QObject(parent)
{
}

void ToolApprovalService::setMaxAutoApproveLevel(AgentToolPermission level)
{
    m_maxAutoLevel = level;
}

void ToolApprovalService::setConfirmCallback(ConfirmFn fn)
{
    m_confirmFn = std::move(fn);
}

void ToolApprovalService::clearSessionState()
{
    m_alwaysAllowed.clear();
}

bool ToolApprovalService::isAlwaysAllowed(const QString &toolName) const
{
    return m_alwaysAllowed.contains(toolName);
}

ToolApprovalService::Decision
ToolApprovalService::evaluate(const QString &toolName,
                              AgentToolPermission requiredLevel,
                              const QJsonObject &args,
                              const QString &description)
{
    // Auto-approve if within permitted level.
    if (static_cast<int>(requiredLevel) <= static_cast<int>(m_maxAutoLevel)) {
        emit decisionMade(toolName, Decision::AllowOnce);
        return Decision::AllowOnce;
    }

    // Auto-approve if the user already said "always" this session.
    if (m_alwaysAllowed.contains(toolName)) {
        emit decisionMade(toolName, Decision::AllowOnce);
        return Decision::AllowOnce;
    }

    // Ask the user (or deny if no callback).
    Decision d = Decision::Deny;
    if (m_confirmFn)
        d = m_confirmFn(toolName, args, description);

    if (d == Decision::AllowAlways)
        m_alwaysAllowed.insert(toolName);

    emit decisionMade(toolName, d);
    return d;
}
