#include "chatsessionservice.h"

#include <QDir>
#include <QUuid>

ChatSessionService::ChatSessionService(SessionStore *store, QObject *parent)
    : QObject(parent),
      m_store(store)
{
}

// ── Session lifecycle ─────────────────────────────────────────────────────────

QString ChatSessionService::createSession(const QString &providerId,
                                          const QString &model,
                                          bool agentMode)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_store->startSession(id, providerId, model, agentMode);
    emit sessionCreated(id);
    return id;
}

void ChatSessionService::endSession(const QString &sessionId)
{
    m_store->recordSessionEnd(sessionId);
}

void ChatSessionService::deleteSession(const QString &sessionId)
{
    // SessionStore::deleteSession expects a file path; construct it.
    const QString dir = m_store->property("m_dir").toString();
    // Use the store's listSessionFiles to find the matching file.
    const QStringList files = m_store->listSessionFiles(500);
    for (const QString &f : files) {
        if (f.contains(sessionId)) {
            m_store->deleteSession(f);
            emit sessionDeleted(sessionId);
            return;
        }
    }
}

void ChatSessionService::renameSession(const QString &sessionId,
                                       const QString &newTitle)
{
    m_store->setSessionTitle(sessionId, newTitle);
    emit sessionRenamed(sessionId, newTitle);
}

// ── Query ─────────────────────────────────────────────────────────────────────

QVector<SessionStore::Summary> ChatSessionService::recentSessions(int max) const
{
    return m_store->recentSessions(max);
}

QStringList ChatSessionService::searchSessions(const QString &query, int max) const
{
    return m_store->searchSessions(query, max);
}

SessionStore::ChatSession ChatSessionService::loadSession(
    const QString &sessionId) const
{
    const QStringList files = m_store->listSessionFiles(500);
    for (const QString &f : files) {
        if (f.contains(sessionId))
            return m_store->loadSession(f);
    }
    return {};
}

SessionStore::ChatSession ChatSessionService::loadLastSession() const
{
    return m_store->loadLastSession();
}

// ── Recording ─────────────────────────────────────────────────────────────────

void ChatSessionService::recordUserMessage(const QString &sessionId,
                                           const QString &content)
{
    m_store->recordUserMessage(sessionId, content);
}

void ChatSessionService::recordAssistantMessage(const QString &sessionId,
                                                const QString &text)
{
    m_store->recordAssistantMessage(sessionId, text);
}

void ChatSessionService::recordToolCall(const QString &sessionId,
                                        const QString &name,
                                        const QJsonObject &args, bool ok,
                                        const QString &result)
{
    m_store->recordToolCall(sessionId, name, args, ok, result);
}

void ChatSessionService::recordError(const QString &sessionId,
                                     const QString &message)
{
    m_store->appendError(sessionId, message);
}

void ChatSessionService::setSessionTitle(const QString &sessionId,
                                         const QString &title)
{
    m_store->setSessionTitle(sessionId, title);
}

// ── Import ────────────────────────────────────────────────────────────────────

QString ChatSessionService::importSession(
    const QString &providerId,
    const QVector<QPair<QString, QString>> &messages)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_store->startSession(id, providerId, QStringLiteral("imported"), false);

    for (const auto &msg : messages) {
        if (msg.first == QLatin1String("user"))
            m_store->recordUserMessage(id, msg.second);
        else if (msg.first == QLatin1String("assistant"))
            m_store->recordAssistantMessage(id, msg.second);
    }

    m_store->recordSessionEnd(id);
    emit sessionCreated(id);
    return id;
}
