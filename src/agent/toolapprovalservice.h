#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <functional>

#include "itool.h"

// ── ToolApprovalService ───────────────────────────────────────────────────────
//
// Centralised tool-approval policy.  Owns the "always-allowed" set for the
// current session, the maximum permitted permission level, and the UI callback
// that asks the user to approve dangerous operations.
//
// This was previously scattered across AgentController (m_confirmFn,
// m_alwaysAllowedTools, m_maxPermission) and AgentChatPanel (the QMessageBox
// callback).  Pulling it into one service lets every surface and controller
// share the same policy without duplication.
//
// Access via ServiceRegistry: registry->service("toolApprovalService")

class ToolApprovalService : public QObject
{
    Q_OBJECT

public:
    explicit ToolApprovalService(QObject *parent = nullptr);

    /// Possible approval outcomes.
    enum class Decision { Deny, AllowOnce, AllowAlways };
    Q_ENUM(Decision)

    /// Callback type that shows a UI confirmation dialog and returns a
    /// synchronous decision.
    using ConfirmFn = std::function<Decision(const QString &toolName,
                                             const QJsonObject &args,
                                             const QString &description)>;

    // ── Configuration ─────────────────────────────────────────────────────

    /// Set the maximum permission level that can be auto-approved.
    /// Tools whose ToolSpec::permission exceeds this level trigger
    /// the confirm callback.
    void setMaxAutoApproveLevel(AgentToolPermission level);
    AgentToolPermission maxAutoApproveLevel() const { return m_maxAutoLevel; }

    /// Set the UI callback that asks the user for a decision.
    void setConfirmCallback(ConfirmFn fn);

    // ── Session state ─────────────────────────────────────────────────────

    /// Clear per-session "always allow" set (call on new session).
    void clearSessionState();

    /// Check whether a tool is in the session-scoped "always allow" set.
    bool isAlwaysAllowed(const QString &toolName) const;

    // ── Approval check ────────────────────────────────────────────────────

    /// Evaluate whether a tool call is approved.
    ///
    /// 1. If the tool's permission <= maxAutoApproveLevel → AllowOnce
    /// 2. If the tool is in the session "always allow" set → AllowOnce
    /// 3. Otherwise invokes the confirm callback (or Deny if none set).
    ///
    /// On AllowAlways, the tool is added to the session set.
    Decision evaluate(const QString &toolName,
                      AgentToolPermission requiredLevel,
                      const QJsonObject &args,
                      const QString &description);

signals:
    /// Emitted after every approval decision for audit/logging.
    void decisionMade(const QString &toolName,
                      ToolApprovalService::Decision decision);

private:
    AgentToolPermission m_maxAutoLevel = AgentToolPermission::SafeMutate;
    QSet<QString>       m_alwaysAllowed;
    ConfirmFn           m_confirmFn;
};
