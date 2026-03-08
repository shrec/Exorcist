#pragma once

#include <QObject>
#include <QString>
#include <QUuid>
#include <QVector>

#include "sessionstore.h"

// ── ChatSessionService ────────────────────────────────────────────────────────
//
// API-level session management facade.  Sits above SessionStore (persistence
// backend) and provides the contract that chat UI and import/export surfaces
// depend on.  SessionStore handles JSONL I/O; this service owns the session
// lifecycle and metadata.
//
// Access via ServiceRegistry: registry->service("chatSessionService")

class ChatSessionService : public QObject
{
    Q_OBJECT

public:
    explicit ChatSessionService(SessionStore *store, QObject *parent = nullptr);

    // ── Session lifecycle ─────────────────────────────────────────────────

    /// Create a new session, returns the generated session ID.
    QString createSession(const QString &providerId,
                          const QString &model,
                          bool agentMode);

    /// End the current session (marks end in the store).
    void endSession(const QString &sessionId);

    /// Delete a session permanently.
    void deleteSession(const QString &sessionId);

    /// Rename / set title for a session.
    void renameSession(const QString &sessionId, const QString &newTitle);

    // ── Query ─────────────────────────────────────────────────────────────

    /// List recent sessions (most-recent first).
    QVector<SessionStore::Summary> recentSessions(int max = 20) const;

    /// Search sessions by text query.
    QStringList searchSessions(const QString &query, int max = 20) const;

    /// Load full session transcript.
    SessionStore::ChatSession loadSession(const QString &sessionId) const;

    /// Load the most recent session.
    SessionStore::ChatSession loadLastSession() const;

    // ── Recording (delegates to store) ────────────────────────────────────

    void recordUserMessage(const QString &sessionId, const QString &content);
    void recordAssistantMessage(const QString &sessionId, const QString &text);
    void recordToolCall(const QString &sessionId, const QString &name,
                        const QJsonObject &args, bool ok,
                        const QString &result);
    void recordError(const QString &sessionId, const QString &message);
    void setSessionTitle(const QString &sessionId, const QString &title);

    // ── Import ────────────────────────────────────────────────────────────

    /// Import a session from a list of {role, text} pairs (e.g. remote
    /// provider transcript).  Returns the new session ID.
    QString importSession(const QString &providerId,
                          const QVector<QPair<QString, QString>> &messages);

    // ── Accessors ─────────────────────────────────────────────────────────

    SessionStore *store() const { return m_store; }

signals:
    void sessionCreated(const QString &sessionId);
    void sessionDeleted(const QString &sessionId);
    void sessionRenamed(const QString &sessionId, const QString &newTitle);

private:
    SessionStore *m_store;
};
